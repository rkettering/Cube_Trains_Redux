#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL/SDL.h>
#include <algorithm>
#include <cmath>
#include <iostream>

#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>

#include "character_type.hpp"
#include "custom_object_type.hpp"
#include "draw_scene.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "foreach.hpp"
#include "item.hpp"
#include "joystick.hpp"
#include "key.hpp"
#include "level.hpp"
#include "level_object.hpp"
#include "load_level.hpp"
#include "message_dialog.hpp"
#include "prop.hpp"
#include "raster.hpp"
#include "sound.hpp"
#include "string_utils.hpp"
#include "texture.hpp"
#include "tile_map.hpp"
#include "wml_node.hpp"
#include "wml_parser.hpp"
#include "wml_utils.hpp"
#include "wml_writer.hpp"

void edit_level(const char* level_cfg);

namespace {

void fade_scene(level& lvl, screen_position& screen_pos) {
	for(int n = 0; n < 255; n += 20) {
		lvl.process();
		draw_scene(lvl, screen_pos);
		const SDL_Rect r = {0, 0, graphics::screen_width(), graphics::screen_height()};
		const SDL_Color c = {0,0,0,0};
		graphics::draw_rect(r, c, n);
		SDL_GL_SwapBuffers();
		SDL_Delay(20);		
	}

	const SDL_Rect r = {0, 0, graphics::screen_width(), graphics::screen_height()};
	const SDL_Color c = {0,0,0,0};
	graphics::draw_rect(r, c, 255);
	SDL_GL_SwapBuffers();
}

void flip_scene(level& lvl, screen_position& screen_pos, bool flip_out) {
	if(!flip_out) {
		screen_pos.flip_rotate = 1000;
	}
	for(int n = 0; n != 20; ++n) {
		screen_pos.flip_rotate += 50 * (flip_out ? 1 : -1);
		lvl.process();
		draw_scene(lvl, screen_pos);

		SDL_GL_SwapBuffers();
		SDL_Delay(20);
	}

	screen_pos.flip_rotate = 0;
}

bool show_title_screen(std::string& level_cfg)
{
	preload_level(level_cfg);

	const int CyclesUntilPreloadReplay = 15*20;
	const int CyclesUntilShowReplay = 18*20;

	graphics::texture img(graphics::texture::get("titlescreen.png"));

	for(int cycle = 0; ; ++cycle) {
		if(cycle == CyclesUntilPreloadReplay) {
			preload_level("replay.cfg");
		} else if(cycle == CyclesUntilShowReplay) {
			level_cfg = "replay.cfg";
			return false;
		}

		graphics::prepare_raster();
		graphics::blit_texture(img, 0, 0, graphics::screen_width(), graphics::screen_height());
		SDL_GL_SwapBuffers();
		joystick::update();
		for(int n = 0; n != 6; ++n) {
			if(joystick::button(n)) {
				return false;
			}
		}
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_QUIT:
				return true;
			case SDL_KEYDOWN:
				if(event.key.keysym.sym == SDLK_ESCAPE) {
					return true;
				}

				return false;
			}
		}

		SDL_Delay(50);
	}

	return true;
}

void show_end_game()
{
	const std::string msg = "to be continued...";
	graphics::texture t(font::render_text(msg, graphics::color_white(), 48));
	const int xpos = graphics::screen_width()/2 - t.width()/2;
	const int ypos = graphics::screen_height()/2 - t.height()/2;
	for(int n = 0; n <= msg.size(); ++n) {
		const GLfloat percent = GLfloat(n)/GLfloat(msg.size());
		SDL_Rect rect = {0, 0, graphics::screen_width(), graphics::screen_height()};
		graphics::draw_rect(rect, graphics::color_black());
		graphics::blit_texture(t, xpos, ypos, t.width()*percent, t.height(), 0.0,
						       0.0, 0.0, percent, 1.0);
		SDL_GL_SwapBuffers();
		SDL_Delay(40);
	}

	bool done = false;
	while(!done) {
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_QUIT:
			case SDL_KEYDOWN:
				done = true;
				break;
			}
		}
		joystick::update();
		for(int n = 0; n != 6; ++n) {
			if(joystick::button(n)) {
				done = true;
			}
		}
	}
}

struct key_frames {
	int frame;
	std::string keys;
};

