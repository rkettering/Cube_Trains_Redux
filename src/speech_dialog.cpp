#include <iostream>
#include <stack>

#include "color_utils.hpp"
#include "draw_scene.hpp"
#include "foreach.hpp"
#include "frame.hpp"
#include "framed_gui_element.hpp"
#include "graphical_font.hpp"
#include "gui_section.hpp"
#include "joystick.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "speech_dialog.hpp"

namespace {
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
	const int OptionHeight = 70;
	const int OptionWidth = 200;
	const int OptionXPad = 20;
#else
	const int OptionHeight = 45;
	const int OptionWidth = 150;
	const int OptionXPad = 10;
#endif
	const int OptionsBorder = 20; // size of the border around the options window
	const int OptionsX = 135; // these denote the bottom right corner
	const int OptionsY = 115;
}

speech_dialog::speech_dialog()
  : cycle_(0), left_side_speaking_(false), horizontal_position_(0), text_char_(0), option_selected_(0),
    joystick_button_pressed_(true),
    joystick_up_pressed_(true),
    joystick_down_pressed_(true),
	expiration_(-1)
{
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
	option_selected_ = -1;
#endif
}

speech_dialog::~speech_dialog()
{
}

bool speech_dialog::handle_mouse_move(int x, int y)
{
	if(preferences::screen_rotated()) {
		x = preferences::actual_screen_width() - x;
		std::swap(x, y);
		x *= 2;
		y *= 2;
	}
	rect box(
		preferences::virtual_screen_width() - OptionsX - OptionWidth - OptionsBorder,
		preferences::virtual_screen_height() - OptionsY - OptionHeight*options_.size() - OptionsBorder,
		OptionWidth + OptionsBorder*2, OptionHeight*options_.size() + OptionsBorder*2
	);
	//std::cerr << "Options box: " << box << " : " << x << " : " << y << "\n";
	if (point_in_rect(point(x, y), box))
	{
		option_selected_ = (y-box.y())/OptionHeight;
		return true;
	} else {
		option_selected_ = -1;
		return false;
	}
}

void speech_dialog::move_up()
{
	--option_selected_;
	if(option_selected_ < 0) {
		option_selected_ = options_.size() - 1;
	}
}

void speech_dialog::move_down()
{
	++option_selected_;
	if(option_selected_ == options_.size()) {
		option_selected_ = 0;
	}
}

bool speech_dialog::key_press(const SDL_Event& event)
{
	static int last_mouse = 0;
	if(text_char_ == num_chars() && options_.empty() == false) {
		if(event.type == SDL_KEYDOWN)
		{
			switch(event.key.keysym.sym) {
			case SDLK_UP:
				move_up();
				break;
			case SDLK_DOWN:
				move_down();
				break;
			case SDLK_RETURN:
			case SDLK_SPACE:
			case SDLK_a:
			case SDLK_s:
				return true;
			default:
				break;
			}
			
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
			if(event.type == SDL_MOUSEBUTTONDOWN)
			{
				last_mouse = event.button.which;
				handle_mouse_move(event.button.x, event.button.y);
			}
			if (event.type == SDL_MOUSEMOTION)
			{
				if (event.motion.which == last_mouse)
					handle_mouse_move(event.motion.x, event.motion.y);
			}
			if (event.type == SDL_MOUSEBUTTONUP)
			{
				if (event.motion.which == last_mouse)
				{
					last_mouse = -1;
					return handle_mouse_move(event.motion.x, event.motion.y);
				}
			}
#endif
		}

		return false;
	}

	if(text_char_ < num_chars()) {
		text_char_ = num_chars();
		return false;
	}

	if(text_.size() > 2) {
		text_.erase(text_.begin());
		text_char_ = text_.front().size();
		return false;
	}

	return true;
}

