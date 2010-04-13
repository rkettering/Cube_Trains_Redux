/*
   Copyright (C) 2007 by David White <dave@whitevine.net>
   Part of the Silver Tree Project

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 or later.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include <GL/gl.h>
#include <GL/glu.h>
#include <pthread.h>

#include "asserts.hpp"
#include "concurrent_cache.hpp"
#include "foreach.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "surface_cache.hpp"
#include "surface_formula.hpp"
#include "texture.hpp"
#include "thread.hpp"
#include "unit_test.hpp"
#include <map>
#include <set>
#include <iostream>
#include <cstring>

namespace graphics
{

pthread_t graphics_thread_id;
surface scale_surface(surface input);

namespace {
	std::set<texture*>& texture_registry() {
		static std::set<texture*>* reg = new std::set<texture*>;
		return *reg;
	}

	threading::mutex& texture_registry_mutex() {
		static threading::mutex* m = new threading::mutex;
		return *m;
	}

	void add_texture_to_registry(texture* t) {
// TODO: Currently the registry is disabled for performance reasons.
//		threading::lock lk(texture_registry_mutex());
//		texture_registry().insert(t);
	}

	void remove_texture_from_registry(texture* t) {
//		threading::lock lk(texture_registry_mutex());
//		texture_registry().erase(t);
	}

	typedef concurrent_cache<std::string,graphics::texture> texture_map;
	texture_map& texture_cache() {
		static texture_map cache;
		return cache;
	}
	typedef concurrent_cache<std::pair<std::string,std::string>,graphics::texture> algorithm_texture_map;
	algorithm_texture_map& algorithm_texture_cache() {
		static algorithm_texture_map cache;
		return cache;
	}

	const size_t TextureBufSize = 128;
	GLuint texture_buf[TextureBufSize];
	size_t texture_buf_pos = TextureBufSize;
	std::vector<GLuint> avail_textures;
	bool graphics_initialized = false;

	GLuint current_texture = 0;

	GLuint get_texture_id() {
		if(!avail_textures.empty()) {
			const GLuint res = avail_textures.back();
			avail_textures.pop_back();
			return res;
		}

		if(texture_buf_pos == TextureBufSize) {
			if(!pthread_equal(graphics_thread_id, pthread_self())) {
				//we are in a worker thread so we can't make an OpenGL
				//call. Throw an exception.
				std::cerr << "CANNOT ALLOCATE TEXTURE ID's in WORKER THREAD\n";
				throw texture::worker_thread_error();
			}

			glGenTextures(TextureBufSize, texture_buf);
			texture_buf_pos = 0;
		}

		return texture_buf[texture_buf_pos++];
	}

	bool npot_allowed = true;
	GLfloat width_multiplier = -1.0;
	GLfloat height_multiplier = -1.0;

	unsigned int next_power_of_2(unsigned int n)
	{
		--n;
		n = n|(n >> 1);
		n = n|(n >> 2);
		n = n|(n >> 4);
		n = n|(n >> 8);
		n = n|(n >> 16);
		++n;
		return n;
	}

	bool is_npot_allowed()
    {
	static bool once = false;
	static bool npot = true;
	if (once) return npot;
	once = true;

	if(preferences::force_no_npot_textures()) {
		npot = false;
		return false;
	}

	const char *supported = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
	const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
	const char *vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

	std::cerr << "OpenGL version: " << version << "\n";
	std::cerr << "OpenGL vendor: " << vendor << "\n";
	std::cerr << "OpenGL extensions: " << supported << "\n";

	// OpenGL >= 2.0 drivers must support NPOT textures
	bool version_2 = (version[0] >= '2');
	npot = version_2;
	// directly test for NPOT extension
	if (std::strstr(supported, "GL_ARB_texture_non_power_of_two")) npot = true;

	if (npot) {
		// Use some heuristic to make sure it is HW accelerated. Might need some
		// more work.
		if (std::strstr(vendor, "NVIDIA Corporation")) {
			if (!std::strstr(supported, "NV_fragment_program2") ||
				!std::strstr(supported, "NV_vertex_program3")) {
					npot = false;
			}
		} else if (std::strstr(vendor, "ATI Technologies")) {
					// TODO: Investigation note: my ATI card works fine for npot textures as long
					// as mipmapping is enabled. otherwise it runs slowly. Work out why. --David
				//if (!std::strstr(supported, "GL_ARB_texture_non_power_of_two"))
					npot = false;
		} else if(std::strstr(vendor, "Apple Computer") || std::strstr(vendor, "Imagination Technologies")) {
			if (!std::strstr(supported, "GL_ARB_texture_non_power_of_two")) {
				npot = false;
			}
		}
	}
	if(!npot) {
		std::cerr << "Using only pot textures\n";
	}
	return npot;
    }

	std::string mipmap_type_to_string(GLenum type) {
		switch(type) {
		case GL_NEAREST:
			return "N";
		case GL_LINEAR:
			return "L";
		case GL_NEAREST_MIPMAP_NEAREST:
			return "NN";
		case GL_NEAREST_MIPMAP_LINEAR:
			return "NL";
		case GL_LINEAR_MIPMAP_NEAREST:
			return "LN";
		case GL_LINEAR_MIPMAP_LINEAR:
			return "LL";
		default:
			return "??";
		}
	}
}

texture::manager::manager() {
	assert(!graphics_initialized);

	width_multiplier = 1.0;
	height_multiplier = 1.0;

	graphics_initialized = true;

	graphics_thread_id = pthread_self();
}

texture::manager::~manager() {
	graphics_initialized = false;
}

void texture::clear_textures()
{
	std::cerr << "TEXTURES LOADING...\n";
	texture_map::lock lck(texture_cache());
	for(texture_map::map_type::const_iterator i = lck.map().begin(); i != lck.map().end(); ++i) {
		if(!i->second.id_) {
			continue;
		}

		std::cerr << "TEXTURE: '" << i->first << "': " << (i->second.id_->init() ? "INIT" : "UNINIT") << "\n";
	}

	std::cerr << "DONE TEXTURES LOADING\n";
/*
	//go through all the textures and clear out the ID's. We only want to
	//re-initialize each shared ID once.
	threading::lock lk(texture_registry_mutex());
	for(std::set<texture*>::iterator i = texture_registry().begin(); i != texture_registry().end(); ++i) {
		texture& t = **i;
		if(t.id_) {
			t.id_->destroy();
		}
	}

	//go through and initialize anyone's ID which hasn't been initialized
	//already.
	for(std::set<texture*>::iterator i = texture_registry().begin(); i != texture_registry().end(); ++i) {
		texture& t = **i;
		if(t.id_ && t.id_->s.get() == NULL) {
			t.initialize();
		}
	}
	*/
}

