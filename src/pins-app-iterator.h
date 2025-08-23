/* pins-app-iterator.h
 *
 * Copyright 2024 Fabrizio
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PINS_TYPE_APP_ITERATOR (pins_app_iterator_get_type ())

G_DECLARE_FINAL_TYPE (PinsAppIterator, pins_app_iterator, PINS, APP_ITERATOR,
                      GObject);

PinsAppIterator *pins_app_iterator_new (void);

void pins_app_iterator_load (PinsAppIterator *self);
void pins_app_iterator_create_user_file (PinsAppIterator *self,
                                         gchar *basename,
                                         const gchar *contents,
                                         GError **error);

G_END_DECLS
