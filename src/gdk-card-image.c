/* gdk-card-image.c
   Copyright 1998 Free Software Foundation, Inc.

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

/* Authors:   Felix Bellaby <felix@pooh.u-net.com>
              Ryu Changwoo <cwryu@eve.kaist.ac.kr> */

/* The library currently handles options as a collection of a set of
 * integers which index image files within a set of directories holding
 * specialist images that can be assembled to form a deck of cards.
 * The user is presented with a table of option menus which list the 
 * filenames of the images in each directory.
 * This works but is not set in stone. */

#include <config.h>
#include <gnome.h>

#include <gdk-card-image.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>

/* An image file used in building the cards:
 * each file can contain multiple images and/or half images */
typedef struct _GdkCardDeckFile
{
  GdkPixmap *p;
  GdkBitmap *b;
  GdkPixmap *pr;
  GdkBitmap *br;
  guint width;
  guint height;

  gchar* name;
  guint cols;
  guint rows;
  gboolean rotate;

  guint refs;
} GdkCardDeckFile;

/* A directory of image files:
 * all the image files in a directory have the same internal structure */
typedef struct _GdkCardDeckDir
{
  gboolean rotate;
  guint cols;
  guint rows;
  gchar* name;
  guint nfiles;
  GdkCardDeckFile *file;
} GdkCardDeckDir;

enum {
  DIR_BACK,
  DIR_JOKER,
  DIR_HONOR,
  DIR_RANK,
  DIR_SUIT_SMALL,
  DIR_SUIT_MEDIUM,
  DIR_SUIT_LARGE,
  DIR_NUM
};

/* The subdirectories of $(datadir)/pixmaps used by this library */
GdkCardDeckDir dir[DIR_NUM] = {
  {FALSE,  1, 1, "cards/backs/" , 0, NULL },
  {TRUE,   1, 1, "cards/jokers/", 0, NULL },
  {TRUE,   4, 3, "cards/honors/", 0, NULL },
  {TRUE,  14, 2, "cards/ranks/" , 0, NULL },
  {TRUE,   4, 1, "cards/suits_small/" , 0, NULL },
  {TRUE,   4, 1, "cards/suits_medium/" , 0, NULL },
  {TRUE,   4, 1, "cards/suits_large/" , 0, NULL }
};

/* The user chooses which image files to use from each directory */
typedef struct _GdkCardDeckOptionData
{
  gchar* description;
  GdkCardDeckDir* dir;
  gchar* def;
} GdkCardDeckOptionData;

enum {
  OPT_BACK,
  OPT_HONOR,
  OPT_JOKER,
  OPT_RANK,
  OPT_SUIT_S,
  OPT_SUIT_M,
  OPT_SUIT_L,
  OPT_NUM
};

GdkCardDeckOptionData option_data[] = {
  {N_("Card back:"),          &dir[DIR_BACK],        "beige.png"},
  {N_("Honor pictures:"),     &dir[DIR_HONOR],       "bonded.png"},
  {N_("Joker icon:"),         &dir[DIR_JOKER],       "gnome.png"},
  {N_("Rank font:"),          &dir[DIR_RANK],        "bold-09x14.png"},
  {N_("Suit font (small):"),  &dir[DIR_SUIT_SMALL],  "knuth-09x10.png"},
  {N_("Suit font (medium):"), &dir[DIR_SUIT_MEDIUM], "knuth-18x21.png"},
  {N_("Suit font (large):"),  &dir[DIR_SUIT_LARGE],  "knuth-21x25.png"}
};

/* The deck of cards itself */
struct _GdkCardDeck
{
  GtkObject object;

  GdkPixmap **faces[4];
  GdkPixmap *back;
  GdkBitmap *mask;

  guint width;
  guint height;
  guint corner;

  /* Private stuff used to build the images */
  gint* index;
  guint rx, ry;
  guint sx, sy;
  guint xdelta;
  guint ydelta;
  guint x0, x1, x2;
  guint y0, y1, y2, y3, y4, y5, y6, y7, y8;
};

struct _GdkCardDeckClass {
  GtkObjectClass parent_class;
};

static GtkObjectClass *parent_class = NULL;

static gint gdk_card_deck_dir_search (GdkCardDeckDir* dir, gchar* name);
static gboolean gdk_card_deck_file_load (GdkCardDeckFile* file);
static void gdk_card_file_unref (GdkCardDeckFile *file);
static void gdk_card_file_draw (GdkCardDeckFile* file, GdkPixmap* p, 
				GdkGC* gc, 
				guint x, guint y, guint i, guint j);
