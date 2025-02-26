/* pins-desktop-file.c
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

#include "pins-desktop-file.h"

#include "pins-directories.h"
#include "pins-locale-utils-private.h"

#include <fcntl.h>

#define KEY_FILE_FLAGS G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS

struct _PinsDesktopFile
{
    GObject parent_instance;

    GFile *user_file;
    GFile *system_file;
    GFile *autostart_file;
    GKeyFile *key_file;
    GKeyFile *backup_key_file;
    gchar *saved_data;
};

G_DEFINE_TYPE (PinsDesktopFile, pins_desktop_file, G_TYPE_OBJECT);

enum
{
    PROP_0,
    PROP_SEARCH_STRING,
    N_PROPS,
};

enum
{
    KEY_SET,
    KEY_REMOVED,
    FILE_DELETED,
    N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

PinsDesktopFile *
pins_desktop_file_new_full (GFile *user_file, GFile *system_file,
                            GError **error)
{
    PinsDesktopFile *desktop_file = NULL;
    g_autoptr (GError) err = NULL;

    g_assert_nonnull (user_file);

    desktop_file = g_object_new (PINS_TYPE_DESKTOP_FILE, NULL);
    desktop_file->user_file = g_object_ref (user_file);
    desktop_file->autostart_file
        = g_file_new_build_filename (pins_desktop_file_autostart_path (),
                                     g_file_get_basename (user_file), NULL);

    if (system_file != NULL)
        {
            g_assert (!g_strcmp0 (g_file_get_basename (user_file),
                                  g_file_get_basename (system_file)));

            desktop_file->system_file = g_object_ref (system_file);

            g_key_file_load_from_file (desktop_file->backup_key_file,
                                       g_file_get_path (system_file),
                                       KEY_FILE_FLAGS, &err);
            if (err != NULL)
                {
                    g_propagate_error (error, err);
                    return NULL;
                }
        }

    if (g_file_query_exists (desktop_file->user_file, NULL))
        g_key_file_load_from_file (desktop_file->key_file,
                                   g_file_get_path (desktop_file->user_file),
                                   KEY_FILE_FLAGS, &err);
    else if (system_file != NULL)
        g_key_file_load_from_file (desktop_file->key_file,
                                   g_file_get_path (desktop_file->system_file),
                                   KEY_FILE_FLAGS, &err);
    else
        g_assert_not_reached ();
    if (err != NULL)
        {
            g_propagate_error (error, err);
            return NULL;
        }

    desktop_file->saved_data
        = g_key_file_to_data (desktop_file->key_file, NULL, NULL);

    return desktop_file;
}

/**
 * Given a `GFile`, it constructs a `PinsDesktopFile` with the following
 * logic:
 *  - If the file is in the system folder and a file with the same name is
 *    found in the user folder (`USER_APPS_DIR`), `PinsDesktopFile` is
 * created with both a user and a system `GKeyFile`;
 *  - If the file is in the system folder and no file with the same name is
 *    found in the user folder, `PinsDesktopFile` is created without a user
 *    `GKeyFile`;
 *  - If the file is in the user folder, `PinsDesktopFile` is created
 * without a system `GKeyFile`.
 *
 * It is assumed that `file` exists.
 */
PinsDesktopFile *
pins_desktop_file_new (GFile *file, GError **error)
{
    gboolean file_is_user_file;

    g_assert (g_file_query_exists (file, NULL));

    file_is_user_file
        = g_file_equal (g_file_get_parent (file),
                        g_file_new_for_path (pins_desktop_file_user_path ()));

    if (file_is_user_file)
        return pins_desktop_file_new_full (file, NULL, error);
    else
        {
            g_autoptr (GFile) user_file
                = g_file_new_build_filename (pins_desktop_file_user_path (),
                                             g_file_get_basename (file), NULL);

            return pins_desktop_file_new_full (user_file, file, error);
        }
}

gboolean
pins_desktop_file_is_user_only (PinsDesktopFile *self)
{
    return self->system_file == NULL;
}

gboolean
pins_desktop_file_is_user_edited (PinsDesktopFile *self)
{
    return g_file_query_exists (self->user_file, NULL);
}

gboolean
pins_desktop_file_is_autostart (PinsDesktopFile *self)
{
    return g_file_query_exists (self->autostart_file, NULL);
}

