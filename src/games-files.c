/* gdk-card-image.c
   Copyright 2003 Callum McKenzie

   This library is free software; you can redistribute it and'or modify
   it under the terms of the GNU Library General Public License as published 
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Authors:   Callum McKenzie <callum@physics.otago.ac.nz> */

/* The API in this file is best described as "raw and wriggling". They
 * are all meant ot be specialised routines to collect lists of files
 * that a game might be interested in. I'm writing them as I perceive
 * and so some of them should be replaced or removed eventually. */

#include <gtk/gtk.h>
#include <stdarg.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "games-files.h"

static GamesFileList * games_file_list_new_internal (gchar * glob, va_list path)
{
  GPatternSpec * filespec = g_pattern_spec_new (glob);
  gchar * pathelement;
  GList * list = NULL;
  GDir * dir;
  const gchar * filename;
  gchar * fullname;

  while ((pathelement = va_arg (path, gchar *)) != NULL) {
    dir = g_dir_open (pathelement, 0, NULL);
    if (dir != NULL) {
      while ((filename = g_dir_read_name (dir)) != NULL) {
	if (g_pattern_match_string (filespec, filename)) {
	  fullname = g_build_filename (pathelement, filename, NULL);
	  if (g_file_test (fullname, G_FILE_TEST_IS_REGULAR)) {
	    list = g_list_append (list, fullname);
	  } else g_free (fullname);
	}
      }
      g_dir_close (dir);
    }
  }

  g_pattern_spec_free (filespec);

  return (GamesFileList *)list;
}

/* This function takes a glob and a NULL terminated list of paths 
 * and finds all files in the path matching the glob. Only regular 
 * files are returned. */
/* The arguments are the filespec followed by a null-terminated list 
 * of paths. */
/* The caller must free the list. */
GamesFileList * games_file_list_new (gchar * glob, ...)
{
  GamesFileList * list;
  va_list paths;

  va_start (paths, glob);
  list = games_file_list_new_internal (glob, paths);
  va_end (paths);

  return list;
}

/* Transform the list of files to be only the basenames. */
void games_file_list_transform_basename (GamesFileList * list)
{
  GList * current;
  gchar * shortname;

  current = (GList *)list;
  while (current) {
    shortname = g_path_get_basename ((gchar *) current->data);
    g_free (current->data);
    (gchar *) current->data = shortname;
    current = g_list_next (current);
  }
}

/* The same as games_file_list_new, but returning a list of short names rather than 
 * the fully qualified version. */
GamesFileList * games_file_list_new_short (gchar * glob, ...)
{
  GamesFileList *list;
  va_list paths;
  
  va_start (paths, glob);
  list = games_file_list_new_internal (glob, paths);
  va_end (paths);
  
  games_file_list_transform_basename (list);

  return list;
}

GSList * image_suffix_list = NULL;
GStaticMutex image_suffix_mutex = G_STATIC_MUTEX_INIT;

/* We only want to initilise the list of suffixes once, this is
 * the function that does it. It might even be thread safe, not that
 * any of the games are in a position to test this ... */
static void games_image_suffix_list_init (void)
{
  GSList * pixbuf_formats;
  GSList * element;
  GdkPixbufFormat * formats;
  gchar ** suffices;
  gchar ** suffix;

  g_static_mutex_lock (&image_suffix_mutex);
  
  if (image_suffix_list)
    return;

  pixbuf_formats = gdk_pixbuf_get_formats ();

  /* Search through the list of formats for the suffices. */
  element = pixbuf_formats;
  while (element) {
    formats = element->data;
    suffices = gdk_pixbuf_format_get_extensions (formats);

    suffix = suffices;
    while (*suffix) {
      image_suffix_list = g_slist_append (image_suffix_list, g_strdup_printf (".%s", *suffix));
      suffix++;
    }

    g_strfreev (suffices);

    element = g_slist_next (element);
  }

  pixbuf_formats = gdk_pixbuf_get_formats ();

  g_slist_free (pixbuf_formats);

  g_static_mutex_unlock (&image_suffix_mutex);
}

static GList * games_file_list_new_images_single (gchar * directory)
{
  GDir * dir;
  GList * list = NULL;
  const gchar * filename;
  gchar * fullname;
  GSList * suffix;

  dir = g_dir_open (directory, 0, NULL);
  if (!dir)
    return NULL;

  games_image_suffix_list_init ();
  
  while ((filename = g_dir_read_name (dir)) != NULL) {
    suffix = image_suffix_list;
    while (suffix) {
      if (g_str_has_suffix (filename, suffix->data)) {
	fullname = g_build_filename (directory, filename, NULL);
	if (g_file_test (fullname, G_FILE_TEST_IS_REGULAR)) {
	  list = g_list_append (list, fullname);
	} else g_free (fullname);
	break;
      } 
      suffix = g_slist_next (suffix);
    }
  }

  g_dir_close (dir);

  return list;
}

/* Get a list of files in the NULL terminated list of directories which are loadable by 
 * GdkPixbuf. Files are identified by their extension. */
GamesFileList * games_file_list_new_images (gchar * path1, ...)
{
  GList * list;
  gchar * pathentry;
  va_list paths;

  list = games_file_list_new_images_single (path1);
  va_start (paths, path1);
  while ((pathentry = va_arg (paths, gchar *)) != NULL) {
    list = g_list_concat (list, games_file_list_new_images_single (pathentry));
  }
  va_end (paths);

  return (GamesFileList *)list;
}

/* Free a file list.  */
void games_file_list_free (GamesFileList * list)
{
  g_list_foreach ((GList *)list, (GFunc) g_free, NULL);
  g_list_free (list);  
}

/* Create a gtk_combo_box with the given list of strings as the
   entries. If selection is given and it matches a list item then that
   item is selected as the default. If selection == NULL then nothing
   is selected. */
GtkWidget * games_file_list_create_widget (GamesFileList * gamesfilelist, gchar * selection)
{
  gint itemno;
  GtkComboBox *widget;
  gchar *visible, *string;
  GList * filelist = (GList *) gamesfilelist;

  widget = GTK_COMBO_BOX (gtk_combo_box_new_text ());

  itemno = 0;
  while (filelist) {
    string = (gchar *) filelist->data;
    visible = g_strdup (string);

    gtk_combo_box_append_text (widget, visible);
    if (selection && (! g_utf8_collate (string, selection))) {
      gtk_combo_box_set_active (widget, itemno);      
    }

    itemno++;
    filelist = g_list_next (filelist);
  }

  return GTK_WIDGET (widget);
}