static void gdk_card_file_draw_r (GdkCardDeckFile* file, GdkPixmap* p, 
				  GdkGC* gc,  
				  guint x, guint y, guint i, guint j);

static GdkPixbuf *gdk_pixbuf_rotate_image_180 (GdkPixbuf * im);

static void 
gdk_card_deck_destroy (GtkObject *o)
{
  GdkCardDeck* w;
  guint i, j;

  g_return_if_fail(o != NULL);
  g_return_if_fail(GDK_IS_CARD_DECK(o));

  w = GDK_CARD_DECK(o);

  for (i = CLUB; i <= SPADE; i++)
    for (j = JOKER; j <= KING; j++)
      g_object_unref (w->faces[i][j]);

  g_object_unref (w->back);
  
  for(i = 0; i < OPT_NUM; i++)
    gdk_card_file_unref (&option_data[i].dir->file[w->index[i]]);

  g_free(w->index);

  (*(GTK_OBJECT_CLASS (parent_class)->destroy))(o);
}

static void
gdk_card_deck_class_init (GdkCardDeckClass *klass)
{
  GtkObjectClass *object_class = (GtkObjectClass*) klass;

  parent_class = gtk_type_class (gtk_object_get_type ());

  object_class->destroy = gdk_card_deck_destroy;
}

GtkTypeInfo gdk_card_deck_info = 
{
  "GdkCardDeck",
  sizeof (GdkCardDeck),
  sizeof (GdkCardDeckClass),
  (GtkClassInitFunc) gdk_card_deck_class_init,
  (GtkObjectInitFunc) NULL,
  NULL,
  NULL,
  (GtkClassInitFunc) NULL
};

GtkType
gdk_card_deck_get_type ()
{
  static GtkType type = 0;

  if (!type)
    type = gtk_type_unique (gtk_object_get_type (), &gdk_card_deck_info);
  return type;
}

static int
is_image (const gchar * name)
{
	const char *type = gnome_vfs_mime_type_from_name (name);

	if (type == NULL ||
	    strncmp (type, "image/", strlen ("image/")) != 0)
		return FALSE;
	else
		return TRUE;

}

static gint
gdk_card_deck_dir_search (GdkCardDeckDir* dir, gchar* name)
{
  guint i;

  if (!dir->file) {
    gchar* dir_name = gnome_program_locate_file (NULL,
		    GNOME_FILE_DOMAIN_APP_PIXMAP,  dir->name, TRUE, NULL);
    GDir * de;
    GList * filelist = NULL;
    GList * node;
    gchar * path;
    const gchar * filename;
    int records = 0;

    if (dir_name == NULL)
      return -1;

    de = g_dir_open (dir_name, 0, NULL);
    while ((filename = g_dir_read_name (de))) {
      if (is_image (filename)) {
        path = g_build_filename (dir_name, filename, NULL);
        filelist = g_list_prepend (filelist, path);
        records++;
      }
    }

    filelist = g_list_sort (filelist, (GCompareFunc)g_utf8_collate);
    
    dir->nfiles = records;
    dir->file = g_new0 (GdkCardDeckFile, dir->nfiles);
    node = filelist;
    for (i = 0; i < dir->nfiles; i++) {
      dir->file[i].name = node->data;
      dir->file[i].cols = dir->cols;
      dir->file[i].rows = dir->rows;
      dir->file[i].rotate = dir->rotate;
      node = g_list_next (node);
    }
    g_dir_close (de);
    g_list_free (filelist);
    g_free (dir_name);
  }

  for (i = 0; i < dir->nfiles; i++)
    if (!strcmp (name, g_basename (dir->file[i].name)))
	return i;

  return -1;
}