gboolean
pins_desktop_file_is_shown (PinsDesktopFile *self)
{
    gboolean hidden, no_display;
    const gchar *current_desktop = NULL;
    g_auto (GStrv) only_shown_in = NULL, not_shown_in = NULL;

    hidden
        = pins_desktop_file_get_boolean (self, G_KEY_FILE_DESKTOP_KEY_HIDDEN);
    no_display = pins_desktop_file_get_boolean (
        self, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY);

    if (hidden || no_display)
        return FALSE;

    current_desktop = g_getenv ("XDG_CURRENT_DESKTOP");

    if (current_desktop == NULL)
        return TRUE;

    only_shown_in = g_key_file_get_string_list (
        self->key_file, G_KEY_FILE_DESKTOP_GROUP,
        G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, NULL, NULL);
    not_shown_in = g_key_file_get_string_list (
        self->key_file, G_KEY_FILE_DESKTOP_GROUP,
        G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, NULL, NULL);

    if (only_shown_in != NULL)
        return g_strv_contains ((const gchar *const *)only_shown_in,
                                current_desktop);

    if (not_shown_in != NULL)
        return !g_strv_contains ((const gchar *const *)not_shown_in,
                                 current_desktop);

    return TRUE;
}

void
pins_desktop_file_trash (PinsDesktopFile *self)
{
    g_autoptr (GError) err = NULL;

    g_file_trash (self->user_file, NULL, &err);
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_file_delete (self->user_file, NULL, NULL);

    g_signal_emit (self, signals[FILE_DELETED], 0);
}

void
pins_desktop_file_save (PinsDesktopFile *self, GError **error)
{
    g_autoptr (GOutputStream) stream = NULL;
    g_autoptr (GError) err = NULL;
    gsize lenght;

    if (!g_strcmp0 (g_key_file_to_data (self->key_file, NULL, NULL),
                    self->saved_data))
        return;

    self->saved_data = g_key_file_to_data (self->key_file, &lenght, NULL);

    if (self->system_file != NULL
        && g_strcmp0 (self->saved_data,
                      g_key_file_to_data (self->backup_key_file, NULL, NULL))
               == 0)
        {
            g_file_delete (self->user_file, NULL, NULL);
            return;
        }

    g_file_replace_contents (self->user_file, self->saved_data, lenght, NULL,
                             FALSE, G_FILE_CREATE_NONE, NULL, NULL, error);
}

void
pins_desktop_file_set_autostart (PinsDesktopFile *self, gboolean value)
{
    if (value == pins_desktop_file_is_autostart (self))
        return;

    if (value)
        g_key_file_save_to_file (self->key_file,
                                 g_file_get_path (self->autostart_file), NULL);
    else
        g_file_delete (self->autostart_file, NULL, NULL);
}

gchar **
pins_desktop_file_get_keys (PinsDesktopFile *self)
{
    gchar **keys;
    GError *err = NULL;

    g_assert (PINS_IS_DESKTOP_FILE (self));

    keys = g_key_file_get_keys (self->key_file, G_KEY_FILE_DESKTOP_GROUP, NULL,
                                &err);
    if (err != NULL)
        {
            gchar **empty_strv = { NULL };
            return empty_strv;
        }

    return keys;
}

gchar **
pins_desktop_file_get_locales (PinsDesktopFile *self)
{
    return _pins_locales_from_keys (pins_desktop_file_get_keys (self));
}

static void
pins_desktop_file_dispose (GObject *object)
{
    PinsDesktopFile *self = PINS_DESKTOP_FILE (object);

    g_clear_object (&self->user_file);
    g_clear_object (&self->system_file);
    g_clear_object (&self->autostart_file);
    g_clear_object (&self->key_file);
    g_clear_object (&self->backup_key_file);
}

