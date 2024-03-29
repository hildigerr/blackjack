/*
 *  Copyright © 2009 Thomas H.P. Andersen <phomes@gmail.com>
 *
 *  This runtime is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This runtime is distributed in the hope runtime it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this runtime; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef GAMES_GTK_COMPAT_H
#define GAMES_GTK_COMPAT_H

G_BEGIN_DECLS

#ifdef GSEAL_ENABLE
/* Temporary fix from http://live.gnome.org/GnomeGoals/UseGseal */
#undef GTK_OBJECT_FLAGS
#define GTK_OBJECT_FLAGS(i) (GTK_OBJECT (i)->GSEAL(flags))
#endif /* GSEAL_ENABLE */

#if !GTK_CHECK_VERSION (2, 17, 8)
#define gtk_widget_set_allocation(widget, alloc) ((widget)->allocation=*(alloc))
#endif /* GTK < 2.17.8 */

#if !GTK_CHECK_VERSION (2, 17, 7)
#define gtk_widget_get_allocation(widget, alloc) (*(alloc)=(widget)->allocation)
#endif /* GTK < 2.17.7 */

#if !GTK_CHECK_VERSION (2, 17, 5)
#define gtk_widget_get_state(widget) ((widget)->state)
#endif /* GTK < 2.17.5 */

#if !GTK_CHECK_VERSION (2, 13, 4)
#define gtk_widget_get_window(widget) ((widget)->window)
#endif /* GTK < 2.13.4 */

G_END_DECLS

#endif /* !GAMES_GTK_COMPAT_H */