/* Stolen from gdeck */
GdkPixbuf *
gdk_pixbuf_rotate_image_180 (GdkPixbuf *pixbuf)
{
  GdkPixbuf *result;
  const int height = gdk_pixbuf_get_height(pixbuf);
  const int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
  const int width = gdk_pixbuf_get_width(pixbuf);
  const int channels = gdk_pixbuf_get_n_channels(pixbuf);
  const int bits_per_sample = gdk_pixbuf_get_bits_per_sample(pixbuf);
  const int bytes_per_pixel = (gdk_pixbuf_get_n_channels(pixbuf)*gdk_pixbuf_get_bits_per_sample(pixbuf))/8;
  guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
  guchar *result_pixels;
  int i, j;
  int row_size;

  result = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(pixbuf),
                          gdk_pixbuf_get_has_alpha(pixbuf),
                          gdk_pixbuf_get_bits_per_sample(pixbuf),
                          width,
                          height);

  result_pixels = gdk_pixbuf_get_pixels(result);

  row_size = width * ((channels * bits_per_sample + 7) / 8);

  for(i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      memcpy(result_pixels+(((height-1)-i)*rowstride) + ((width-1)-j)*bytes_per_pixel, pixels+(i*rowstride) + j*bytes_per_pixel,  bytes_per_pixel);
    }
  }
  return result;
}

static gboolean
gdk_card_deck_file_load (GdkCardDeckFile* file)
{
  g_return_val_if_fail (file != NULL, FALSE);
 
  if (!file->refs) {
    GdkPixbuf *im;
    if (!(im = gdk_pixbuf_new_from_file (file->name, NULL))) 
      return FALSE;
    
    file->width = gdk_pixbuf_get_width (im) / file->cols;
    file->height = gdk_pixbuf_get_height (im) / file->rows;
    
    gdk_pixbuf_render_pixmap_and_mask (im, &file->p, &file->b, 127);
    if (!file->p || !file->b)
      return FALSE;
    
    if (file->rotate) {
      GdkPixbuf *imr = gdk_pixbuf_rotate_image_180 (im);
      gdk_pixbuf_render_pixmap_and_mask (imr, &file->pr, &file->br, 127);
      gdk_pixbuf_unref (imr);	
    }
    
    gdk_pixbuf_unref (im);
  }
  file->refs++;
  return TRUE;
}

static void
gdk_card_file_unref (GdkCardDeckFile* file)
{
  if (!--file->refs) {
    g_object_unref (file->p);
    g_object_unref (file->b);

    if (file->pr) {
      g_object_unref (file->pr);
      g_object_unref (file->br);
    }
  }
}

static void
gdk_card_file_draw (GdkCardDeckFile* file, GdkPixmap* p, GdkGC* gc, 
		    guint x, guint y, guint i, guint j)
{
  guint xp = i*file->width, yp = j*file->height; 

  gdk_gc_set_clip_origin(gc, x - xp, y - yp);
  gdk_gc_set_clip_mask(gc, file->b);
  gdk_window_copy_area((GdkWindow *)p, gc, x, y, 
		       (GdkWindow *)file->p, xp, yp, 
		       file->width, file->height);
}

static void
gdk_card_file_draw_r (GdkCardDeckFile* file, GdkPixmap* p, GdkGC* gc, 
		      guint x, guint y, guint i, guint j)
{
  guint xp = (file->cols - i - 1)*file->width;
  guint yp = (file->rows - j - 1)*file->height; 

  gdk_gc_set_clip_origin(gc, x - xp, y - yp);
  gdk_gc_set_clip_mask(gc, file->br);
  gdk_window_copy_area((GdkWindow *)p, gc, x, y, 
		       (GdkWindow *)file->pr, xp, yp, 
		       file->width, file->height);
}

struct _GtkCardDeckOptionsEdit {
  GtkAlignment container;
  
  GtkOptionMenu **menu;
};

struct _GtkCardDeckOptionsEditClass {
  GtkAlignmentClass parent_class;

  void (* changed) (GtkCardDeckOptionsEdit* w);
};

enum {
  CHANGED,
  N_SIGNALS
};

static GtkAlignmentClass * options_parent_class = NULL;
static gint gtk_card_deck_options_edit_signals[N_SIGNALS] = { 0 };

static void 
gtk_card_deck_options_edit_destroy (GtkObject *o)
{
  GtkCardDeckOptionsEdit* w;
  guint i;

  g_return_if_fail(o != NULL);
  g_return_if_fail(GTK_IS_CARD_DECK_OPTIONS_EDIT(o));

  w = GTK_CARD_DECK_OPTIONS_EDIT(o);

  for(i = 0; i < OPT_NUM; i++)
    gtk_widget_destroy (GTK_WIDGET(w->menu[i]));
  g_free(w->menu);

  (*(GTK_OBJECT_CLASS (parent_class)->destroy))(o);
}

