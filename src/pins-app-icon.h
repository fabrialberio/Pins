/* pins-app-icon.h
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

#include "pins-desktop-file.h"

G_BEGIN_DECLS

#define PINS_TYPE_APP_ICON (pins_app_icon_get_type ())

G_DECLARE_FINAL_TYPE (PinsAppIcon, pins_app_icon, PINS, APP_ICON, GtkWidget)

void pins_app_icon_invalidate_cached_key (gchar *icon_key);
void pins_app_icon_set_desktop_file (PinsAppIcon *self,
                                     PinsDesktopFile *desktop_file);

G_END_DECLS
