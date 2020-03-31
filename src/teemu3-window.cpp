/* teemu3-window.cpp
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

#include "teemu3-window.h"
#include "teensy_emu.h"
#include <gtkmm/image.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/eventbox.h>
#include <glibmm/main.h>

#include <functional>


Teemu3Window::Teemu3Window()
	: Glib::ObjectBase("Teemu3Window")
	, Gtk::Window()
	, headerbar(nullptr)
	, label(nullptr)
{
	this->set_default_size(1280, 720);
	this->resize(1280, 720);
    this->property_resizable() = 0;

	builder = Gtk::Builder::create_from_resource("/org/boutemeur/teemu3/teemu3-window.ui");
	builder->get_widget("headerbar", headerbar);
	builder->get_widget("label", label);

    Gtk::EventBox *sdcard, *remote, *bevent;
    Gtk::Image *remoteimg, *backlight;
    Gtk::FileChooserButton *modchoose;
    Gtk::TextView *console;
    Gtk::Label *lcdscreen;
    Gtk::ScrolledWindow *conscroll;

	builder->get_widget("modchoose", modchoose);
	builder->get_widget("backlight", backlight);
	builder->get_widget("conscroll", conscroll);
	builder->get_widget("console", console);
	builder->get_widget("lcdscreen", lcdscreen);
	builder->get_widget("remote", remoteimg);
	builder->get_widget("sdevent", sdcard);
	builder->get_widget("bevent", bevent);
	builder->get_widget("remoteevent", remote);

	sdcard->signal_button_press_event().connect([this](GdkEventButton *event) {
        Gtk::FileChooserDialog fcd("Choose a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
        fcd.set_transient_for(*this);
        fcd.add_button("_Cancel", 2);
        fcd.add_button("Select", 1);
        if (fcd.run() == 1) {
        	set_teensy_sd_root(fcd.get_filename().c_str());
        }
        return false;
	});
	bevent->signal_button_press_event().connect([this](GdkEventButton *event) {
        triggerInt(17, 0); // le bouton sur mon cablage est branché sur la broche 17...
        return false;
	});
	remote->signal_button_press_event().connect([=](GdkEventButton *event) {
        // flemme de faire 21 boutons donc on calcul le bouton appuyé sur l'image
        // par rapport aux coordonnés du clic
        static uint16_t keys[] = { // Pour ma télécommande...
		        0xA25D, 0x629D, 0xE21D,
		        0x22DD, 0x02FD, 0xC23D,
		        0xE01F, 0xA857, 0x906F,
		        0x6897, 0x9867, 0xB04F,
		        0x30CF, 0x18E7, 0x7A85,
		        0x10EF, 0x38C7, 0x5AA5,
		        0x42BD, 0x4AB5, 0x52AD
	        };

        int x, y;
        remote->get_pointer(x, y);
        auto buff = remoteimg->get_pixbuf();
        int w = buff->get_width();
        int h = buff->get_height();
        int rad = w / 6; // distance max d'un bouton

        int t = w / 3; // un tier de largeur (trois boutons par ligne)
        int c = x / t; // dans quel colonne le pointeur est
        t = h / 7; // un septieme (7 boutons par colonne)
        int r = y / t; // dans quel colonne le pointeur est
        teensy_remote_send(0xFF0000 | keys[r * 3 + c]);
        return false;
	});

	add(*label);
	label->show();
	set_titlebar(*headerbar);
	headerbar->show();

	set_teensy_serial_print(
	    [=](const void *data, size_t len){
	        char *d = (char*)malloc(len + 1);
	        memcpy(d, data, len);
	        d[len] = 0;
	        Glib::signal_idle().connect([=](){
	            const char *cd = (const char*)d;
	            auto buffer = console->get_buffer();
	            buffer->insert(buffer->end(), cd, cd + len);
	            auto adj = conscroll->get_vadjustment();
	            adj->set_value(adj->get_upper() - adj->get_page_size());
	            free(d);
	            return false;
	        });
	    }
	);

	set_teensy_lcd_print(
	    [=](const void *data, size_t len){
	        char *d = (char*)malloc(len + 1);
	        memcpy(d, data, len);
	        d[len] = 0;
	        Glib::signal_idle().connect([=](){
	            const char *cd = (const char*)d;
	            char *formatted;
	            asprintf(&formatted, "%.16s\n%.16s", cd, cd + 16);
                lcdscreen->set_text(formatted);
                free(formatted);
                free(d);
                return false;
            });
	    }
	);

    // la broche 9 est connecté à un transistor qui controle l'alimentation
    // du rétro éclairage
	teensy_observe_pin(9, [=](int v){
	        Glib::signal_idle().connect([=](){
                std::string s = "/org/boutemeur/teemu3/screen";
                s += v ? "on" : "off";
                s += ".png";
                backlight->set_from_resource(s);
                return false;
            });
	});

	modchoose->signal_file_set().connect([=](){
	    if (teensy_emu_init(modchoose->get_filename().c_str())) {
            auto buffer = console->get_buffer();
            buffer->erase(buffer->begin(), buffer->end());
            // pas besoin de reset l'écran car un programme est censé partir du
            // principe qu'il est dans un état indéfini, et l'initialisera lui même
	        return;
	    }
	    Gtk::MessageDialog d("An error occured");
	    d.run();
	});

}