static void
gtk_card_deck_options_edit_class_init (GtkCardDeckOptionsEditClass *klass)
{
  GtkObjectClass *object_class = (GtkObjectClass*) klass;
    
  options_parent_class = gtk_type_class (gtk_widget_get_type ());
  
  gtk_card_deck_options_edit_signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkCardDeckOptionsEditClass, changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  object_class->destroy = gtk_card_deck_options_edit_destroy;
  
  klass->changed = NULL;
}

GTypeInfo gtk_card_deck_options_edit_info = 
{
  sizeof (GtkCardDeckOptionsEditClass),
  NULL,
  NULL,
  (GClassInitFunc) gtk_card_deck_options_edit_class_init,
  NULL,
  NULL,
  sizeof (GtkCardDeckOptionsEdit),
  0,
  NULL,
  NULL
};

guint
gtk_card_deck_options_edit_get_type ()
{
  static guint type = 0;

  if (!type)
    type = g_type_register_static (GTK_TYPE_ALIGNMENT, "GtkCardDeckOptionsEdit",
                                   &gtk_card_deck_options_edit_info, 0);
  return type;
}

static void calculate_dimensions (GdkCardDeck* deck, GdkCardDeckFile** file);
static void make_rounded_rectangle (GdkWindow *window, GdkGC **gc, 
				    GdkPixmap **pixmap, GdkBitmap **bitmap,
				    guint w, guint h, guint c);
static void draw_rank (GdkCardDeck* deck, GdkCardDeckFile* file, 
		       GdkGC *gc, GdkPixmap *p, guint suit, guint rank);
static void draw_small_suit (GdkCardDeck* deck, GdkCardDeckFile* file,
			     GdkGC *gc, GdkPixmap *p, guint suit);
static void make_suit (GdkCardDeck* deck, GdkCardDeckFile** file,
		       GdkWindow *window, GdkGC *gc, 
		       GdkPixmap ***sp, guint suit);

static void
calculate_dimensions (GdkCardDeck* deck, GdkCardDeckFile** file)
{
  deck->xdelta = (file[OPT_HONOR]->width-4)/3;
  deck->width = file[OPT_HONOR]->width + 2*deck->xdelta;
  deck->height = 2*file[OPT_HONOR]->height - 1 + 2*deck->xdelta;
  deck->corner = ((2*deck->xdelta)/5)*2;

  deck->ydelta = deck->height / 10;  

  deck->x1 = (deck->width - file[OPT_SUIT_M]->width) / 2;
  deck->y3 = (deck->height - file[OPT_SUIT_M]->height) / 2;

  deck->x0 = deck->x1 - deck->xdelta; 
  deck->x2 = deck->x1 + deck->xdelta; 

  deck->y0 = deck->y3 - 3 * deck->ydelta;
  deck->y1 = deck->y3 - 2 * deck->ydelta;
  deck->y2 = deck->y3 - 1 * deck->ydelta;
  deck->y4 = deck->y3 + 1 * deck->ydelta;
  deck->y5 = deck->y3 + 2 * deck->ydelta;
  deck->y6 = deck->y3 + 3 * deck->ydelta;

  deck->y7 = deck->y3 - (3 * deck->ydelta) / 2;
  deck->y8 = deck->y3 + (3 * deck->ydelta) / 2;

  deck->sx = deck->x0 + (file[OPT_SUIT_M]->width - file[OPT_SUIT_S]->width) / 2 - deck->xdelta; 
  deck->sy = deck->y0 + (file[OPT_SUIT_M]->width - file[OPT_SUIT_S]->width) / 2;
  deck->rx = deck->sx + (file[OPT_SUIT_S]->width - file[OPT_RANK]->width) / 2;
  deck->ry = deck->sy - file[OPT_RANK]->height - 1;
  if (deck->ry < 6) {
    deck->ry = 6;
    deck->sy = deck->ry + file[OPT_RANK]->height + 1;
  }
}

