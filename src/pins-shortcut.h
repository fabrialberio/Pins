/* pins-shortcut.h
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

#include <gio/gio.h>
#include <glib/gi18n.h>

G_BEGIN_DECLS

#define PINS_TYPE_SHORTCUT (pins_shortcut_get_type ())

#define PINS_SHORTCUT_SUFFIX ".desktop"
#define PINS_SHORTCUT_DEFAULT_CONTENT                                         \
    g_strconcat ("[Desktop Entry]\nName=", _ ("New application"),             \
                 "\nType=", G_KEY_FILE_DESKTOP_TYPE_APPLICATION,              \
                 "\nIcon=", "application-x-executable",                       \
                 "\nExec=", _ ("notify-send \"Hello world!\""), NULL)

G_DECLARE_FINAL_TYPE (PinsShortcut, pins_shortcut, PINS, SHORTCUT, GObject);

PinsShortcut *pins_shortcut_new_full (GFile *user_file, GFile *system_file,
                                      GError **error);
PinsShortcut *pins_shortcut_new (GFile *file, GError **error);

gboolean pins_shortcut_is_user_only (PinsShortcut *self);
gboolean pins_shortcut_is_user_edited (PinsShortcut *self);
gboolean pins_shortcut_is_autostart (PinsShortcut *self);
gboolean pins_shortcut_is_shown (PinsShortcut *self);

void pins_shortcut_save (PinsShortcut *self, GError **error,
                         gboolean remove_unedited_user_files);
void pins_shortcut_trash (PinsShortcut *self);
void pins_shortcut_set_autostart (PinsShortcut *self, gboolean value);

gchar *pins_shortcut_get_desktop_id (PinsShortcut *self);
GFile *pins_shortcut_get_user_file (PinsShortcut *self);
gchar **pins_shortcut_get_keys (PinsShortcut *self);
gchar **pins_shortcut_get_locales (PinsShortcut *self);

gboolean pins_shortcut_get_boolean (PinsShortcut *self, const gchar *key);
gchar *pins_shortcut_get_string (PinsShortcut *self, const gchar *key);
void pins_shortcut_set_boolean (PinsShortcut *self, const gchar *key,
                                const gboolean value);
void pins_shortcut_set_string (PinsShortcut *self, const gchar *key,
                               const gchar *value);

gchar *pins_shortcut_get_locale_for_key (PinsShortcut *self, const gchar *key);
gboolean pins_shortcut_has_backup_for_key (PinsShortcut *self,
                                           const gchar *key);
gboolean pins_shortcut_has_key (PinsShortcut *self, const gchar *key);
gboolean pins_shortcut_is_key_edited (PinsShortcut *self, const gchar *key);
void pins_shortcut_reset_key (PinsShortcut *self, const gchar *key);

G_END_DECLS