static void
pins_desktop_file_get_property (GObject *object, guint prop_id, GValue *value,
                                GParamSpec *pspec)
{
    PinsDesktopFile *self = PINS_DESKTOP_FILE (object);

    switch (prop_id)
        {
        case PROP_SEARCH_STRING:
            g_value_set_string (value, self->saved_data);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
pins_desktop_file_class_init (PinsDesktopFileClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = pins_desktop_file_dispose;
    object_class->get_property = pins_desktop_file_get_property;

    properties[PROP_SEARCH_STRING]
        = g_param_spec_string ("search-string", "Search String",
                               "Data of the file as a searchable string", "",
                               (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_properties (object_class, N_PROPS, properties);

    signals[KEY_SET] = g_signal_new ("key-set", G_TYPE_FROM_CLASS (klass),
                                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
                                     G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[KEY_REMOVED] = g_signal_new (
        "key-removed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[FILE_DELETED] = g_signal_new (
        "file-removed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 0);
}

static void
pins_desktop_file_init (PinsDesktopFile *self)
{
    self->key_file = g_key_file_new ();
    self->backup_key_file = g_key_file_new ();
}

gboolean
pins_desktop_file_get_boolean (PinsDesktopFile *self, const gchar *key)
{
    gboolean value;
    GError *err = NULL;

    value = g_key_file_get_boolean (self->key_file, G_KEY_FILE_DESKTOP_GROUP,
                                    key, &err);
    if (g_error_matches (err, G_KEY_FILE_ERROR,
                         G_KEY_FILE_ERROR_INVALID_VALUE))
        return TRUE;
    else if (err != NULL)
        return FALSE;

    return value;
}

gchar *
pins_desktop_file_get_string (PinsDesktopFile *self, const gchar *key)
{
    gchar *value;
    GError *err = NULL;

    value = g_key_file_get_string (self->key_file, G_KEY_FILE_DESKTOP_GROUP,
                                   key, &err);
    if (err != NULL)
        return g_strdup ("");

    return value;
}

void
pins_desktop_file_set_boolean (PinsDesktopFile *self, const gchar *key,
                               const gboolean value)
{
    g_key_file_set_boolean (self->key_file, G_KEY_FILE_DESKTOP_GROUP, key,
                            value);

    g_signal_emit (self, signals[KEY_SET], 0, key);
}

void
pins_desktop_file_set_string (PinsDesktopFile *self, const gchar *key,
                              const gchar *value)
{
    g_key_file_set_string (self->key_file, G_KEY_FILE_DESKTOP_GROUP, key,
                           value);

    g_signal_emit (self, signals[KEY_SET], 0, key);
}

gchar *
get_locale_for_key_checked (GKeyFile *key_file, const gchar *key)
{
    gchar *locale;

    locale = g_key_file_get_locale_for_key (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                            key, NULL);

    if (!g_strcmp0 (locale, "C"))
        {
            g_free (locale);
            return NULL;
        }

    return locale;
}

gchar *
pins_desktop_file_get_locale_for_key (PinsDesktopFile *self, const gchar *key)
{
    g_autofree gchar *locale;

    locale = get_locale_for_key_checked (self->key_file, key);

    if (locale == NULL && self->system_file != NULL)
        locale = get_locale_for_key_checked (self->backup_key_file, key);

    return g_strdup (locale);
}

gboolean
pins_desktop_file_has_backup_for_key (PinsDesktopFile *self, const gchar *key)
{
    if (self->system_file == NULL)
        return FALSE;

    return g_key_file_has_key (self->backup_key_file, G_KEY_FILE_DESKTOP_GROUP,
                               key, NULL);
}

gboolean
pins_desktop_file_has_key (PinsDesktopFile *self, const gchar *key)
{
    return g_key_file_has_key (self->key_file, G_KEY_FILE_DESKTOP_GROUP, key,
                               NULL)
           || pins_desktop_file_has_backup_for_key (self, key);
}

gboolean
pins_desktop_file_is_key_edited (PinsDesktopFile *self, const gchar *key)
{
    if (self->system_file == NULL)
        return TRUE;

    return g_strcmp0 (
               g_key_file_get_string (self->key_file, G_KEY_FILE_DESKTOP_GROUP,
                                      key, NULL),
               g_key_file_get_string (self->backup_key_file,
                                      G_KEY_FILE_DESKTOP_GROUP, key, NULL))
           != 0;
}

void
pins_desktop_file_reset_key (PinsDesktopFile *self, const gchar *key)
{
    if (pins_desktop_file_has_backup_for_key (self, key))
        {
            g_key_file_set_string (
                self->key_file, G_KEY_FILE_DESKTOP_GROUP, key,
                g_key_file_get_string (self->backup_key_file,
                                       G_KEY_FILE_DESKTOP_GROUP, key, NULL));

            g_signal_emit (self, signals[KEY_SET], 0, key);
        }
    else
        {
            g_key_file_remove_key (self->key_file, G_KEY_FILE_DESKTOP_GROUP,
                                   key, NULL);

            g_signal_emit (self, signals[KEY_REMOVED], 0, key);
        }
}
