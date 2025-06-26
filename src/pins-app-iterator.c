/* pins-app-iterator.c
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

#include "pins-app-iterator.h"

#include "pins-desktop-file.h"
#include "pins-directories.h"

#define DESKTOP_FILE_ATTRIBUTES                                               \
    g_strjoin (",", G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,                   \
               G_FILE_ATTRIBUTE_STANDARD_NAME,                                \
               G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,                        \
               G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME, NULL)
#define DESKTOP_CONTENT_TYPE "application/x-desktop"

struct _PinsAppIterator
{
    GObject parent_instance;

    GHashTable *desktop_files_by_id;
    GPtrArray *desktop_files_array;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (PinsAppIterator, pins_app_iterator, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                list_model_iface_init))

enum
{
    LOADING,
    FILE_CREATED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

PinsAppIterator *
pins_app_iterator_new (void)
{
    return g_object_new (PINS_TYPE_APP_ITERATOR, NULL);
}

void
pins_app_iterator_key_set_cb (PinsAppIterator *self, gchar *key,
                              PinsDesktopFile *desktop_file)
{
    guint position;

    if (!g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NAME))
        {
            g_ptr_array_find (self->desktop_files_array, desktop_file,
                              &position);

            g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 1);
        }
}

void
pins_app_iterator_file_deleted_cb (PinsAppIterator *self,
                                   PinsDesktopFile *desktop_file)
{
    g_autofree gchar *desktop_id;
    guint position;

    desktop_id = pins_desktop_file_get_desktop_id (desktop_file);
    g_ptr_array_find (self->desktop_files_array, desktop_file, &position);

    g_assert (g_hash_table_remove (self->desktop_files_by_id, desktop_id));
    g_assert (g_ptr_array_remove_index (self->desktop_files_array, position));

    g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

void
desktop_files_by_id_insert_file (PinsAppIterator *self, gchar *desktop_id,
                                 PinsDesktopFile *desktop_file)
{
    g_hash_table_insert (self->desktop_files_by_id, desktop_id, desktop_file);

    g_signal_connect_object (desktop_file, "key-set",
                             G_CALLBACK (pins_app_iterator_key_set_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (desktop_file, "file-deleted",
                             G_CALLBACK (pins_app_iterator_file_deleted_cb),
                             self, G_CONNECT_SWAPPED);
}

void
load_file_checked (PinsAppIterator *self, GFileInfo *info, GFile *file)
{
    PinsDesktopFile *desktop_file = NULL;
    g_autoptr (GError) err = NULL;

    if (g_strcmp0 (g_file_info_get_content_type (info), DESKTOP_CONTENT_TYPE))
        return;

    desktop_file = pins_desktop_file_new (file, &err);
    if (err != NULL)
        {
            g_warning ("Error loading file: %s", err->message);
            return;
        }

    desktop_files_by_id_insert_file (self, g_file_get_basename (file),
                                     g_object_ref (desktop_file));
}

void
pins_app_iterator_load (PinsAppIterator *self)
{
    GFileEnumerator *enumerator = NULL;
    g_autoptr (GFileInfo) info = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GError) err = NULL;
    g_auto (GStrv) paths;

    g_signal_emit (self, signals[LOADING], 0, TRUE);

    g_hash_table_remove_all (self->desktop_files_by_id);
    g_ptr_array_free (self->desktop_files_array, TRUE);

    paths = pins_desktop_file_search_paths ();

    for (int i = 0; paths[i] != 0 && paths != NULL; i++)
        {
            enumerator = g_file_enumerate_children (
                g_file_parse_name (paths[i]), DESKTOP_FILE_ATTRIBUTES,
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

    self->desktop_files_array
        = g_hash_table_get_values_as_ptr_array (self->desktop_files_by_id);

    g_list_model_items_changed (G_LIST_MODEL (self), 0, 0,
                                self->desktop_files_array->len);

    g_signal_emit (self, signals[LOADING], 0, FALSE);
}

void
pins_app_iterator_create_user_file (PinsAppIterator *self, gchar *basename,
                                    GError **error)
{
    gchar increment[8] = "";
    g_autoptr (GFile) file;
    g_autoptr (GError) err = NULL;
    gchar *filename;
    PinsDesktopFile *desktop_file;

    for (int i = 0; i < 999999; i++)
        {
            if (i > 0)
                sprintf (increment, "-%d", i);

            filename = g_strconcat (basename, increment,
                                    PINS_DESKTOP_FILE_SUFFIX, NULL);
            if (!g_hash_table_contains (self->desktop_files_by_id, filename))
                break;
        }

    file = g_file_new_build_filename (pins_desktop_file_user_path (), filename,
                                      NULL);
    g_file_replace_contents (file, PINS_DESKTOP_FILE_DEFAULT_CONTENT,
                             strlen (PINS_DESKTOP_FILE_DEFAULT_CONTENT), NULL,
                             FALSE, G_FILE_CREATE_NONE, NULL, NULL, &err);
    if (err != NULL)
        {
            g_propagate_error (error, err);
            return;
        }

    desktop_file = pins_desktop_file_new (file, NULL);

    desktop_files_by_id_insert_file (self, filename, desktop_file);
    g_assert (g_hash_table_contains (self->desktop_files_by_id, filename));

    g_ptr_array_add (self->desktop_files_array, desktop_file);
    g_list_model_items_changed (G_LIST_MODEL (self),
                                self->desktop_files_array->len - 1, 0, 1);

    g_signal_emit (self, signals[FILE_CREATED], 0, desktop_file);
}

static void
pins_app_iterator_dispose (GObject *object)
{
    PinsAppIterator *self = PINS_APP_ITERATOR (object);

    g_hash_table_unref (self->desktop_files_by_id);
    g_ptr_array_unref (self->desktop_files_array);
}

static void
pins_app_iterator_class_init (PinsAppIteratorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = pins_app_iterator_dispose;

    signals[LOADING] = g_signal_new ("loading", G_TYPE_FROM_CLASS (klass),
                                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
                                     G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals[FILE_CREATED] = g_signal_new (
        "file-created", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_OBJECT);
}

static void
pins_app_iterator_init (PinsAppIterator *self)
{
    self->desktop_files_by_id = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       g_free, g_object_unref);
    self->desktop_files_array = g_ptr_array_new ();
}

gpointer
pins_app_iterator_get_item (GListModel *list, guint position)
{
    PinsAppIterator *self = PINS_APP_ITERATOR (list);

    if (position < self->desktop_files_array->len)
        return g_object_ref (self->desktop_files_array->pdata[position]);
    else
        return NULL;
}

GType
pins_app_iterator_get_item_type (GListModel *list)
{
    return PINS_TYPE_DESKTOP_FILE;
}

guint
pins_app_iterator_get_n_items (GListModel *list)
{
    PinsAppIterator *self = PINS_APP_ITERATOR (list);

    return self->desktop_files_array->len;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
    iface->get_item = pins_app_iterator_get_item;
    iface->get_item_type = pins_app_iterator_get_item_type;
    iface->get_n_items = pins_app_iterator_get_n_items;
}
