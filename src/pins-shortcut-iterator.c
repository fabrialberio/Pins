/* pins-shortcut-iterator.c
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

#include "pins-shortcut-iterator.h"

#include "pins-directories.h"
#include "pins-locale-utils-private.h"
#include "pins-shortcut.h"

#define SHORTCUT_ATTRIBUTES                                                   \
    g_strjoin (",", G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,                   \
               G_FILE_ATTRIBUTE_STANDARD_NAME,                                \
               G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,                        \
               G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME, NULL)
#define SHORTCUT_CONTENT_TYPE "application/x-desktop"

struct _PinsShortcutIterator
{
    GObject parent_instance;

    GHashTable *shortcuts_by_id;
    GHashTable *files_by_id;
    GPtrArray *shortcuts_array;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (PinsShortcutIterator, pins_shortcut_iterator,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                list_model_iface_init))

enum
{
    LOADING,
    SHORTCUT_CREATED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

PinsShortcutIterator *
pins_shortcut_iterator_new (void)
{
    return g_object_new (PINS_TYPE_SHORTCUT_ITERATOR, NULL);
}

void
pins_shortcut_iterator_key_set_cb (PinsShortcutIterator *self, gchar *key,
                                   PinsShortcut *shortcut)
{
    guint position;

    // TODO: Even when it emits items-changed on every key-set event, the
    // filter list models of PinsShortcutFilter do not update immediately.
    if (!g_strcmp0 (_pins_split_key_locale (key).key,
                    G_KEY_FILE_DESKTOP_KEY_NAME))
        {
            g_ptr_array_find (self->shortcuts_array, shortcut, &position);

            g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 1);
        }
}

void
pins_shortcut_iterator_shortcut_deleted_cb (PinsShortcutIterator *self,
                                            PinsShortcut *shortcut)
{
    g_autofree gchar *desktop_id;
    guint position;

    desktop_id = pins_shortcut_get_desktop_id (shortcut);
    g_ptr_array_find (self->shortcuts_array, shortcut, &position);

    g_assert (g_hash_table_remove (self->shortcuts_by_id, desktop_id));
    g_assert (g_hash_table_remove (self->files_by_id, desktop_id));
    g_assert (g_ptr_array_remove_index (self->shortcuts_array, position));

    g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

void
shortcuts_by_id_insert_shortcut (PinsShortcutIterator *self, GFile *file,
                                 PinsShortcut *shortcut)
{
    gchar *desktop_id = g_file_get_basename (file);

    g_hash_table_insert (self->shortcuts_by_id, desktop_id, shortcut);
    g_hash_table_insert (self->files_by_id, g_strdup (desktop_id), file);

    g_signal_connect_object (shortcut, "key-set",
                             G_CALLBACK (pins_shortcut_iterator_key_set_cb),
                             self, G_CONNECT_SWAPPED);
    g_signal_connect_object (
        shortcut, "deleted",
        G_CALLBACK (pins_shortcut_iterator_shortcut_deleted_cb), self,
        G_CONNECT_SWAPPED);
}

void
load_file_checked (PinsShortcutIterator *self, GFileInfo *info, GFile *file)
{
    PinsShortcut *shortcut = NULL;
    g_autoptr (GError) err = NULL;

    if (g_strcmp0 (g_file_info_get_content_type (info), SHORTCUT_CONTENT_TYPE))
        return;

    shortcut = pins_shortcut_new (file, &err);
    if (err != NULL)
        {
            g_warning ("Error loading file «%s»: %s", g_file_get_path (file),
                       err->message);
            return;
        }

    shortcuts_by_id_insert_shortcut (self, file, g_object_ref (shortcut));
}

void
pins_shortcut_iterator_load (PinsShortcutIterator *self)
{
    GFileEnumerator *enumerator = NULL;
    g_autoptr (GFileInfo) info = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GError) err = NULL;
    g_auto (GStrv) paths;

    g_signal_emit (self, signals[LOADING], 0, TRUE);

    g_hash_table_remove_all (self->shortcuts_by_id);
    g_hash_table_remove_all (self->files_by_id);
    g_ptr_array_free (self->shortcuts_array, TRUE);

    paths = pins_shortcut_search_paths ();

    for (int i = 0; paths[i] != 0 && paths != NULL; i++)
        {
            enumerator = g_file_enumerate_children (
                g_file_parse_name (paths[i]), SHORTCUT_ATTRIBUTES,
                G_FILE_QUERY_INFO_NONE, NULL, &err);
            if (err != NULL)
                {
                    err = NULL;
                    continue;
                }

            while (TRUE)
                {
                    g_file_enumerator_iterate (enumerator, &info, &file, NULL,
                                               NULL);
                    if (info == NULL)
                        break;

                    load_file_checked (self, info, file);
                }

            g_file_enumerator_close (enumerator, NULL, NULL);
            g_object_unref (enumerator);
        }

    self->shortcuts_array
        = g_hash_table_get_values_as_ptr_array (self->shortcuts_by_id);

    g_list_model_items_changed (G_LIST_MODEL (self), 0, 0,
                                self->shortcuts_array->len);

    g_signal_emit (self, signals[LOADING], 0, FALSE);
}

void
pins_shortcut_iterator_create_user_file (PinsShortcutIterator *self,
                                         const gchar *basename,
                                         const gchar *contents, GError **error)
{
    gchar increment[8] = "";
    g_autoptr (GFile) file;
    g_autoptr (GError) err = NULL;
    gchar *filename;
    PinsShortcut *shortcut;

    for (int i = 0; i < 999999; i++)
        {
            if (i > 0)
                sprintf (increment, "-%d", i);

            filename = g_strconcat (basename, increment, PINS_SHORTCUT_SUFFIX,
                                    NULL);
            if (!g_hash_table_contains (self->shortcuts_by_id, filename))
                break;
        }

    file = g_file_new_build_filename (pins_shortcut_user_path (), filename,
                                      NULL);
    g_file_replace_contents (file, contents, strlen (contents), NULL, FALSE,
                             G_FILE_CREATE_NONE, NULL, NULL, &err);
    if (err != NULL)
        return g_propagate_error (error, err);

    shortcut = pins_shortcut_new (file, NULL);

    shortcuts_by_id_insert_shortcut (self, file, shortcut);
    g_assert (g_hash_table_contains (self->shortcuts_by_id, filename));

    g_ptr_array_add (self->shortcuts_array, shortcut);
    g_list_model_items_changed (G_LIST_MODEL (self),
                                self->shortcuts_array->len - 1, 0, 1);

    g_signal_emit (self, signals[SHORTCUT_CREATED], 0, shortcut);
}

void
pins_shortcut_iterator_duplicate_shortcut (PinsShortcutIterator *self,
                                           const gchar *desktop_id,
                                           GError **error)
{
    GFile *file = g_hash_table_lookup (self->files_by_id, desktop_id);
    GError *err = NULL;
    gchar *contents = NULL, *basename = NULL;
    g_auto (GStrv) split_desktop_id = NULL;

    g_file_get_contents (g_file_get_path (file), &contents, NULL, &err);
    if (err != NULL)
        return g_propagate_error (error, err);

    // Remove ".desktop" suffix from desktop_id.
    split_desktop_id = g_strsplit (desktop_id, ".", -1);
    split_desktop_id[g_strv_length (split_desktop_id) - 1] = NULL;
    basename = g_strjoinv (".", split_desktop_id);

    return pins_shortcut_iterator_create_user_file (self, basename, contents,
                                                    error);
}

static void
pins_shortcut_iterator_dispose (GObject *object)
{
    PinsShortcutIterator *self = PINS_SHORTCUT_ITERATOR (object);

    g_hash_table_unref (self->shortcuts_by_id);
    g_hash_table_unref (self->files_by_id);
    g_ptr_array_unref (self->shortcuts_array);
}

static void
pins_shortcut_iterator_class_init (PinsShortcutIteratorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = pins_shortcut_iterator_dispose;

    signals[LOADING] = g_signal_new ("loading", G_TYPE_FROM_CLASS (klass),
                                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
                                     G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals[SHORTCUT_CREATED] = g_signal_new (
        "shortcut-created", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_OBJECT);
}

static void
pins_shortcut_iterator_init (PinsShortcutIterator *self)
{
    self->shortcuts_by_id = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   g_free, g_object_unref);
    self->files_by_id = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                               g_object_unref);
    self->shortcuts_array = g_ptr_array_new ();
}

gpointer
pins_shortcut_iterator_get_item (GListModel *list, guint position)
{
    PinsShortcutIterator *self = PINS_SHORTCUT_ITERATOR (list);

    if (position < self->shortcuts_array->len)
        return g_object_ref (self->shortcuts_array->pdata[position]);
    else
        return NULL;
}

GType
pins_shortcut_iterator_get_item_type (GListModel *list)
{
    return PINS_TYPE_SHORTCUT;
}

guint
pins_shortcut_iterator_get_n_items (GListModel *list)
{
    PinsShortcutIterator *self = PINS_SHORTCUT_ITERATOR (list);

    return self->shortcuts_array->len;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
    iface->get_item = pins_shortcut_iterator_get_item;
    iface->get_item_type = pins_shortcut_iterator_get_item_type;
    iface->get_n_items = pins_shortcut_iterator_get_n_items;
}