static void
make_rounded_rectangle (GdkWindow *window, GdkGC **gc, 
			GdkPixmap **pixmap, GdkBitmap **bitmap,
			guint w, guint h, guint c)
{
  GdkColor masked = {0, 0, 0, 0}, unmasked = {1, 65535, 65535, 65535};
  GdkColor white, black;

  *bitmap = gdk_pixmap_new (window, w, h, 1);  
  *gc = gdk_gc_new (*bitmap);
  gdk_gc_set_foreground (*gc, &masked);
  gdk_draw_rectangle (*bitmap, *gc, TRUE, 0, 0, -1, -1);
  gdk_gc_set_foreground (*gc, &unmasked);
  gdk_draw_arc (*bitmap, *gc, FALSE, w-c-1, 0, c, c, 0*64, 90*64);
  gdk_draw_arc (*bitmap, *gc, FALSE, 0, 0, c, c, 90*64, 90*64);
  gdk_draw_arc (*bitmap, *gc, FALSE, 0, h-c-1, c, c, 180*64, 90*64);
  gdk_draw_arc (*bitmap, *gc, FALSE, w-c-1, h-c-1, c, c, 270*64, 90*64);
  gdk_draw_arc (*bitmap, *gc, TRUE, w-c-1, 0, c, c, 0*64, 90*64);
  gdk_draw_arc (*bitmap, *gc, TRUE, 0, 0, c, c, 90*64, 90*64);
  gdk_draw_arc (*bitmap, *gc, TRUE, 0, h-c-1, c, c, 180*64, 90*64);
  gdk_draw_arc (*bitmap, *gc, TRUE, w-c-1, h-c-1, c, c, 270*64, 90*64);
  gdk_draw_rectangle (*bitmap, *gc, TRUE, c/2, 0, w-c, h);
  gdk_draw_rectangle (*bitmap, *gc, TRUE, 0, c/2, w, h-c);
  gdk_gc_unref(*gc);

  gdk_color_white (gdk_window_get_colormap(window), &white);
  gdk_color_black (gdk_window_get_colormap(window), &black);

  *pixmap = gdk_pixmap_new (window, w, h, -1);  
  *gc = gdk_gc_new (*pixmap);
  gdk_gc_set_foreground (*gc, &white);
  gdk_draw_rectangle (*pixmap, *gc, TRUE, 0, 0, -1, -1);
  gdk_gc_set_foreground (*gc, &black);
  gdk_draw_arc (*pixmap, *gc, FALSE, w-c-1, 0, c, c, 0*64, 90*64);
  gdk_draw_arc (*pixmap, *gc, FALSE, 0, 0, c, c, 90*64, 90*64);
  gdk_draw_arc (*pixmap, *gc, FALSE, 0, h-c-1, c, c, 180*64, 90*64);
  gdk_draw_arc (*pixmap, *gc, FALSE, w-c-1, h-c-1, c, c, 270*64, 90*64);
  gdk_draw_rectangle (*pixmap, *gc, FALSE, 0, 0, w - 1, h - 1);
}

static void
copy_card (GdkCardDeck* deck, GdkWindow* w, GdkGC *gc, 
	   GdkPixmap **p, GdkPixmap *q)
{
  *p = gdk_pixmap_new (w, deck->width, deck->height, -1);  
  gdk_gc_set_clip_mask(gc, NULL);
  gdk_window_copy_area((GdkWindow *)*p, gc, 0, 0,
		       (GdkWindow *)q, 0, 0, deck->width, deck->height);
}

static void
draw_rank (GdkCardDeck* deck, GdkCardDeckFile* file, 
	   GdkGC *gc, GdkPixmap *p, guint color, guint rank)
{
  guint cx, cy;

  cx = deck->width - deck->rx - file->width;
  cy = deck->height - deck->ry - file->height;

  gdk_card_file_draw (file, p, gc, deck->rx, deck->ry, rank, color);
  gdk_card_file_draw_r (file, p, gc, cx, cy, rank, color);
  gdk_card_file_draw (file, p, gc, cx, deck->ry, rank, color);
  gdk_card_file_draw_r (file, p, gc, deck->rx, cy, rank, color);
}

static void
draw_small_suit (GdkCardDeck* deck, GdkCardDeckFile* file, 
		 GdkGC *gc, GdkPixmap *p, guint suit)
{
  guint cx, cy;

  cx = deck->width - file->width - deck->sx;
  cy = deck->height - file->height - deck->sy;

  gdk_card_file_draw (file, p, gc, deck->sx, deck->sy, suit, 0);
  gdk_card_file_draw_r (file, p, gc, cx, cy, suit, 0);
  gdk_card_file_draw (file, p, gc, cx, deck->sy, suit, 0);
  gdk_card_file_draw_r (file, p, gc, deck->sx, cy, suit, 0);
}

