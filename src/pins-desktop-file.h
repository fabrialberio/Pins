/* pins-desktop-file.h
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

#define PINS_TYPE_DESKTOP_FILE (pins_desktop_file_get_type ())

#define PINS_DESKTOP_FILE_SUFFIX ".desktop"
#define PINS_DESKTOP_FILE_DEFAULT_CONTENT                                     \
    g_strconcat ("[Desktop Entry]\nName=", _ ("New application"),             \
                 "\nType=", G_KEY_FILE_DESKTOP_TYPE_APPLICATION,              \
                 "\nIcon=", "application-x-executable",                       \
                 "\nExec=", _ ("notify-send \"Hello world!\""), NULL)

G_DECLARE_FINAL_TYPE (PinsDesktopFile, pins_desktop_file, PINS, DESKTOP_FILE,
                      GObject);

PinsDesktopFile *pins_desktop_file_new_full (GFile *user_file,
                                             GFile *system_file,
                                             GError **error);
PinsDesktopFile *pins_desktop_file_new (GFile *file, GError **error);

gboolean pins_desktop_file_is_user_only (PinsDesktopFile *self);
gboolean pins_desktop_file_is_user_edited (PinsDesktopFile *self);
gboolean pins_desktop_file_is_autostart (PinsDesktopFile *self);
gboolean pins_desktop_file_is_shown (PinsDesktopFile *self);

void pins_desktop_file_save (PinsDesktopFile *self, GError **error,
                             gboolean remove_unedited_user_files);
void pins_desktop_file_trash (PinsDesktopFile *self);
void pins_desktop_file_set_autostart (PinsDesktopFile *self, gboolean value);

gchar *pins_desktop_file_get_desktop_id (PinsDesktopFile *self);
GFile *pins_desktop_file_get_user_file (PinsDesktopFile *self);
gchar **pins_desktop_file_get_keys (PinsDesktopFile *self);
gchar **pins_desktop_file_get_locales (PinsDesktopFile *self);

gboolean pins_desktop_file_get_boolean (PinsDesktopFile *self,
                                        const gchar *key);
gchar *pins_desktop_file_get_string (PinsDesktopFile *self, const gchar *key);
void pins_desktop_file_set_boolean (PinsDesktopFile *self, const gchar *key,
                                    const gboolean value);
void pins_desktop_file_set_string (PinsDesktopFile *self, const gchar *key,
                                   const gchar *value);

gchar *pins_desktop_file_get_locale_for_key (PinsDesktopFile *self,
                                             const gchar *key);
gboolean pins_desktop_file_has_backup_for_key (PinsDesktopFile *self,
                                               const gchar *key);
gboolean pins_desktop_file_has_key (PinsDesktopFile *self, const gchar *key);
gboolean pins_desktop_file_is_key_edited (PinsDesktopFile *self,
                                          const gchar *key);
void pins_desktop_file_reset_key (PinsDesktopFile *self, const gchar *key);

G_END_DECLS
