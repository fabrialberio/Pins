/* pins-home-view.h
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

#include "pins-shortcut-iterator.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define PINS_TYPE_HOME_VIEW (pins_home_view_get_type ())

G_DECLARE_FINAL_TYPE (PinsHomeView, pins_home_view, PINS, HOME_VIEW, AdwBin)

void
pins_home_view_set_shortcut_iterator (PinsHomeView *self,
                                      PinsShortcutIterator *shortcut_iterator);

G_END_DECLS
