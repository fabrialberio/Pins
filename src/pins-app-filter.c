/* pins-app-filter.c
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

#include "pins-app-filter.h"

#include "pins-desktop-file.h"
#include "pins-locale-utils-private.h"

struct _PinsAppFilter
{
    GObject parent_instance;

    gboolean show_all_apps;
    GtkCustomFilter *show_all_apps_filter;
    GtkStringFilter *search_filter;
    GtkSortListModel *sort_model;
    GtkFilterListModel *show_all_apps_model;
    GtkFilterListModel *search_model;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (PinsAppFilter, pins_app_filter, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                list_model_iface_init));

enum
{
    PROP_0,
    PROP_SHOW_ALL_APPS,
    N_PROPS,
};

static GParamSpec *properties[N_PROPS];

PinsAppFilter *
pins_app_filter_new (void)
{
    return g_object_new (PINS_TYPE_APP_FILTER, NULL);
}

void
pins_app_filter_set_model (PinsAppFilter *self, GListModel *model)
{
    gtk_sort_list_model_set_model (self->sort_model, model);
}

void
pins_app_filter_set_search (PinsAppFilter *self, const gchar *search)
{
    gtk_string_filter_set_search (self->search_filter, search);
}

void
show_all_apps_notify_cb (PinsAppFilter *self, GParamSpec *pspec)
{
    GtkFilterChange change = self->show_all_apps
                                 ? GTK_FILTER_CHANGE_LESS_STRICT
                                 : GTK_FILTER_CHANGE_MORE_STRICT;

    gtk_filter_changed (GTK_FILTER (self->show_all_apps_filter), change);
}

gboolean
show_all_apps_match_func (gpointer desktop_file, gpointer user_data)
{
    PinsAppFilter *self = PINS_APP_FILTER (user_data);

    g_assert (PINS_IS_DESKTOP_FILE (desktop_file));

    if (self->show_all_apps)
        return TRUE;

    return pins_desktop_file_is_shown (PINS_DESKTOP_FILE (desktop_file))
           || pins_desktop_file_is_user_edited (
               PINS_DESKTOP_FILE (desktop_file));
}

int
sort_compare_func (gconstpointer a, gconstpointer b, gpointer user_data)
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

static void
pins_app_filter_dispose (GObject *object)
{
    PinsAppFilter *self = PINS_APP_FILTER (object);

    g_clear_object (&self->show_all_apps_filter);
    g_clear_object (&self->search_filter);
    g_clear_object (&self->sort_model);
    g_clear_object (&self->show_all_apps_model);
    g_clear_object (&self->search_model);
}

static void
pins_app_filter_get_property (GObject *object, guint prop_id, GValue *value,
                              GParamSpec *pspec)
{
    PinsAppFilter *self = PINS_APP_FILTER (object);

    switch (prop_id)
        {
        case PROP_SHOW_ALL_APPS:
            g_value_set_boolean (value, self->show_all_apps);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
pins_app_filter_set_property (GObject *object, guint prop_id,
                              const GValue *value, GParamSpec *pspec)
{
    PinsAppFilter *self = PINS_APP_FILTER (object);

    switch (prop_id)
        {
        case PROP_SHOW_ALL_APPS:
            self->show_all_apps = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
pins_app_filter_class_init (PinsAppFilterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = pins_app_filter_dispose;
    object_class->get_property = pins_app_filter_get_property;
    object_class->set_property = pins_app_filter_set_property;

    properties[PROP_SHOW_ALL_APPS] = g_param_spec_boolean (
        "show-all-apps", "Show All Apps", "Whether all apps are shown", FALSE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
pins_app_filter_init (PinsAppFilter *self)
{
    self->show_all_apps_filter
        = gtk_custom_filter_new (&show_all_apps_match_func, self, NULL);

    self->search_filter = gtk_string_filter_new (gtk_property_expression_new (
        PINS_TYPE_DESKTOP_FILE, NULL, "search-string"));

    self->sort_model = gtk_sort_list_model_new (
        NULL,
        GTK_SORTER (gtk_custom_sorter_new (&sort_compare_func, NULL, NULL)));

    self->show_all_apps_model
        = gtk_filter_list_model_new (G_LIST_MODEL (self->sort_model),
                                     GTK_FILTER (self->show_all_apps_filter));

    self->search_model
        = gtk_filter_list_model_new (G_LIST_MODEL (self->show_all_apps_model),
                                     GTK_FILTER (self->search_filter));

    g_signal_connect_object (self->search_model, "items-changed",
                             G_CALLBACK (g_list_model_items_changed), self,
                             G_CONNECT_SWAPPED);

    g_signal_connect_object (self, "notify::show-all-apps",
                             G_CALLBACK (show_all_apps_notify_cb), self, 0);
}

gpointer
pins_app_filter_get_item (GListModel *list, guint position)
{
    PinsAppFilter *self = PINS_APP_FILTER (list);

    return g_list_model_get_item (G_LIST_MODEL (self->search_model), position);
}

GType
pins_app_filter_get_item_type (GListModel *list)
{
    return PINS_TYPE_DESKTOP_FILE;
}

guint
pins_app_filter_get_n_items (GListModel *list)
{
    PinsAppFilter *self = PINS_APP_FILTER (list);

    return g_list_model_get_n_items (G_LIST_MODEL (self->search_model));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
    iface->get_item = pins_app_filter_get_item;
    iface->get_item_type = pins_app_filter_get_item_type;
    iface->get_n_items = pins_app_filter_get_n_items;
}
