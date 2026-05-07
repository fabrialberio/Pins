/* pins-shortcut-filter.c
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

#include "pins-shortcut-filter.h"

#include "pins-locale-utils-private.h"
#include "pins-shortcut.h"

struct _PinsShortcutFilter
{
    GObject parent_instance;

    gboolean show_all_apps;
    PinsShortcutFilterCategory category;

    GtkCustomFilter *category_filter;
    GtkStringFilter *search_filter;
    GtkSortListModel *sort_model;
    GtkFilterListModel *category_model;
    GtkFilterListModel *search_model;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (PinsShortcutFilter, pins_shortcut_filter,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                list_model_iface_init));

enum
{
    PROP_0,
    PROP_SHOW_ALL_APPS,
    PROP_CATEGORY,
    N_PROPS,
};

static GParamSpec *properties[N_PROPS];

PinsShortcutFilter *
pins_shortcut_filter_new (void)
{
    return g_object_new (PINS_TYPE_SHORTCUT_FILTER, NULL);
}

void
pins_shortcut_filter_set_model (PinsShortcutFilter *self, GListModel *model)
{
    gtk_sort_list_model_set_model (self->sort_model, model);
}

void
pins_shortcut_filter_set_search (PinsShortcutFilter *self, const gchar *search)
{
    gtk_string_filter_set_search (self->search_filter, search);
}

void
pins_shortcut_filter_reset_category (PinsShortcutFilter *self)
{
    if (self->show_all_apps)
        self->category = PINS_SHORTCUT_FILTER_CATEGORY_ALL;
    else
        self->category = PINS_SHORTCUT_FILTER_CATEGORY_VISIBLE;

    g_object_notify (G_OBJECT (self), "category");
}

void
category_notify_cb (PinsShortcutFilter *self, GParamSpec *pspec)
{
    gtk_filter_changed (GTK_FILTER (self->category_filter),
                        GTK_FILTER_CHANGE_DIFFERENT);
}

gboolean
category_match_func (gpointer shortcut, gpointer user_data)
{
    PinsShortcutFilter *self = PINS_SHORTCUT_FILTER (user_data);
    PinsShortcut *file = PINS_SHORTCUT (shortcut);

    switch (self->category)
        {
        case PINS_SHORTCUT_FILTER_CATEGORY_ALL:
            return TRUE;
        case PINS_SHORTCUT_FILTER_CATEGORY_VISIBLE:
            return pins_shortcut_is_shown (file)
                   || pins_shortcut_is_user_edited (file);
        case PINS_SHORTCUT_FILTER_CATEGORY_EDITED:
            return pins_shortcut_is_user_edited (file);
        case PINS_SHORTCUT_FILTER_CATEGORY_SYSTEM:
            return pins_shortcut_is_shown (file)
                   && !pins_shortcut_is_user_edited (file);
        case PINS_SHORTCUT_FILTER_CATEGORY_HIDDEN:
            return !pins_shortcut_is_shown (file);
        case PINS_SHORTCUT_FILTER_CATEGORY_AUTOSTART:
            return pins_shortcut_is_autostart (file);
        default:
            g_warning ("Invalid PinsShortcutFilterCategory");
            return FALSE;
        }
}

int
sort_compare_func (gconstpointer a, gconstpointer b, gpointer user_data)
{
    PinsShortcut *first = PINS_SHORTCUT ((gpointer)a);
    PinsShortcut *second = PINS_SHORTCUT ((gpointer)b);
    const gchar *first_key, *second_key, *first_name, *second_name;

    g_return_val_if_fail (PINS_IS_SHORTCUT (first), 0);
    g_return_val_if_fail (PINS_IS_SHORTCUT (second), 0);

    first_key = _pins_join_key_locale (
        G_KEY_FILE_DESKTOP_KEY_NAME,
        pins_shortcut_get_locale_for_key (first, G_KEY_FILE_DESKTOP_KEY_NAME));
    first_name = pins_shortcut_get_string (first, first_key);

    second_key = _pins_join_key_locale (
        G_KEY_FILE_DESKTOP_KEY_NAME, pins_shortcut_get_locale_for_key (
                                         second, G_KEY_FILE_DESKTOP_KEY_NAME));
    second_name = pins_shortcut_get_string (second, second_key);

    /// TODO: Use UTF8 compare
    return g_strcmp0 (first_name, second_name);
}

static void
pins_shortcut_filter_dispose (GObject *object)
{
    PinsShortcutFilter *self = PINS_SHORTCUT_FILTER (object);

    g_clear_object (&self->category_filter);
    g_clear_object (&self->search_filter);
    g_clear_object (&self->sort_model);
    g_clear_object (&self->category_model);
    g_clear_object (&self->search_model);
}

static void
pins_shortcut_filter_get_property (GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec)
{
    PinsShortcutFilter *self = PINS_SHORTCUT_FILTER (object);

    switch (prop_id)
        {
        case PROP_SHOW_ALL_APPS:
            g_value_set_boolean (value, self->show_all_apps);
            break;
        case PROP_CATEGORY:
            g_value_set_uint (value, self->category);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
pins_shortcut_filter_set_property (GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec)
{
    PinsShortcutFilter *self = PINS_SHORTCUT_FILTER (object);

    switch (prop_id)
        {
        case PROP_SHOW_ALL_APPS:
            self->show_all_apps = g_value_get_boolean (value);
            pins_shortcut_filter_reset_category (self);
            break;
        case PROP_CATEGORY:
            self->category = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
pins_shortcut_filter_class_init (PinsShortcutFilterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = pins_shortcut_filter_dispose;
    object_class->get_property = pins_shortcut_filter_get_property;
    object_class->set_property = pins_shortcut_filter_set_property;

    properties[PROP_SHOW_ALL_APPS] = g_param_spec_boolean (
        "show-all-apps", "Show All Apps",
        "Whether all apps are shown when no other filters are applied", FALSE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_CATEGORY] = g_param_spec_uint (
        "category", "Category", "Category of apps to be shown",
        PINS_SHORTCUT_FILTER_CATEGORY_ALL,
        PINS_SHORTCUT_FILTER_CATEGORY_AUTOSTART,
        PINS_SHORTCUT_FILTER_CATEGORY_VISIBLE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
pins_shortcut_filter_init (PinsShortcutFilter *self)
{
    self->category_filter
        = gtk_custom_filter_new (&category_match_func, self, NULL);

    self->search_filter = gtk_string_filter_new (gtk_property_expression_new (
        PINS_TYPE_SHORTCUT, NULL, "search-string"));

    self->sort_model = gtk_sort_list_model_new (
        NULL,
        GTK_SORTER (gtk_custom_sorter_new (&sort_compare_func, NULL, NULL)));

    self->category_model = gtk_filter_list_model_new (
        G_LIST_MODEL (self->sort_model), GTK_FILTER (self->category_filter));

    self->search_model = gtk_filter_list_model_new (
        G_LIST_MODEL (self->category_model), GTK_FILTER (self->search_filter));

    g_signal_connect_object (self->search_model, "items-changed",
                             G_CALLBACK (g_list_model_items_changed), self,
                             G_CONNECT_SWAPPED);

    g_signal_connect_object (self, "notify::show-all-apps",
                             G_CALLBACK (category_notify_cb), self, 0);
    g_signal_connect_object (self, "notify::category",
                             G_CALLBACK (category_notify_cb), self, 0);
}

gpointer
pins_shortcut_filter_get_item (GListModel *list, guint position)
{
    PinsShortcutFilter *self = PINS_SHORTCUT_FILTER (list);

    return g_list_model_get_item (G_LIST_MODEL (self->search_model), position);
}

GType
pins_shortcut_filter_get_item_type (GListModel *list)
{
    return PINS_TYPE_SHORTCUT;
}

guint
pins_shortcut_filter_get_n_items (GListModel *list)
{
    PinsShortcutFilter *self = PINS_SHORTCUT_FILTER (list);

    return g_list_model_get_n_items (G_LIST_MODEL (self->search_model));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
    iface->get_item = pins_shortcut_filter_get_item;
    iface->get_item_type = pins_shortcut_filter_get_item_type;
    iface->get_n_items = pins_shortcut_filter_get_n_items;
}
