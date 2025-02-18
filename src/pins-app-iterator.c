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
#include "pins-locale-utils-private.h"

#define DIR_LIST_FILE_ATTRIBUTES                                              \
    g_strjoin (",", G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,                   \
               G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,                        \
               G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME, NULL)

struct _PinsAppIterator
{
    GObject parent_instance;

    GHashTable *duplicate_paths;
    GHashTable *unique_filenames;
    gboolean just_created_file;
    GListModel *model;
    GtkFilterListModel *filter_model;
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
pins_app_iterator_create_user_file (PinsAppIterator *self, gchar *basename,
                                    GError **error)
{
    gchar increment[8] = "";
    gchar *filename;
    GFile *file;
    GOutputStream *stream;
    GError *err = NULL;

    if (self->just_created_file)
        return;

    for (int i = 0; i < 999999; i++)
        {
            if (i > 0)
                sprintf (increment, "-%d", i);

            filename = g_strconcat (basename, increment,
                                    PINS_DESKTOP_FILE_SUFFIX, NULL);
            if (!g_hash_table_contains (self->unique_filenames, filename))
                break;
        }

    file = g_file_new_build_filename (pins_desktop_file_user_path (), filename,
                                      NULL);
    stream = g_io_stream_get_output_stream (G_IO_STREAM (
        g_file_create_readwrite (file, G_FILE_CREATE_NONE, NULL, &err)));
    if (err != NULL)
        {
            g_propagate_error (error, err);
            return;
        }

    g_output_stream_write (stream, PINS_DESKTOP_FILE_DEFAULT_CONTENT,
                           strlen (PINS_DESKTOP_FILE_DEFAULT_CONTENT), NULL,
                           &err);
    if (err != NULL)
        g_propagate_error (error, err);

    g_output_stream_close (stream, NULL, NULL);
    self->just_created_file = TRUE;
}

static void
pins_app_iterator_update_duplicates (GListModel *model, guint position,
                                     guint removed, guint added,
                                     PinsAppIterator *self)
{
    g_hash_table_foreach (self->unique_filenames, (GHFunc)g_free, NULL);
    g_hash_table_remove_all (self->unique_filenames);

    g_hash_table_foreach (self->duplicate_paths, (GHFunc)g_free, NULL);
    g_hash_table_remove_all (self->duplicate_paths);

    for (int i = 0; i < g_list_model_get_n_items (model); i++)
        {
            g_autoptr (GFileInfo) file_info = NULL;
            GFile *file;
            g_autofree gchar *display_name;

            file_info = G_FILE_INFO (g_list_model_get_item (model, i));
            file = G_FILE (g_file_info_get_attribute_object (
                file_info, "standard::file"));
            display_name = (gchar *)g_file_get_basename (file);

            if (g_hash_table_contains (self->unique_filenames, display_name))
                g_hash_table_add (self->duplicate_paths,
                                  g_file_get_path (file));
            else
                g_hash_table_add (self->unique_filenames,
                                  g_strdup (display_name));
        }
}

void
pins_app_iterator_load (PinsAppIterator *self)
{
    GListStore *dir_list_store;
    GtkFlattenListModel *flattened_dir_list;
    g_auto (GStrv) paths;

    paths = pins_desktop_file_search_paths ();
    dir_list_store = g_list_store_new (GTK_TYPE_DIRECTORY_LIST);

    for (int i = 0; i < g_strv_length (paths); i++)
        {
            GFile *file = g_file_new_for_path (paths[i]);
            GtkDirectoryList *dir_list
                = gtk_directory_list_new (DIR_LIST_FILE_ATTRIBUTES, file);

            g_list_store_append (dir_list_store, dir_list);
        }

    flattened_dir_list
        = gtk_flatten_list_model_new (G_LIST_MODEL (dir_list_store));

    gtk_filter_list_model_set_model (self->filter_model,
                                     G_LIST_MODEL (flattened_dir_list));

    g_signal_connect_object (
        G_LIST_MODEL (flattened_dir_list), "items-changed",
        G_CALLBACK (pins_app_iterator_update_duplicates), self, 0);
}

gboolean
pins_app_iterator_filter_match_func (gpointer file_info, gpointer user_data)
{
    gboolean is_desktop_file, is_duplicate = FALSE;
    PinsAppIterator *self = PINS_APP_ITERATOR (user_data);
    GFile *file = G_FILE (
        g_file_info_get_attribute_object (file_info, "standard::file"));
    g_auto (GStrv) split_path = g_strsplit (g_file_get_path (file), ".", -1);

    is_desktop_file
        = g_strcmp0 (g_strconcat (".",
                                  split_path[g_strv_length (split_path) - 1],
                                  NULL),
                     PINS_DESKTOP_FILE_SUFFIX)
          == 0;
    is_duplicate = g_hash_table_contains (self->duplicate_paths,
                                          g_file_get_path (file));

    return is_desktop_file && !is_duplicate;
}

void
desktop_file_key_set_cb (PinsDesktopFile *desktop_file, gchar *key,
                         GtkSorter *sorter)
{
    if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NAME) == 0)
        gtk_sorter_changed (sorter, GTK_SORTER_CHANGE_DIFFERENT);
}