std::vector<key_frames> key_record;
int key_record_pos = 0;

void read_key_frames(const std::string& s) {
	std::vector<std::string> frames = util::split(s, ';');
	foreach(const std::string& f, frames) {
		std::vector<std::string> items = util::split(f, ':', 0);
		if(items.size() == 2) {
			key_frames frame;
			frame.frame = atoi(items.front().c_str());
			frame.keys = items.back();
			key_record.push_back(frame);
		}
	}
}

std::string write_key_frames() {
	std::ostringstream s;
	foreach(const key_frames& f, key_record) {
		s << f.frame << ":" << f.keys << ";";
	}

	return s.str();
}

void play_level(boost::scoped_ptr<level>& lvl, std::string& level_cfg, bool record_replay)
{
	boost::scoped_ptr<level> start_lvl;
	if(record_replay) {
		start_lvl.reset(load_level(level_cfg));
	}

	CKey key;

	int cycle = 0;
	bool done = false;
	while(!done) {
		const int desired_end_time = SDL_GetTicks() + 20;
		if(lvl->player() && lvl->player()->hitpoints() <= 0) {
			boost::intrusive_ptr<pc_character> save = lvl->player()->save_condition();
			preload_level(save->current_level());
			fade_scene(*lvl, last_draw_position());
			level* new_level = load_level(save->current_level());
			sound::play_music(new_level->music());
			if(!save) {
				return;
			}
			set_scene_title(new_level->title());
			new_level->add_player(save);
			save->save_game();
			lvl.reset(new_level);
			last_draw_position() = screen_position();
		}

		const level::portal* portal = lvl->get_portal();
		if(portal) {
			level_cfg = portal->level_dest;
			preload_level(level_cfg);

			const std::string transition = portal->transition;
			if(transition.empty() || transition == "fade") {
				fade_scene(*lvl, last_draw_position());
			} else if(transition == "flip") {
				flip_scene(*lvl, last_draw_position(), true);
			}

			level* new_level = load_level(level_cfg);
			sound::play_music(new_level->music());
			set_scene_title(new_level->title());
			point dest = portal->dest;
			if(portal->dest_starting_pos) {
				character_ptr new_player = new_level->player();
				if(new_player) {
					dest = point(new_player->x(), new_player->y());
				}
			}

			character_ptr player = lvl->player();
			if(player) {
				player->set_pos(dest);
				new_level->add_player(player);
				player->move_to_standing(*new_level);
			}

			lvl.reset(new_level);
			last_draw_position() = screen_position();

			if(transition == "flip") {
				flip_scene(*lvl, last_draw_position(), false);
			}
		}

		joystick::update();
		//if we're in a replay any joystick motion will exit it.
		if(!record_replay && key_record.empty() == false) {
			if(joystick::left() || joystick::right() || joystick::up() || joystick::down() || joystick::button(0) || joystick::button(1) || joystick::button(2) || joystick::button(3)) {
				done = true;
			}
		}

		if(message_dialog::get() == NULL) {
			SDL_Event event;
			while(SDL_PollEvent(&event)) {
				switch(event.type) {
				case SDL_QUIT:
					done = true;
					break;
				case SDL_KEYDOWN: {
					//if we're in a replay any keypress will exit it.
					if(!record_replay && key_record.empty() == false) {
						done = true;
						break;
					}

					const SDLMod mod = SDL_GetModState();
					if(event.key.keysym.sym == SDLK_ESCAPE) {
						done = true;
						break;
					} else if(event.key.keysym.sym == SDLK_e && (mod&KMOD_CTRL)) {
						std::cerr << "editor...\n";
						edit_level(level_cfg.c_str());
						lvl.reset(load_level(level_cfg));
					} else if(event.key.keysym.sym == SDLK_s && (mod&KMOD_CTRL)) {
						std::string data;

						wml::node_ptr lvl_node = wml::deep_copy(lvl->write());
						if(record_replay) {
							lvl_node = wml::deep_copy(start_lvl->write());
							lvl_node->set_attr("replay_data", write_key_frames());
						}
						wml::write(lvl_node, data);
						sys::write_file("save.cfg", data);
					}
					break;
				}
				default:
					break;
				}
			}
		}

		if(record_replay) {
			std::string data;
			key.Write(&data);
			if(key_record.empty() || key_record.back().keys != data) {
				key_frames f;
				f.frame = cycle;
				f.keys = data;
				key_record.push_back(f);
			}
		}

		if(!record_replay && key_record.empty() == false) {
			if(key_record_pos < key_record.size() && key_record[key_record_pos].frame == cycle) {
				if(lvl->player()) {
					lvl->player()->set_key_state(key_record[key_record_pos].keys);
				}
				++key_record_pos;

				std::cerr << "SHOW_FRAME: " << key_record_pos << "/" << key_record.size() << "\n";

				if(key_record_pos == key_record.size()) {
					fade_scene(*lvl, last_draw_position());
					return;
				}
			}
		}

		if(message_dialog::get()) {
			message_dialog::get()->process();
		} else {
			lvl->process();
		}

		if(lvl->end_game()) {
			fade_scene(*lvl, last_draw_position());
			show_end_game();
			done = true;
			break;
		}

		draw_scene(*lvl, last_draw_position());
		SDL_GL_SwapBuffers();

		const int wait_time = std::max<int>(1, desired_end_time - SDL_GetTicks());
		SDL_Delay(wait_time);

		++cycle;
	}
}

}