texture::texture() : width_(0), height_(0)
{
	add_texture_to_registry(this);
}

texture::texture(const key& surfs)
   : width_(0), height_(0), ratio_w_(1.0), ratio_h_(1.0)
{
	add_texture_to_registry(this);
	initialize(surfs);
}

texture::texture(const texture& t)
  : id_(t.id_), width_(t.width_), height_(t.height_),
   ratio_w_(t.ratio_w_), ratio_h_(t.ratio_h_),
   alpha_map_(t.alpha_map_)
{
	add_texture_to_registry(this);
}

texture::~texture()
{
	remove_texture_from_registry(this);
}

//this function is designed to convert a 24bpp surface to a 32bpp one, adding
//an alpha channel. The dest surface may be larger than the source surface,
//in which case it will put it in the upper-left corner. This is much faster
//than using SDL blitting functions.
void add_alpha_channel_to_surface(uint8_t* dst_ptr, const uint8_t* src_ptr, size_t dst_w, size_t src_w, size_t src_h, size_t src_pitch)
{
	ASSERT_GE(dst_w, src_w);

	for(size_t y = 0; y < src_h; ++y) {
		uint8_t* dst = dst_ptr + y*dst_w*4;
		const uint8_t* src = src_ptr + y*src_pitch;
		for(size_t x = 0; x < src_w; ++x) {
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = 0xFF;
		}

		dst += (dst_w - src_w)*4;
	}
}

void set_alpha_for_transparent_colors_in_rgba_surface(SDL_Surface* s)
{
	const int npixels = s->w*s->h;
	for(int n = 0; n != npixels; ++n) {
		//we use a color in our sprite sheets to indicate transparency, rather than an alpha channel
		static const unsigned char AlphaPixel[] = {0x6f, 0x6d, 0x51}; //the background color, brown
		static const unsigned char AlphaPixel2[] = {0xf9, 0x30, 0x3d}; //the border color, red
		unsigned char* pixel = reinterpret_cast<unsigned char*>(s->pixels) + n*4;

		if(pixel[0] == AlphaPixel[0] && pixel[1] == AlphaPixel[1] && pixel[2] == AlphaPixel[2] ||
		   pixel[0] == AlphaPixel2[0] && pixel[1] == AlphaPixel2[1] && pixel[2] == AlphaPixel2[2]) {
			pixel[3] = 0;
		}
	}
}

