// -*- mode:C++; tab-width:2; c-basic-offset:2; indent-tabs-mode:nil -*-
/* Blackjack - splash.cpp
 * Copyright (C) 2003 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 1998 Felix Bellaby <felix@pooh.u-net.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "splash.h"
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>

static GtkWidget* progress = NULL;
static GtkWidget* label = NULL;
static GtkWidget* splash = NULL;
static gboolean waiting_for_expose = FALSE;

void 
splash_update (gchar* text, gfloat percent)
{
  if (label != NULL
      && progress != NULL)
    {
      gtk_label_set_text (GTK_LABEL (label), text);
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), percent); 
      while (gtk_events_pending ())
        gtk_main_iteration ();
    }
}

static void
splash_destroyed (GtkWidget *w)
{
  splash = NULL;
  label = NULL;
  progress = NULL;
  if (waiting_for_expose)
    {
      waiting_for_expose = FALSE;
      gtk_main_quit ();
    }
}

static gboolean
expose_event (GtkWidget *w, GdkEventExpose *event)
{
  if (waiting_for_expose)
    {
      waiting_for_expose = FALSE;
      gtk_main_quit ();
    }
}


void 
splash_new ()
{
  gchar* image_file;
  GtkWidget* splash_pixmap = NULL;
  GtkWidget* vbox;

  image_file = gnome_program_locate_file 
    (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
     "blackjack/blackjack-splash.png", TRUE, NULL);

  if (image_file != NULL)
    splash_pixmap = gtk_image_new_from_file (image_file);

  g_free (image_file);

  progress = gtk_progress_bar_new ();
  label = gtk_label_new ("");

  splash = gtk_dialog_new ();

  gtk_window_set_position (GTK_WINDOW(splash), 
                           GTK_WIN_POS_CENTER);
  gtk_window_set_title (GTK_WINDOW (splash), _("Blackjack"));
  gtk_window_set_resizable (GTK_WINDOW (splash), FALSE);
  gnome_window_icon_set_from_default (GTK_WINDOW (splash));
  g_signal_connect (GTK_OBJECT (splash), "destroy",
                    GTK_SIGNAL_FUNC (splash_destroyed),
                    NULL);

  vbox = GTK_DIALOG(splash)->vbox;
  
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);

  if (splash_pixmap != NULL)
    gtk_box_pack_start (GTK_BOX (vbox), splash_pixmap, FALSE, FALSE, 
                        GNOME_PAD_SMALL);
  gtk_box_pack_end (GTK_BOX (vbox), progress, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_box_pack_end (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  
  gtk_widget_show_all (splash);

  /* Give window manager time to map the window */
  if (splash_pixmap != NULL)
    {
      g_signal_connect (GTK_OBJECT (splash_pixmap), "expose_event",
                        GTK_SIGNAL_FUNC (expose_event), NULL);
      waiting_for_expose = TRUE;
      gtk_main ();
      waiting_for_expose = FALSE;
    }
}

void 
splash_destroy ()
{
  if (splash != NULL)
    {
      gtk_widget_hide (splash);
      gtk_widget_destroy (splash);
    }
}