bool speech_dialog::process()
{
	++cycle_;

	if(text_char_ < num_chars()) {
		++text_char_;
	}

	const int ScrollSpeed = 20;
	if(left_side_speaking_) {
		if(horizontal_position_ > 0) {
			horizontal_position_ -= ScrollSpeed;
			if(horizontal_position_ < 0) {
				horizontal_position_ = 0;
			}
		}
	} else {
		const int width = gui_section::get("speech_portrait_pane")->width();
		if(horizontal_position_ < width) {
			horizontal_position_ += ScrollSpeed;
			if(horizontal_position_ > width) {
				horizontal_position_ = width;
			}
		}
	}

	if(expiration_ <= 0) {
		joystick::update();

		if(!joystick_up_pressed_ && joystick::up()) {
			move_up();
		}

		if(!joystick_down_pressed_ && joystick::down()) {
			move_down();
		}

		if(!joystick_button_pressed_ && (joystick::button(0) || joystick::button(10))) {
			return true;
		}
	}

	joystick_up_pressed_ = joystick::up();
	joystick_down_pressed_ = joystick::down();
	joystick_button_pressed_ = joystick::button(0) || joystick::button(1);

	return cycle_ == expiration_;
}

void speech_dialog::draw() const
{
	static const const_gui_section_ptr top_corner = gui_section::get("speech_dialog_top_corner");
	static const const_gui_section_ptr bottom_corner = gui_section::get("speech_dialog_bottom_corner");
	static const const_gui_section_ptr top_edge = gui_section::get("speech_dialog_top_edge");
	static const const_gui_section_ptr bottom_edge = gui_section::get("speech_dialog_bottom_edge");
	static const const_gui_section_ptr side_edge = gui_section::get("speech_dialog_side_edge");
	static const const_gui_section_ptr arrow = gui_section::get("speech_dialog_arrow");

	const_graphical_font_ptr font = graphical_font::get("default");

	const int TextAreaHeight = 80;

	const int TextBorder = 10;
	const rect pane_area(
	  top_corner->width(),
	  graphics::screen_height() - TextAreaHeight + TextBorder,
	  graphics::screen_width() - top_corner->width()*2,
	  TextAreaHeight - bottom_corner->height());

	const rect text_area(pane_area.x()-30, pane_area.y()-30, pane_area.w()+60, pane_area.h()+60);

	graphics::draw_rect(pane_area, graphics::color(85, 53, 53, 255));
	top_corner->blit(pane_area.x() - top_corner->width(), pane_area.y() - top_corner->height());
	top_corner->blit(pane_area.x2()-1, pane_area.y() - top_corner->height(), -top_corner->width(), top_corner->height());

	top_edge->blit(pane_area.x(), pane_area.y() - top_edge->height(), pane_area.w(), top_edge->height());

	bottom_corner->blit(pane_area.x() - bottom_corner->width(), pane_area.y2());
	bottom_corner->blit(pane_area.x2()-1, pane_area.y2(), -bottom_corner->width(), bottom_corner->height());

	bottom_edge->blit(pane_area.x(), pane_area.y2(), pane_area.w(), bottom_edge->height());

	side_edge->blit(pane_area.x() - side_edge->width(), pane_area.y(), side_edge->width(), pane_area.h());
	side_edge->blit(pane_area.x2()-1, pane_area.y(), -side_edge->width(), pane_area.h());

	const_entity_ptr speaker = left_side_speaking_ ? left_ : right_;
	if(speaker) {
		const screen_position& pos = last_draw_position();
		const int screen_x = pos.x/100 + (graphics::screen_width()/2)*(-1.0/pos.zoom + 1.0);
		const int screen_y = pos.y/100 + (graphics::screen_height()/2)*(-1.0/pos.zoom + 1.0);

		const int xpos = (speaker->feet_x() - screen_x)*pos.zoom - 36;
		const int ypos = (speaker->feet_y() - screen_y)*pos.zoom - 10;

		//if the arrow to the speaker is within reasonable limits, then
		//blit it.
		if(xpos > top_corner->width() && xpos < graphics::screen_width() - top_corner->width() - arrow->width()) {
			arrow->blit(xpos, pane_area.y() - arrow->height() - 30);
		}
	}


	//we center our text. Create a vector of the left edge of the text.
	std::vector<int> text_left_align;

	int total_height = 0;
	for(int n = 0; n < text_.size(); ++n) {
		rect area = font->dimensions(text_[n]);

		if(n < 2) {
			total_height += area.h();
		}

		const int width = area.w();
		const int left = text_area.x() + text_area.w()/2 - width/2;
		text_left_align.push_back(left);
	}

	int ypos = text_area.y() + (text_area.h() - total_height)/2;
	int nchars = text_char_;
	for(int n = 0; n < 2 && n < text_.size() && nchars > 0; ++n) {
		std::string str(text_[n].begin(), text_[n].begin() +
		                  std::min<int>(nchars, text_[n].size()));
		//currently the color of speech is hard coded.
		glColor4ub(255, 187, 10, 255);
		rect area = font->draw(text_left_align[n], ypos, str);
		glColor4f(1.0, 1.0, 1.0, 1.0);
		ypos = area.y2();
		nchars -= text_[n].size();
	}

	if(text_.size() > 2 && text_char_ == num_chars() && (cycle_&16)) {
		const_gui_section_ptr down_arrow = gui_section::get("speech_text_down_arrow");
		down_arrow->blit(text_area.x2() - down_arrow->width() - 10, text_area.y2() - down_arrow->height() - 10);
		
	}

	if(text_char_ == num_chars() && options_.empty() == false) {
		//const_gui_section_ptr options_panel = gui_section::get("speech_portrait_pane");
		const_framed_gui_element_ptr options_panel = framed_gui_element::get("regular_window");
		int xpos = graphics::screen_width() - OptionsX - OptionWidth - OptionsBorder*2;
		int ypos = graphics::screen_height() - OptionsY - OptionHeight*options_.size() - OptionsBorder*2;
		// the division by 2 (and lack of multiplication of OptionsBorder) here is
		// because the last options specifies that it will multiply everything by 2
		options_panel->blit(xpos, ypos, OptionsBorder + OptionWidth/2, OptionsBorder + (OptionHeight * options_.size())/2, 2);

		xpos += OptionsBorder + OptionXPad;
		ypos += OptionsBorder;

		glColor4ub(255, 187, 10, 255);
		int index = 0;
		foreach(const std::string& option, options_) {
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
			if(index == option_selected_) {
				graphics::draw_rect(rect(xpos-OptionXPad, ypos, OptionWidth, OptionHeight), graphics::color(0xC74545FF));
				glColor4ub(255, 187, 10, 255); //reset color to what it was, since draw_rect changes it
			}
#endif
			rect area = font->dimensions(option);
			area = font->draw(xpos, ypos+(OptionHeight/2-area.h()/4), option);

#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_IPHONE
			if(index == option_selected_) {
				const_gui_section_ptr cursor = gui_section::get("cursor");
				cursor->blit(area.x2(), area.y());
			}
#endif

			ypos += OptionHeight;
			++index;
		}
		glColor4f(1.0, 1.0, 1.0, 1.0);
	}
}

void speech_dialog::set_speaker_and_flip_side(const_entity_ptr e)
{
	std::cerr << "set speaker\n";
	left_side_speaking_ = !left_side_speaking_;
	set_speaker(e, left_side_speaking_);
}

void speech_dialog::set_speaker(const_entity_ptr e, bool left_side)
{
	if(left_side) {
		left_ = e;
	} else {
		right_ = e;
	}
}

void speech_dialog::set_side(bool left_side)
{
	left_side_speaking_ = left_side;
}

void speech_dialog::set_text(const std::vector<std::string>& text)
{
	text_ = text;
	text_char_ = 0;
}

void speech_dialog::set_options(const std::vector<std::string>& options)
{
	options_ = options;
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
	option_selected_ = -1;
#else
	option_selected_ = 0;
#endif
}

int speech_dialog::num_chars() const
{
	int res = 0;
	if(text_.size() >= 1) {
		res += text_[0].size();
	}

	if(text_.size() >= 2) {
		res += text_[1].size();
	}

	return res;
}
