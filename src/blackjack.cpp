// -*- mode:C++; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil -*-
/* Blackjack
 * Copyright (C) 2003 William Jon McCann <mccann@jhu.edu>
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

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>

#include "blackjack.h"
#include "events.h"
#include "draw.h"
#include "slot.h"
#include "card.h"
#include "menu.h"
#include "dialog.h"

#include "game.h"
#include "hand.h"

#include <iostream>
using namespace std;

// Global Variables

GtkWidget        *app;
GtkWidget        *playing_area;
GtkWidget        *option_dialog = NULL;
GdkGC            *draw_gc = NULL;
GdkGC            *bg_gc = NULL;
GdkGC            *slot_gc = NULL;
GdkPixmap        *surface;

gchar            *card_style;

GtkWidget        *wager_value_label;

static GtkWidget *shoe_value_label = NULL;

GtkWidget        *status_bar;
GtkWidget        *balance_value_label;

guint32          seed;

guint            x_spacing = 5;
guint            y_spacing = 15;
double           x_expanded_offset = 0.21;
double           y_expanded_offset = 0.21;

gboolean         events_pending = false;

static GConfClient *gconf_client = NULL;

gfloat wager_value = 5.0;
gfloat balance_value = 0.0;
gboolean quick_deals = FALSE;
gboolean show_probabilities = FALSE;
gboolean show_toolbar = TRUE;
gchar *game_variation = NULL;

#define DEFAULT_VARIATION      "Vegas_Strip.rules"

void
bj_make_window_title (gchar *game_name, gint lseed) 
{
        char *title;

        title = g_strdup_printf (_("Blackjack: %s (%d)"), game_name, lseed);

        gtk_window_set_title (GTK_WINDOW (app), title); 

        g_free (title);
}

void
bj_gui_show_toolbar (gboolean do_toolbar)
{
        BonoboDockItem *toolbar_gdi;

        toolbar_gdi = gnome_app_get_dock_item_by_name (GNOME_APP(app), 
                                                       GNOME_APP_TOOLBAR_NAME);
        if (do_toolbar) {
                gtk_widget_show (GTK_WIDGET(toolbar_gdi));
        }
        else {
                gtk_widget_hide (GTK_WIDGET(toolbar_gdi));
                gtk_widget_queue_resize (app);
        }
}

void
bj_show_balance (gfloat balance)
{
        gchar *b;
        b = g_strdup_printf ("%.2f  ", balance);

        if (balance_value_label)
                gtk_label_set_text (GTK_LABEL (balance_value_label), b);
        g_free (b);
}

void
bj_show_shoe_cards (gfloat value)
{
        gchar *str = g_strdup_printf ("%d", (gint)value);
        if (shoe_value_label)
                gtk_label_set_text (GTK_LABEL (shoe_value_label), str);
        g_free (str);
}

gdouble
bj_get_wager () 
{
        return wager_value;
}

void
bj_set_wager (gdouble value)
{
        if (value < 5)
                value = 5;
        if (value > 500)
                value = 500;

        wager_value = value;
        gchar *valstr = g_strdup_printf ("%.2f", value);
        gtk_label_set_text (GTK_LABEL (wager_value_label), valstr);
        g_free (valstr);
}

void
bj_adjust_wager (gdouble offset)
{
        gdouble wager;
        wager = bj_get_wager ();
        wager += offset;
        bj_set_wager (wager);
}

static void
bj_create_board ()
{
        playing_area = gtk_drawing_area_new ();
        gtk_widget_set_events (playing_area, 
                               gtk_widget_get_events (playing_area) 
                               | GAME_EVENTS );

        gnome_app_set_contents (GNOME_APP (app), playing_area);

        GTK_WIDGET_SET_FLAGS (playing_area, GTK_CAN_FOCUS);
        gtk_widget_grab_focus (playing_area);

        g_signal_connect (playing_area, "expose_event",  
                          G_CALLBACK (bj_event_expose_callback), NULL);
        g_signal_connect (playing_area,"button_release_event",
                          G_CALLBACK (bj_event_button_release), NULL);
        g_signal_connect (playing_area, "motion_notify_event",
                          G_CALLBACK (bj_event_motion_notify), NULL);
        g_signal_connect (playing_area, "enter_notify_event",
                          G_CALLBACK (bj_event_enter_notify), NULL);
        g_signal_connect (playing_area, "button_press_event",
                          G_CALLBACK (bj_event_button_press), NULL);
        g_signal_connect (playing_area, "key_press_event",
                          G_CALLBACK (bj_event_key_press), NULL);
        g_signal_connect (playing_area, "configure_event",
                          G_CALLBACK (bj_event_playing_area_configure), NULL);
}

gboolean
bj_quit_app (GtkMenuItem *menuitem)
{
        delete rules;
        delete strategy;
        delete dealer;
        delete dealerProbabilities;
        delete shoe;
        delete distribution;

        g_list_free (playerHands);

        bj_card_free_pixmaps ();
        bj_slot_free_pixmaps ();
        g_object_unref (surface);
        g_object_unref (press_data->moving_cards);

        g_object_unref (gconf_client);
        gtk_widget_destroy (app);
        gtk_main_quit ();

        return TRUE;
}

static void
create_main_window ()
{
        /* This is the prefix used to retrieve the state when NOT restarted: */
        const gchar* prefix = 
                gnome_client_get_global_config_prefix (gnome_master_client ());
	if (!gconf_client)
        	gconf_client = gconf_client_get_default ();
        gint width, height;

        width = gconf_client_get_int (gconf_client, GCONF_KEY_WIDTH, NULL);
        height = gconf_client_get_int (gconf_client, GCONF_KEY_HEIGHT, NULL);

        app = gnome_app_new ("blackjack", _("Blackjack"));
        /* Use "prefix" as the default config location ... */
        gnome_config_push_prefix (prefix);
        /* ... and use it for the menubar and toolbar aw well: */
        GNOME_APP (app)->prefix = (gchar*)prefix;

        gtk_window_set_default_size (GTK_WINDOW (app), width, height);


        gtk_widget_realize (app);

        g_signal_connect (app, "delete_event", 
                          G_CALLBACK (bj_quit_app), NULL);
        g_signal_connect (app, "configure_event",
                          G_CALLBACK (bj_event_configure), NULL);
}

