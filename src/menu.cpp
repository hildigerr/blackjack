// -*- mode:C++; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil -*-
/*
 * Blackjack - menu.cpp
 *
 * Copyright (C) 2003-2004 William Jon McCann <mccann@jhu.edu>
 *
 * This game is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#ifdef HAVE_GNOME
#include <gnome.h>
#endif

using namespace std;

#include <games-stock.h>

#include "blackjack.h"
#include "menu.h"
#include "dialog.h"
#include "game.h"
#include "hand.h"

void
on_game_restart_activate (GtkAction *action,
                          gpointer   data)
{
        bj_game_new (bj_game_get_rules_file (), &seed);
}

void
on_game_new_activate (GtkAction *action,
                      gpointer   data)
{
        bj_game_new (bj_game_get_rules_file (), NULL);
}

void
on_game_hint_activate (GtkAction *action,
                       gpointer   data)
{
        show_hint_dialog ();
}

void
on_game_quit_activate (GtkAction *action,
                       gpointer   data)
{
        bj_quit_app ();
}

void
on_toolbar_activate (GtkToggleAction *action,
                     gpointer         data)
{
        gboolean value = gtk_toggle_action_get_active (action);
        bj_set_show_toolbar (value);
}

void
on_preferences_activate (GtkAction *action,
                         gpointer   data)
{
        show_preferences_dialog ();
}

void
on_control_deal_activate (GtkAction *action,
                         gpointer   data)
{
        bj_hand_new_deal ();
}

void
on_control_hit_activate (GtkAction *action,
                         gpointer   data)
{
        bj_hand_hit_with_delay ();
}

void
on_control_stand_activate (GtkAction *action,
                           gpointer   data)
{
        bj_hand_stand ();
}

void
on_control_surrender_activate (GtkAction *action,
                               gpointer   data)
{
        bj_hand_surrender ();
}

void
on_control_split_activate (GtkAction *action,
                               gpointer   data)
{
        bj_hand_split ();
}

void
on_control_double_activate (GtkAction *action,
                            gpointer   data)
{
        bj_hand_double ();
}

void
on_help_contents_activate (GtkAction *action,
                           gpointer   data)
{
#ifdef HAVE_GNOME
        GError *error = NULL;
        gnome_help_display_desktop_on_screen (NULL, "blackjack", "blackjack", NULL,
                                              gtk_widget_get_screen (toplevel_window), &error);
#endif
}

void
on_help_about_activate (GtkAction *action,
                        gpointer   data)
{
        const gchar *authors[] = {
                "William Jon McCann <mccann@jhu.edu>",
                "Eric Farmer <erfarmer@comcast.net>",
                NULL
        };

        const gchar *documenters[] = {
                "William Jon McCann <mccann@jhu.edu>",
                NULL
        };
	gchar *license = games_get_license (_("Blackjack"));

        gtk_show_about_dialog (GTK_WINDOW (toplevel_window),
                               "authors", authors,
                               "documenters", documenters,
                               "version", VERSION,
                               "copyright", "Copyright \xc2\xa9 2003-2006 William Jon McCann, Eric Farmer",
                               "comments", _("Blackjack is a casino-style card game."),
                               "logo-icon-name", "gnome-blackjack",
                               "license", license,
                               "translator_credits", _("translator-credits"),
                               "logo-icon-name", "gnome-aisleriot.png",
                               "website", "http://www.gnome.org/projects/gnome-games/",
                               "wrap-license", TRUE,
                               NULL);
	g_free (license);

        return;
}
