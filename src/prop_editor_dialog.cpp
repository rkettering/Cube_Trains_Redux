#include <boost/bind.hpp>

#include "button.hpp"
#include "editor.hpp"
#include "foreach.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "label.hpp"
#include "prop_editor_dialog.hpp"
#include "raster.hpp"

namespace editor_dialogs
{

prop_editor_dialog::prop_editor_dialog(editor& e)
  : gui::dialog(graphics::screen_width() - 160, 40, 160, 560), editor_(e)
{
	if(editor_.get_props().empty() == false) {
		category_ = editor_.get_props().front()->category();
	}
	init();
}

void prop_editor_dialog::init()
{
	clear();
	using namespace gui;
	set_padding(20);

	const_prop_ptr prop = editor_.get_props()[editor_.get_item()];
	image_widget* preview = new image_widget(prop->get_frame().img());
	preview->set_dim(150, 150);
	preview->set_area(prop->get_frame().area());
	add_widget(widget_ptr(preview), 10, 10);

	button* category_button = new button(widget_ptr(new label(category_, graphics::color_white())), boost::bind(&prop_editor_dialog::show_category_menu, this));
	add_widget(widget_ptr(category_button));

	grid_ptr grid(new gui::grid(2));
	int index = 0;
	foreach(const_prop_ptr prop, editor_.get_props()) {
		if(prop->category() == category_) {
			image_widget* preview = new image_widget(prop->get_frame().img());
			preview->set_dim(64, 64);
			preview->set_area(prop->get_frame().area());
			button_ptr prop_button(new button(widget_ptr(preview), boost::bind(&prop_editor_dialog::set_prop, this, index)));
			prop_button->set_dim(68, 68);
			grid->add_col(prop_button);
		}

		++index;
	}

	grid->finish_row();
	add_widget(grid);
}

void prop_editor_dialog::show_category_menu()
{
	using namespace gui;
	gui::grid* grid = new gui::grid(2);
	grid->set_show_background(true);
	grid->set_hpad(10);
	grid->allow_selection();
	grid->register_selection_callback(boost::bind(&prop_editor_dialog::close_context_menu, this, _1));

	std::set<std::string> categories;
	foreach(const_prop_ptr prop, editor_.get_props()) {
		if(categories.count(prop->category())) {
			continue;
		}

		categories.insert(prop->category());

		image_widget* preview = new image_widget(prop->get_frame().img());
		preview->set_dim(48, 48);
		preview->set_area(prop->get_frame().area());
		grid->add_col(widget_ptr(preview))
		     .add_col(widget_ptr(new label(prop->category(), graphics::color_white())));
		grid->register_row_selection_callback(boost::bind(&prop_editor_dialog::select_category, this, prop->category()));
	}

	int mousex, mousey;
	SDL_GetMouseState(&mousex, &mousey);
	if(mousex + grid->width() > graphics::screen_width()) {
		mousex = graphics::screen_width() - grid->width();
	}

	if(mousey + grid->height() > graphics::screen_height()) {
		mousey = graphics::screen_height() - grid->height();
	}

	mousex -= x();
	mousey -= y();

	remove_widget(context_menu_);
	context_menu_.reset(grid);
	add_widget(context_menu_, mousex, mousey);
}

void prop_editor_dialog::set_prop(int index)
{
	editor_.set_item(index);
	init();
}

void prop_editor_dialog::close_context_menu(int index)
{
	remove_widget(context_menu_);
	context_menu_.reset();
}

void prop_editor_dialog::select_category(const std::string& category)
{
	category_ = category;
	init();
}

}