static void
make_suit (GdkCardDeck* deck, GdkCardDeckFile** file, GdkWindow *window, 
	   GdkGC *gc, GdkPixmap ***sp, guint suit)
{
  guint rank;

  *sp = g_new0(GdkPixmap*, KING + 1);
  
  copy_card (deck, window, gc, &(*sp)[ACE], deck->back);
  /* we don't want the small_suit on the JOKER */
  copy_card (deck, window, gc, &(*sp)[JOKER], (*sp)[ACE]);
  draw_small_suit(deck, file[OPT_SUIT_S], gc, (*sp)[ACE], suit);
  
  copy_card (deck, window, gc, &(*sp)[TWO], (*sp)[ACE]);
  copy_card (deck, window, gc, &(*sp)[FOUR], (*sp)[ACE]);
  copy_card (deck, window, gc, &(*sp)[JACK], (*sp)[ACE]);
  copy_card (deck, window, gc, &(*sp)[QUEEN], (*sp)[ACE]);
  copy_card (deck, window, gc, &(*sp)[KING], (*sp)[ACE]);
  
  gdk_card_file_draw (file[OPT_SUIT_L], (*sp)[ACE], gc, 
		      (deck->width - file[OPT_SUIT_L]->width)/2, 
		      (deck->height - file[OPT_SUIT_L]->height)/2, suit, 0);
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[TWO], gc, 
		      deck->x1, deck->y0, suit, 0);
  gdk_card_file_draw_r (file[OPT_SUIT_M], (*sp)[TWO], gc, 
			deck->x1, deck->y6, suit, 0);
  
  copy_card (deck, window, gc, &(*sp)[THREE], (*sp)[TWO]);
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[THREE], gc, 
		      deck->x1, deck->y3, suit, 0);
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[FOUR], gc, 
		      deck->x0, deck->y0, suit, 0);
  gdk_card_file_draw_r (file[OPT_SUIT_M], (*sp)[FOUR], gc, 
			deck->x2, deck->y6, suit, 0);
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[FOUR], gc, 
		      deck->x2, deck->y0, suit, 0);
  gdk_card_file_draw_r (file[OPT_SUIT_M], (*sp)[FOUR], gc, 
			deck->x0, deck->y6, suit, 0);
  
  copy_card (deck, window, gc, &(*sp)[FIVE], (*sp)[FOUR]);
  copy_card (deck, window, gc, &(*sp)[SIX], (*sp)[FOUR]);
  copy_card (deck, window, gc, &(*sp)[NINE], (*sp)[FOUR]);
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[FIVE], gc, 
		      deck->x1, deck->y3, suit, 0);
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[SIX], gc, 
		      deck->x0, deck->y3, suit, 0);
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[SIX], gc, 
		      deck->x2, deck->y3, suit, 0);
  
  copy_card (deck, window, gc, &(*sp)[SEVEN], (*sp)[SIX]);
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[SEVEN], gc, 
		      deck->x1, deck->y7, suit, 0);
  
  copy_card (deck, window, gc, &(*sp)[EIGHT], (*sp)[SEVEN]);
  
  gdk_card_file_draw_r (file[OPT_SUIT_M], (*sp)[EIGHT], gc, 
			deck->x1, deck->y8, suit, 0);
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[NINE], gc, 
		      deck->x0, deck->y2, suit, 0);
  gdk_card_file_draw_r (file[OPT_SUIT_M], (*sp)[NINE], gc, 
			deck->x2, deck->y4, suit, 0);
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[NINE], gc, 
		      deck->x2, deck->y2, suit, 0);
  gdk_card_file_draw_r (file[OPT_SUIT_M], (*sp)[NINE], gc, 
			deck->x0, deck->y4, suit, 0);
  
  copy_card (deck, window, gc, &(*sp)[TEN], (*sp)[NINE]);    
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[NINE], gc, 
		      deck->x1, deck->y3, suit, 0);
  
  gdk_card_file_draw (file[OPT_SUIT_M], (*sp)[TEN], gc, 
		      deck->x1, deck->y1, suit, 0);
  gdk_card_file_draw_r (file[OPT_SUIT_M], (*sp)[TEN], gc, 
			deck->x1, deck->y5, suit, 0);
  
  gdk_card_file_draw (file[OPT_HONOR], (*sp)[JACK], gc, 
		      (deck->width - file[OPT_HONOR]->width)/2, 
		      (deck->height + 1)/2 - file[OPT_HONOR]->height,
		      suit, JACK-JACK);
  gdk_card_file_draw_r (file[OPT_HONOR], (*sp)[JACK], gc, 
			(deck->width - file[OPT_HONOR]->width)/2, 
			(deck->height + 1)/2 - 1,
			suit, JACK-JACK);
  
  gdk_card_file_draw (file[OPT_HONOR], (*sp)[QUEEN], gc, 
		      (deck->width - file[OPT_HONOR]->width)/2, 
		      (deck->height + 1)/2 - file[OPT_HONOR]->height,
		      suit, QUEEN-JACK);
  gdk_card_file_draw_r (file[OPT_HONOR], (*sp)[QUEEN], gc, 
			(deck->width - file[OPT_HONOR]->width)/2, 
			(deck->height + 1)/2 - 1,
			suit, QUEEN-JACK);
  
  gdk_card_file_draw (file[OPT_HONOR], (*sp)[KING], gc, 
		      (deck->width - file[OPT_HONOR]->width)/2, 
		      (deck->height + 1)/2 - file[OPT_HONOR]->height,
		      suit, KING-JACK);
  gdk_card_file_draw_r (file[OPT_HONOR], (*sp)[KING], gc, 
			(deck->width - file[OPT_HONOR]->width)/2, 
			(deck->height + 1)/2 - 1,
			suit, KING-JACK);
  
  gdk_card_file_draw (file[OPT_JOKER], (*sp)[JOKER], gc, 
		      (deck->width - file[OPT_JOKER]->width)/2, 
		      (deck->height + 1)/2 - file[OPT_JOKER]->height, 0, 0);
  gdk_card_file_draw_r (file[OPT_JOKER], (*sp)[JOKER], gc, 
			(deck->width - file[OPT_JOKER]->width)/2, 
			(deck->height + 1)/2 - 1, 0, 0);
  
  for(rank = JOKER; rank <= KING; rank++)
    draw_rank(deck, file[OPT_RANK], gc, (*sp)[rank], 
	      (suit >> 1) ^ (suit & 1), rank);
}