static void
create_press_data ()
{
        GdkWindowAttr attributes;

        attributes.wclass = GDK_INPUT_OUTPUT;
        attributes.window_type = GDK_WINDOW_CHILD;
        attributes.event_mask = 0;
        attributes.width = card_width;
        attributes.height = card_height;
        attributes.colormap = gdk_drawable_get_colormap (GDK_DRAWABLE (playing_area->window));
        attributes.visual = gdk_drawable_get_visual (GDK_DRAWABLE (playing_area->window));
  
        press_data = (press_data_type*) g_malloc (sizeof (press_data_type));
        press_data->moving_cards = gdk_window_new (playing_area->window, &attributes,
                                                   (GDK_WA_VISUAL | GDK_WA_COLORMAP));
        press_data->status = 0;
}

static void
create_chip_stack_press_data ()
{
        GdkWindowAttr attributes;

        attributes.wclass = GDK_INPUT_OUTPUT;
        attributes.window_type = GDK_WINDOW_CHILD;
        attributes.event_mask = 0;
        attributes.width = chip_width;
        attributes.height = attributes.width;
        attributes.colormap = gdk_drawable_get_colormap (GDK_DRAWABLE (playing_area->window));
        attributes.visual = gdk_drawable_get_visual (GDK_DRAWABLE (playing_area->window));
  
        chip_stack_press_data = 
                (chip_stack_press_data_type*) g_malloc (sizeof (chip_stack_press_data_type));
        chip_stack_press_data->moving_chips = gdk_window_new (playing_area->window, 
                                                              &attributes,
                                                              (GDK_WA_VISUAL 
                                                               | GDK_WA_COLORMAP));
        chip_stack_press_data->status = 0;
}