void texture::initialize(const key& k)
{
	assert(graphics_initialized);
	if(k.empty() ||
	   std::find(k.begin(),k.end(),surface()) != k.end()) {
		return;
	}

	npot_allowed = is_npot_allowed();

	width_ = k.front()->w;
	height_ = k.front()->h;
	alpha_map_.reset(new std::vector<bool>(width_*height_));

	unsigned int surf_width = width_;
	unsigned int surf_height = height_;
	if(!npot_allowed) {
		surf_width = next_power_of_2(surf_width);
		surf_height = next_power_of_2(surf_height);
//		surf_width = surf_height =
//		   std::max(next_power_of_2(surf_width),
//		            next_power_of_2(surf_height));
		ratio_w_ = GLfloat(width_)/GLfloat(surf_width);
		ratio_h_ = GLfloat(height_)/GLfloat(surf_height);
	}

	surface s(SDL_CreateRGBSurface(SDL_SWSURFACE,surf_width,surf_height,32,SURFACE_MASK));
	if(k.size() == 1 && k.front()->format->Rmask == 0xFF && k.front()->format->Gmask == 0xFF00 && k.front()->format->Bmask == 0xFF0000 && k.front()->format->Amask == 0) {
		add_alpha_channel_to_surface((uint8_t*)s->pixels, (uint8_t*)k.front()->pixels, s->w, k.front()->w, k.front()->h, k.front()->pitch);
	} else if(k.size() == 1 && k.front()->format->Rmask == 0xFF00 && k.front()->format->Gmask == 0xFF0000 && k.front()->format->Bmask == 0xFF000000 && k.front()->format->Amask == 0xFF) {
		//alpha channel already exists, so no conversion necessary.
		s = k.front();
	} else {
		for(key::const_iterator i = k.begin(); i != k.end(); ++i) {
			if(i == k.begin()) {
				SDL_SetAlpha(i->get(), 0, SDL_ALPHA_OPAQUE);
			} else {
				SDL_SetAlpha(i->get(), SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
			}

			SDL_BlitSurface(i->get(),NULL,s.get(),NULL);
		}
	}

	set_alpha_for_transparent_colors_in_rgba_surface(s.get());
	const int npixels = s->w*s->h;
	for(int n = 0; n != npixels; ++n) {
		const unsigned char* pixel = reinterpret_cast<const unsigned char*>(s->pixels) + n*4;
		if(pixel[3] == 0) {
			const int x = n%s->w;
			const int y = n/s->w;
			if(x < width_ && y < height_) {
				(*alpha_map_)[y*width_ + x] = true;
			}
		}
	}

	if(!id_) {
		id_.reset(new ID);
	}

	id_->s = s;

	current_texture = 0;
}

int next_pot (int n)
{
	int num = 1;
	while (num < n)
	{
		num *= 2;
	}
	return num;
}

namespace {
threading::mutex id_to_build_mutex;
}

GLuint texture::get_id() const
{
	if(!valid()) {
		return 0;
	}

	if(id_->init() == false) {
		id_->id = get_texture_id();
		if(preferences::use_pretty_scaling()) {
			id_->s = scale_surface(id_->s);
		}

		if(!pthread_equal(graphics_thread_id, pthread_self())) {
			threading::lock lck(id_to_build_mutex);
			id_to_build_.push_back(id_);
		} else {
			id_->build_id();
		}
	}

	return id_->id;
}

void texture::build_textures_from_worker_threads()
{
	ASSERT_LOG(pthread_equal(pthread_self(), graphics_thread_id), "CALLED build_textures_from_worker_threads from thread other than the main one");
	threading::lock lck(id_to_build_mutex);
	foreach(boost::shared_ptr<ID> id, id_to_build_) {
		id->build_id();
	}

	id_to_build_.clear();
}

void texture::set_current_texture(GLuint id)
{
	if(!id || current_texture == id) {
		return;
	}

	glBindTexture(GL_TEXTURE_2D,id);
	current_texture = id;
}

void texture::set_as_current_texture() const
{
	width_multiplier = ratio_w_;
	height_multiplier = ratio_h_;

	const GLuint id = get_id();
	if(!id || current_texture == id) {
		return;
	}

	current_texture = id;

	glBindTexture(GL_TEXTURE_2D,id);
	//std::cerr << gluErrorString(glGetError()) << "~set_as_current_texture~\n";
}

texture texture::get(const std::string& str)
{
	texture result = texture_cache().get(str);
	if(!result.valid()) {
		key surfs;
		surfs.push_back(surface_cache::get_no_cache(str));
		result = texture(surfs);
		texture_cache().put(str, result);
		std::cerr << (next_power_of_2(result.width())*next_power_of_2(result.height())*2)/1024 << "KB TEXTURE " << str << ": " << result.width() << "x" << result.height() << "\n";
	}

	return result;
}

texture texture::get(const std::string& str, const std::string& algorithm)
{
	if(algorithm.empty()) {
		return get(str);
	}

	std::pair<std::string,std::string> k(str, algorithm);
	texture result = algorithm_texture_cache().get(k);
	if(!result.valid()) {
		key surfs;
		surfs.push_back(get_surface_formula(surface_cache::get_no_cache(str), algorithm));
		result = texture(surfs);
		algorithm_texture_cache().put(k, result);
	}

	return result;
}

texture texture::get_no_cache(const key& surfs)
{
	return texture(surfs);
}

texture texture::get_no_cache(const surface& surf)
{
	return texture(key(1,surf));
}

GLfloat texture::get_coord_x(GLfloat x)
{
	return npot_allowed ? x : x*width_multiplier;
}

GLfloat texture::get_coord_y(GLfloat y)
{
	return npot_allowed ? y : y*height_multiplier;
}

GLfloat texture::translate_coord_x(GLfloat x) const
{
	return npot_allowed ? x : x*ratio_w_;
}

GLfloat texture::translate_coord_y(GLfloat y) const
{
	return npot_allowed ? y : y*ratio_h_;
}

void texture::clear_cache()
{
	texture_cache().clear();
}

const unsigned char* texture::color_at(int x, int y) const
{
	if(!id_ || !id_->s) {
		return NULL;
	}

	const unsigned char* pixels = reinterpret_cast<const unsigned char*>(id_->s->pixels);
	return pixels + (y*id_->s->w + x)*id_->s->format->BytesPerPixel;
}

texture::ID::~ID()
{
	destroy();
}

namespace {

inline unsigned int round_8bits_to_4bits(unsigned int n) {
	n = n&0xFF;
	const unsigned int pattern = n&0xF0;
	if(n >= 0x80) {
		const unsigned int high = pattern | (pattern >> 4);
		const unsigned int low = (pattern-0x10) | ((pattern-0x10) >> 4);
		if(abs(n - low) < abs(high - n)) {
			return low;
		} else {
			return high;
		}
	} else {
		const unsigned int low = pattern | (pattern >> 4);
		const unsigned int high = (pattern+0x10) | ((pattern+0x10) >> 4);
		if(abs(n - low) < abs(high - n)) {
			return low;
		} else {
			return high;
		}
	}
}

}

void texture::ID::build_id()
{
	glBindTexture(GL_TEXTURE_2D,id);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


	if(preferences::use_16bpp_textures()) {
		std::vector<GLushort> buf(s->w*s->h);
		const GLuint* src = reinterpret_cast<const GLuint*>(s->pixels);
		GLushort* dst = &*buf.begin();
		for(int n = 0; n != s->w*s->h; ++n) {
			const GLuint p =
			  round_8bits_to_4bits(*src >> 24) << 24 |
			  round_8bits_to_4bits(*src >> 16) << 16 |
			  round_8bits_to_4bits(*src >> 8) << 8 |
			  round_8bits_to_4bits(*src >> 0) << 0;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			*dst = (((p >> 28)&0xF) << 12) |
			       (((p >> 20)&0xF) << 8) |
			       (((p >> 12)&0xF) << 4) |
			       (((p >> 4)&0xF) << 0);
#else
			*dst = (((p >> 28)&0xF) << 0) |
			       (((p >> 20)&0xF) << 4) |
			       (((p >> 12)&0xF) << 8) |
			       (((p >> 4)&0xF) << 12);
#endif
			++dst;
			++src;
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s->w, s->h, 0, GL_RGBA,
		             GL_UNSIGNED_SHORT_4_4_4_4, &buf[0]);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s->w, s->h, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, s->pixels);
	}

	//free the surface.
	if(!preferences::compiling_tiles) {
		std::cerr << "RELEASING SURFACE... " << s.get()->refcount << "\n";
		s = surface();
	}
}

void texture::ID::destroy()
{
	if(graphics_initialized && init()) {
		avail_textures.push_back(id);
	}

	id = GLuint(-1);
	s = surface();
}

std::vector<boost::shared_ptr<texture::ID> > texture::id_to_build_;

}

BENCHMARK(texture_copy_ctor)
{
	graphics::texture t(graphics::texture::get("characters/frogatto-spritesheet1.png"));
	BENCHMARK_LOOP {
		graphics::texture t2(t);
	}
}