static void
resolve_options (GdkCardDeckOptionData* option_data, 
		 GdkCardDeckOptions deck_options, 
		 gint* index)
{
  gchar** name;
  guint n, i;

  if (deck_options) 
    gnome_config_make_vector (deck_options, &n, &name);
  else
    n = 0;

  for (i = 0; i < OPT_NUM; i++, index++, option_data++, name++) {
    GdkCardDeckDir* dir = option_data->dir;

    /* Compiling in the defaults would be safer but would break the 
     * current configuration model. Hmm... */
    if (i >= n || (*index = gdk_card_deck_dir_search (dir, *name)) == -1) 
      *index = gdk_card_deck_dir_search (dir, option_data->def);
  }
}

GtkObject *
gdk_card_deck_new (GdkWindow *window, GdkCardDeckOptions deck_options)
{
  GdkCardDeck* w;
  GdkCardDeckFile** file;
  GdkGC* gc;
  guint i;

  g_return_val_if_fail (window != NULL, NULL);

  w = gtk_type_new(gdk_card_deck_get_type());

  w->index = g_new(gint, OPT_NUM);
  file = g_new(GdkCardDeckFile*, OPT_NUM);
  
  resolve_options (option_data, deck_options, w->index);

  for (i = 0; i < OPT_NUM; i++) {
    g_return_val_if_fail(w->index[i] != -1, NULL);
    file[i] = &option_data[i].dir->file[w->index[i]];
    gdk_card_deck_file_load (file[i]);
  }

  calculate_dimensions (w, file);

  make_rounded_rectangle (window, &gc, &w->back, &w->mask, 
			  w->width, w->height, w->corner);

  for (i = CLUB; i <= SPADE; i++) 
    make_suit(w, file, window, gc, &w->faces[i], i);

  gdk_card_file_draw (file[OPT_BACK], w->back, gc, 
		      (w->width - file[OPT_BACK]->width)/2, 
		      (w->height - file[OPT_BACK]->height)/2, 0, 0);
  gdk_gc_unref(gc);

  return GTK_OBJECT (w);
}