static void
main_prog (int argc, char *argv[])
{
        GtkWidget *wager_label, *balance_label, *status_box, *group_box;
        gchar *label_string;
        guint context_id;

        seed = time (NULL);
        g_random_set_seed (seed);

        create_main_window ();

        bj_card_load_pixmaps (app, bj_get_card_style ());
        bj_slot_load_pixmaps ();
 
        bj_create_board ();
        bj_menu_create ();

        status_box = gtk_hbox_new (FALSE, GNOME_PAD);

        group_box = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (status_box), group_box, FALSE, FALSE, 0);

        GtkWidget *shoe_label = gtk_label_new (_("Cards left:"));
        gtk_box_pack_start (GTK_BOX (group_box), shoe_label, FALSE, FALSE, 0);
        shoe_value_label = gtk_label_new ("");
        gtk_box_pack_start (GTK_BOX (group_box), shoe_value_label, FALSE, FALSE, 0);


	group_box = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (status_box), group_box, FALSE, FALSE, 0);

        wager_label = gtk_label_new (_("Wager:"));
        gtk_box_pack_start (GTK_BOX (group_box), wager_label, FALSE, FALSE, 0);

        wager_value = 5.0;
        label_string = g_strdup_printf ("%.2f", wager_value);
        wager_value_label = gtk_label_new (label_string);
        g_free (label_string);
        gtk_box_pack_start (GTK_BOX (group_box), wager_value_label, FALSE, FALSE, 0);


	group_box = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (status_box), group_box, FALSE, FALSE, 0);

        balance_label = gtk_label_new (_("Balance:"));
        gtk_box_pack_start (GTK_BOX (group_box), balance_label, FALSE, FALSE, 0);
        label_string = g_strdup_printf ("%.2f", balance_value);
        balance_value_label = gtk_label_new (label_string);
        g_free (label_string);
        gtk_box_pack_start (GTK_BOX (group_box), balance_value_label, FALSE, FALSE, 0);


        status_bar = gtk_statusbar_new ();
        gtk_box_pack_start (GTK_BOX (status_box), status_bar, TRUE, TRUE, 0);
        gnome_app_set_statusbar_custom (GNOME_APP (app), status_box, status_bar);

        context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (status_bar),
                                                   "help");
        gtk_statusbar_push (GTK_STATUSBAR (status_bar), context_id,
                            _("Place your wager or deal a hand"));

        gtk_widget_show_all (status_box);

        bj_menu_install_hints (GNOME_APP (app));

        bj_game_new (bj_get_game_variation (), &seed);

        gtk_widget_show (app);

        bj_gui_show_toolbar (show_toolbar);

        create_press_data ();
        create_chip_stack_press_data ();

        gtk_widget_pop_colormap ();

        gtk_main ();
}

GConfClient *
bj_gconf_client ()
{
        if (!gconf_client) 
                gconf_client = gconf_client_get_default ();
        return gconf_client;
}

static void
bj_gconf_balance_cb (GConfClient *client, guint cnxn_id, 
                     GConfEntry *entry, gpointer user_data)
{
        balance_value = gconf_client_get_float (client, GCONF_KEY_BALANCE, NULL);
        bj_show_balance (balance_value);
}

static void
bj_gconf_card_style_cb (GConfClient *client, guint cnxn_id, 
                        GConfEntry *entry, gpointer user_data)
{
        gchar *lcard_style;

        lcard_style = bj_get_card_style ();
        bj_card_set_theme (lcard_style);
        bj_draw_refresh_screen ();
}

static void
bj_gconf_show_probabilities_cb (GConfClient *client, guint cnxn_id, 
                                GConfEntry *entry, gpointer user_data)
{
        show_probabilities = gconf_client_get_bool (client,
                                                    GCONF_KEY_SHOW_PROBABILITIES,
                                                    NULL);
        bj_draw_refresh_screen ();
}

static void
bj_gconf_quick_deal_cb (GConfClient *client, guint cnxn_id, 
                        GConfEntry *entry, gpointer user_data)
{
        quick_deals = gconf_client_get_bool (client, GCONF_KEY_QUICK_DEAL, NULL);
        bj_draw_refresh_screen ();
}

static void
bj_gconf_show_toolbar_cb (GConfClient *client, guint cnxn_id, 
                          GConfEntry *entry, gpointer user_data)
{
        gboolean lshow_toolbar;

        lshow_toolbar = gconf_client_get_bool (client, GCONF_KEY_SHOW_TOOLBAR, NULL);
        bj_gui_show_toolbar (lshow_toolbar);
}

gchar *
bj_get_card_style ()
{
        gchar *lcard_style;

        lcard_style = gconf_client_get_string (bj_gconf_client (), 
                                              GCONF_KEY_CARD_STYLE,
                                              NULL);
        return lcard_style;
}

void
bj_set_card_style (gchar *value)
{
        gconf_client_set_string (bj_gconf_client (), 
                                 GCONF_KEY_CARD_STYLE,
                                 value,
                                 NULL);
}

gboolean
bj_get_show_probabilities ()
{
        return show_probabilities;
}
void
bj_set_show_probabilities (gboolean value)
{
        gconf_client_set_bool (bj_gconf_client (), 
                               GCONF_KEY_SHOW_PROBABILITIES,
                               value, NULL);
}

gboolean
bj_get_show_toolbar ()
{
        return show_toolbar;
}
void
bj_set_show_toolbar (gboolean value)
{
        show_toolbar = value;
        bj_gui_show_toolbar (value);
        gconf_client_set_bool (bj_gconf_client (), 
                               GCONF_KEY_SHOW_TOOLBAR,
                               value, NULL);
}