gpointer
pins_app_iterator_map_func (gpointer file_info, gpointer sorter)
{
    PinsDesktopFile *desktop_file;
    GError *err = NULL;
    GFile *file;

    g_assert (G_IS_FILE_INFO (file_info));

    file = G_FILE (
        g_file_info_get_attribute_object (file_info, "standard::file"));

    desktop_file = pins_desktop_file_new_from_file (file, &err);
    if (err != NULL)
        {
            g_warning ("Could not load desktop file at `%s`",
                       g_file_get_path (file));
            return NULL; /// TODO: Handle invalid files
        }

    g_signal_connect_object (desktop_file, "key-set",
                             G_CALLBACK (desktop_file_key_set_cb), sorter, 0);

    return desktop_file;
}

int
pins_app_iterator_sort_compare_func (gconstpointer a, gconstpointer b,
                                     gpointer user_data)
{
    PinsDesktopFile *first = PINS_DESKTOP_FILE ((gpointer)a);
    PinsDesktopFile *second = PINS_DESKTOP_FILE ((gpointer)b);
    const gchar *first_key, *second_key, *first_name, *second_name;

    g_return_val_if_fail (PINS_IS_DESKTOP_FILE (first), 0);
    g_return_val_if_fail (PINS_IS_DESKTOP_FILE (second), 0);

    first_key = _pins_join_key_locale (
        G_KEY_FILE_DESKTOP_KEY_NAME, pins_desktop_file_get_locale_for_key (
                                         first, G_KEY_FILE_DESKTOP_KEY_NAME));
    first_name = pins_desktop_file_get_string (first, first_key);

    second_key = _pins_join_key_locale (
        G_KEY_FILE_DESKTOP_KEY_NAME, pins_desktop_file_get_locale_for_key (
                                         second, G_KEY_FILE_DESKTOP_KEY_NAME));
    second_name = pins_desktop_file_get_string (second, second_key);

    /// TODO: Use UTF8 compare
    return g_strcmp0 (first_name, second_name);
}

void
pins_app_iterator_filter_pending_changed_cb (GtkFilterListModel *model,
                                             guint amount,
                                             PinsAppIterator *self)
{
    g_assert (GTK_IS_FILTER_LIST_MODEL (model));
    g_assert (PINS_IS_APP_ITERATOR (self));

    g_signal_emit (self, signals[LOADING], 0,
                   gtk_filter_list_model_get_pending (model) != 0);
}

void
desktop_file_model_items_changed_cb (GListModel *model, guint position,
                                     guint removed, guint added,
                                     PinsAppIterator *self)
{
    if (added > 0 && self->just_created_file)
        {
            PinsDesktopFile *desktop_file
                = PINS_DESKTOP_FILE (g_list_model_get_item (model, position));

            g_signal_emit (self, signals[FILE_CREATED], 0, desktop_file);
            self->just_created_file = FALSE;
        }

    g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
pins_app_iterator_class_init (PinsAppIteratorClass *klass)
{
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
    GtkSorter *sorter;
    GtkMapListModel *map_model;

    self->unique_filenames = g_hash_table_new (g_str_hash, g_str_equal);
    self->duplicate_paths = g_hash_table_new (g_str_hash, g_str_equal);
    self->just_created_file = FALSE;

    self->filter_model = gtk_filter_list_model_new (
        NULL, GTK_FILTER (gtk_custom_filter_new (
                  &pins_app_iterator_filter_match_func, self, NULL)));
    gtk_filter_list_model_set_incremental (self->filter_model, TRUE);

    sorter = GTK_SORTER (gtk_custom_sorter_new (
        pins_app_iterator_sort_compare_func, NULL, NULL));

    map_model
        = gtk_map_list_model_new (G_LIST_MODEL (self->filter_model),
                                  &pins_app_iterator_map_func, sorter, NULL);

    self->model = G_LIST_MODEL (
        gtk_sort_list_model_new (G_LIST_MODEL (map_model), sorter));

    g_signal_connect_object (
        self->filter_model, "notify::pending",
        G_CALLBACK (pins_app_iterator_filter_pending_changed_cb), self, 0);
    g_signal_connect_object (self->model, "items-changed",
                             G_CALLBACK (desktop_file_model_items_changed_cb),
                             self, 0);
}

gpointer
pins_app_iterator_get_item (GListModel *list, guint position)
{
    PinsAppIterator *self = PINS_APP_ITERATOR (list);

    return g_list_model_get_item (self->model, position);
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

    return g_list_model_get_n_items (self->model);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
    iface->get_item = pins_app_iterator_get_item;
    iface->get_item_type = pins_app_iterator_get_item_type;
    iface->get_n_items = pins_app_iterator_get_n_items;
}