GdkCardDeckOptions 
gdk_card_deck_get_options (GdkCardDeck* deck)
{
  guint i = 0;
  gint* index = deck->index;
  gchar** name = g_new0(gchar*, OPT_NUM);
  GdkCardDeckOptions deck_options;

  for(i = 0; i < OPT_NUM; i++, index++)
    name[i] = g_strdup (g_basename (option_data[i].dir->file[*index].name));

  deck_options = gnome_config_assemble_vector (OPT_NUM, 
					       (const gchar* const*) name);
  g_free (name);
  return deck_options;
}

GdkPixmap* 
gdk_card_deck_face (GdkCardDeck* deck, GdkCardSuit suit, GdkCardRank rank)
{
  return deck->faces[suit][rank];
}

GdkPixmap* 
gdk_card_deck_joker (GdkCardDeck* deck, GdkCardSuit suit)
{
  return deck->faces[suit][0];
}

GdkPixmap* 
gdk_card_deck_back (GdkCardDeck* deck)
{
  return deck->back;
}

GdkBitmap* 
gdk_card_deck_mask (GdkCardDeck* deck)
{
  return deck->mask;
}

static void 
changed(GtkCardDeckOptionsEdit* w)
{
  gtk_signal_emit(GTK_OBJECT(w), 
		  gtk_card_deck_options_edit_signals[CHANGED],
		  NULL);
}

void          
gtk_card_deck_options_edit_set (GtkCardDeckOptionsEdit* w,
				GdkCardDeckOptions deck_options)
{
  guint i;
  gint* index = g_new(gint, OPT_NUM);

  resolve_options (option_data, deck_options, index);

  for (i = 0; i < OPT_NUM; i++)
    gtk_option_menu_set_history (w->menu[i], index[i]);
}

GdkCardDeckOptions 
gtk_card_deck_options_edit_get (GtkCardDeckOptionsEdit* w)
{
  guint i,j;
  gchar** name = g_new0(gchar*, OPT_NUM);
  GdkCardDeckOptions deck_options;

  for(i = 0; i < OPT_NUM; i++) {
    j = gtk_option_menu_get_history (GTK_OPTION_MENU (w->menu[i]));
    g_free (name[i]); 
    name[i] = g_strdup ( g_basename (option_data[i].dir->file[j].name));
  }

  deck_options = gnome_config_assemble_vector (OPT_NUM, 
					       (const gchar* const*) name);
  g_free (name);
  return deck_options;
}

GtkWidget* 
gtk_card_deck_options_edit_new (void)
{
  GtkCardDeckOptionsEdit* w;
  GtkAlignment * a;
  GtkWidget *table;
  guint i, j;
  
  w = gtk_type_new(gtk_card_deck_options_edit_get_type());
  a = GTK_ALIGNMENT (w);

  gtk_alignment_set (a, 0.5, 0.5, 1.0, 1.0);
  
  w->menu = g_new(GtkOptionMenu*, OPT_NUM);

  table = gtk_table_new(OPT_NUM, 2, FALSE);
  gtk_container_border_width (GTK_CONTAINER (table), GNOME_PAD_SMALL);

  gtk_container_add (GTK_CONTAINER (w), table);
  
  for (i = 0; i < OPT_NUM; i++) {
    GtkWidget* label = gtk_label_new(_(option_data[i].description));
    GtkWidget* menu = gtk_menu_new();

    w->menu[i] = GTK_OPTION_MENU (gtk_option_menu_new ());

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, i, i+1, 
		     0, 0, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET (w->menu[i]), 1, 2, i, i+1, 
		     GTK_EXPAND|GTK_FILL, GTK_EXPAND|GTK_FILL, 
		     GNOME_PAD_SMALL, GNOME_PAD_SMALL);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    for(j = 0; j < option_data[i].dir->nfiles; j++) {
      gchar* filename = option_data[i].dir->file[j].name;
      GtkWidget *menu_item = 
	gtk_menu_item_new_with_label (g_basename (filename));
      gtk_signal_connect_object (GTK_OBJECT(menu_item), "activate", 
				 GTK_SIGNAL_FUNC (changed), GTK_OBJECT(w));
      gtk_menu_shell_append (GTK_MENU_SHELL(menu), menu_item);
    }
    gtk_widget_show_all (menu);

    gtk_option_menu_set_menu (GTK_OPTION_MENU (w->menu[i]), menu);
  }
  gtk_card_deck_options_edit_set (w, NULL);

  return GTK_WIDGET (w);
}