extern "C" int main(int argc, char** argv)
{
	bool record_replay = false;
	bool fullscreen = false;
	int width = 800, height = 600;
	std::string level_cfg = "frogatto-house.cfg";
	for(int n = 1; n < argc; ++n) {
		std::string arg(argv[n]);
		if(arg == "--fullscreen") {
			fullscreen = true;
		} else if(arg == "--width") {
			std::string w(argv[++n]);
			width = boost::lexical_cast<int>(w);
		} else if(arg == "--height" && n+1 < argc) {
			std::string h(argv[++n]);
			height = boost::lexical_cast<int>(h);
		} else if(arg == "--level" && n+1 < argc) {
			level_cfg = argv[++n];
		} else if(arg == "--record_replay") {
			record_replay = true;
		} else {
			std::cerr << "unrecognized arg: '" << arg << "'\n";
			return 0;
		}
	}

	if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK) < 0) {
		std::cerr << "could not init SDL\n";
		return -1;
	}

	if(SDL_SetVideoMode(width,height,0,SDL_OPENGL|(fullscreen ? SDL_FULLSCREEN : 0)) == NULL) {
		std::cerr << "could not set video mode\n";
		return -1;
	}

	std::cerr << "JOYSTICKS: " << SDL_NumJoysticks() << "\n";

	const load_level_manager load_manager;

	{ //manager scope
	const font::manager font_manager;
	const sound::manager sound_manager;
	const joystick::manager joystick_manager;

	const SDL_Surface* fb = SDL_GetVideoSurface();
	if(fb == NULL) {
		return 0;
	}

	sound::play("arrive.wav");

	graphics::texture::manager texture_manager;

	character_type::init(wml::parse_wml(sys::read_file("characters.cfg")));
	custom_object_type::init(wml::parse_wml(sys::read_file("objects.cfg")));
	item_type::init(wml::parse_wml(sys::read_file("items.cfg")));
	level_object::init(wml::parse_wml(sys::read_file("tiles.cfg")));
	tile_map::init(wml::parse_wml(sys::read_file("tiles.cfg")));
	prop::init(wml::parse_wml(sys::read_file("prop.cfg")));

	glEnable(GL_SMOOTH);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	while(!show_title_screen(level_cfg)) {
		last_draw_position() = screen_position();

		boost::scoped_ptr<level> lvl(load_level(level_cfg));
		assert(lvl.get());
		sound::play_music(lvl->music());
		if(lvl->player()) {
			lvl->player()->set_current_level(level_cfg);
			lvl->player()->save_game();
		}
		set_scene_title(lvl->title());

		if(lvl->replay_data().empty() == false) {
			read_key_frames(lvl->replay_data());
			key_record_pos = 0;
		}

		play_level(lvl, level_cfg, record_replay);
		level_cfg = "frogatto-house.cfg";

		key_record.clear();
		key_record_pos = 0;
	}

	} //end manager scope, make managers destruct before calling SDL_Quit
	std::cerr << "quitting...\n";
	SDL_Quit();
	std::cerr << "quit called...\n";
	return 0;
}