gboolean
bj_get_quick_deal ()
{
        return quick_deals;
}
void
bj_set_quick_deal (gboolean value)
{
        gconf_client_set_bool (bj_gconf_client (), 
                               GCONF_KEY_QUICK_DEAL,
                               value, NULL);
}

gchar *
bj_get_game_variation ()
{
        return g_strdup (game_variation);
}

void
bj_set_game_variation (const gchar *value)
{
        if (game_variation)
                g_free (game_variation);
        game_variation = g_strdup (value);
        gconf_client_set_string (bj_gconf_client (), 
                                 GCONF_KEY_GAME_VARIATION,
                                 game_variation, NULL);
}

gdouble
bj_get_balance ()
{
        return balance_value;
}

void
bj_set_balance (gdouble balance)
{
        balance_value = balance;
        bj_show_balance (balance_value);
        gconf_client_set_float (bj_gconf_client (),
                                GCONF_KEY_BALANCE,
                                balance, NULL);
}

void
bj_adjust_balance (gdouble offset)
{
        gdouble balance;
        balance = bj_get_balance ();
        balance += offset;
        bj_set_balance (balance);
}

static void
bj_gconf_init (GConfClient *client)
{
        gdouble balance;
        gchar *variation_tmp;

        variation_tmp = gconf_client_get_string (client,
                                                 GCONF_KEY_GAME_VARIATION,
                                                 NULL);
        if (variation_tmp) {
                game_variation = g_strdup (variation_tmp);
                g_free (variation_tmp);
        }
        else
                game_variation = g_strdup (DEFAULT_VARIATION);

        balance = gconf_client_get_float (client,
                                          GCONF_KEY_BALANCE,
                                          NULL);
        if (balance)
                balance_value = balance;

        show_probabilities = gconf_client_get_bool (client,
                                                    GCONF_KEY_SHOW_PROBABILITIES,
                                                    NULL);
        quick_deals = gconf_client_get_bool (client, 
                                             GCONF_KEY_QUICK_DEAL,
                                             NULL);
        show_toolbar = gconf_client_get_bool (client, 
                                              GCONF_KEY_SHOW_TOOLBAR,
                                              NULL);

        gconf_client_notify_add (client,
                                 GCONF_KEY_BALANCE,
                                 bj_gconf_balance_cb,
                                 NULL, NULL, NULL);
        gconf_client_notify_add (client,
                                 GCONF_KEY_CARD_STYLE,
                                 bj_gconf_card_style_cb,
                                 NULL, NULL, NULL);
        gconf_client_notify_add (client,
                                 GCONF_KEY_SHOW_PROBABILITIES,
                                 bj_gconf_show_probabilities_cb,
                                 NULL, NULL, NULL);
        gconf_client_notify_add (client,
                                 GCONF_KEY_QUICK_DEAL,
                                 bj_gconf_quick_deal_cb,
                                 NULL, NULL, NULL);
        gconf_client_notify_add (client,
                                 GCONF_KEY_SHOW_TOOLBAR,
                                 bj_gconf_show_toolbar_cb,
                                 NULL, NULL, NULL);
}

int
main (int argc, char *argv [])
{
        gchar* variation = "";
        struct poptOption blackjack_opts[] = {
                {"variation", '\0', POPT_ARG_STRING, NULL, 0, NULL, NULL},
                {NULL, '\0', 0, NULL, 0, NULL, NULL}
        };

        blackjack_opts[0].arg = &variation;
        blackjack_opts[0].descrip = N_("Variation on game rules");
        blackjack_opts[0].argDescrip = N_("NAME");

        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        gnome_program_init ("blackjack", VERSION,
                            LIBGNOMEUI_MODULE, 
                            argc, argv,
                            GNOME_PARAM_POPT_TABLE, blackjack_opts,
                            GNOME_PARAM_APP_DATADIR, DATADIR, NULL);

        gconf_init (argc, argv, NULL);
        gconf_client_add_dir (bj_gconf_client (), GCONF_KEY_DIR,
                              GCONF_CLIENT_PRELOAD_NONE, NULL);

        gtk_widget_push_colormap (gdk_rgb_get_colormap ());

        gchar *icon_path = g_build_filename (GNOME_ICONDIR,
                                             "gnome-blackjack.png",
                                             NULL);

        gnome_window_icon_set_default_from_file (icon_path);
        g_free (icon_path);

        g_signal_connect (gnome_master_client (), "die",
                          G_CALLBACK (bj_quit_app), NULL);

        bj_gconf_init (bj_gconf_client ());

        bj_game_find_rules (variation);

        main_prog (argc, argv);

        return 0;
}
