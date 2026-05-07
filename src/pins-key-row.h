/* pins-key-row.h
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

#include <adwaita.h>

#include "pins-shortcut.h"

G_BEGIN_DECLS

#define PINS_TYPE_KEY_ROW (pins_key_row_get_type ())

G_DECLARE_FINAL_TYPE (PinsKeyRow, pins_key_row, PINS, KEY_ROW, AdwEntryRow)

PinsKeyRow *pins_key_row_new (void);

void pins_key_row_set_key (PinsKeyRow *self, PinsShortcut *shortcut,
                           gchar *key, gchar **locales);
gchar *pins_key_row_get_key (PinsKeyRow *self);

void pins_key_row_set_locale (PinsKeyRow *self, gchar *selected_locale);

G_END_DECLS
