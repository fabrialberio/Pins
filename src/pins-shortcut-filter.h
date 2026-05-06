/* pins-shortcut-filter.h
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

#define PINS_TYPE_SHORTCUT_FILTER (pins_shortcut_filter_get_type ())

typedef enum
{
    PINS_SHORTCUT_FILTER_CATEGORY_ALL,
    PINS_SHORTCUT_FILTER_CATEGORY_VISIBLE, // System or edited, aka not hidden.
    PINS_SHORTCUT_FILTER_CATEGORY_EDITED,
    PINS_SHORTCUT_FILTER_CATEGORY_SYSTEM,
    PINS_SHORTCUT_FILTER_CATEGORY_HIDDEN,
    PINS_SHORTCUT_FILTER_CATEGORY_AUTOSTART,
} PinsShortcutFilterCategory;

G_DECLARE_FINAL_TYPE (PinsShortcutFilter, pins_shortcut_filter, PINS,
                      SHORTCUT_FILTER, GObject);

PinsShortcutFilter *pins_shortcut_filter_new (void);

void pins_shortcut_filter_set_model (PinsShortcutFilter *self,
                                     GListModel *model);
void pins_shortcut_filter_set_search (PinsShortcutFilter *self,
                                      const gchar *query);
void pins_shortcut_filter_reset_category (PinsShortcutFilter *self);

G_END_DECLS
