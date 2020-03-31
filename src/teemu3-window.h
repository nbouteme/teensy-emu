/* teemu3-window.h
 *
 * Copyright 2020 nbouteme
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtkmm/builder.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/layout.h>
#include <gtkmm/window.h>

class Teemu3Window : public Gtk::Window
{
public:
	Teemu3Window();

private:
	Gtk::HeaderBar *headerbar;
	Gtk::Layout *label;
	Glib::RefPtr<Gtk::Builder> builder;
};
