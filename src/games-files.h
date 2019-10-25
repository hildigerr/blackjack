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

#ifndef GAMES_FILES_H
#define GAMES_FILES_H

G_BEGIN_DECLS

#include <gtk/gtk.h>

typedef struct _GamesFileList GamesFileList;
typedef struct _GamesFileListClass GamesFileListClass;

struct _GamesFileList {
  GObject parent;

  GList * list;
};

struct _GamesFileListClass {
  GObjectClass parent;
};

#define GAMES_FILE_LIST_TYPE (games_file_list_get_type ())

GType games_file_list_get_type (void);

GamesFileList * games_file_list_new (gchar * glob, ...);
GamesFileList * games_file_list_new_images (gchar * path1, ...);

void games_file_list_transform_basename (GamesFileList * list);

void games_file_list_for_each (GamesFileList * filelist, GFunc function, 
			       gpointer userdata);

gchar * games_file_list_get_nth (GamesFileList * filelist, gint n);

enum {
  GAMES_FILE_LIST_REMOVE_EXTENSION    = 1 << 0,
  GAMES_FILE_LIST_REPLACE_UNDERSCORES = 1 << 1,
};

GtkWidget * games_file_list_create_widget (GamesFileList * filelist, gchar * selection, guint flags);

G_END_DECLS

#endif /* GAMES_FILES_H */