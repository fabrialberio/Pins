/* pins-app-group.h
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

G_BEGIN_DECLS

#define PINS_TYPE_APP_GROUP (pins_app_group_get_type ())

G_DECLARE_FINAL_TYPE (PinsAppGroup, pins_app_group, PINS, APP_GROUP, GtkBox)

PinsAppGroup *pins_app_group_new (void);
void pins_app_group_set_model (PinsAppGroup *self, GListModel *model,
                               guint offset, guint size);
void pins_app_group_set_offset (PinsAppGroup *self, guint offset);
void pins_app_group_set_size (PinsAppGroup *self, guint size);

G_END_DECLS
