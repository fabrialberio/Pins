/* pins-app-view.c
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

// Widget that handles:
// - Filtering a model for search
// - Showing loading / placeholder

#include "pins-app-view.h"

#include "pins-app-grid.h"
#include "pins-desktop-file.h"
#include "pins-locale-utils-private.h"

struct _PinsAppView
{
    AdwBin parent_instance;

    gboolean show_all_apps;
    GtkCustomFilter *show_all_apps_filter;
    GtkStringFilter *search_filter;
    GtkFilterListModel *show_all_apps_filter_model;
    GtkFilterListModel *search_filter_model;

    GtkToggleButton *search_button;
    GtkSearchBar *search_bar;
    GtkSearchEntry *search_entry;
    AdwViewStack *view_stack;
    PinsAppGrid *app_grid;
};

G_DEFINE_TYPE (PinsAppView, pins_app_view, ADW_TYPE_BIN);

enum
{
    PROP_0,
    PROP_SHOW_ALL_APPS,
    N_PROPS,
};

enum
{
    ACTIVATE,
    N_SIGNALS
};

enum
{
    PAGE_APPS,
    PAGE_EMPTY,
    PAGE_LOADING,
    N_PAGES,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];
static gchar *pages[N_PAGES] = {
    "apps",
    "empty",
    "loading",
};

void
app_iterator_loading_cb (PinsAppView *self, gboolean is_loading)
{
    g_assert (PINS_IS_APP_VIEW (self));

    if (is_loading)
        adw_view_stack_set_visible_child_name (self->view_stack,
                                               pages[PAGE_LOADING]);
    else
        adw_view_stack_set_visible_child_name (self->view_stack,
                                               pages[PAGE_APPS]);
}

void
app_iterator_file_created_cb (PinsAppView *self, PinsDesktopFile *desktop_file)
{
    g_signal_emit (self, signals[ACTIVATE], 0, desktop_file);
}

void
pins_app_view_set_app_iterator (PinsAppView *self,
                                PinsAppIterator *app_iterator)
{
    adw_view_stack_set_visible_child_name (self->view_stack,
                                           pages[PAGE_LOADING]);

    gtk_filter_list_model_set_model (
        g_object_ref (self->show_all_apps_filter_model),
        G_LIST_MODEL (app_iterator));

    pins_app_grid_set_model (self->app_grid, G_LIST_MODEL (g_object_ref (
                                                 self->search_filter_model)));

    g_signal_connect_object (app_iterator, "loading",
                             G_CALLBACK (app_iterator_loading_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (app_iterator, "file-created",
                             G_CALLBACK (app_iterator_file_created_cb), self,
                             G_CONNECT_SWAPPED);

    pins_app_iterator_load (app_iterator);
}

static void
pins_app_view_dispose (GObject *object)
{
    PinsAppView *self = PINS_APP_VIEW (object);

    g_clear_object (&self->search_filter);
    g_clear_object (&self->show_all_apps_filter);
    g_clear_object (&self->show_all_apps_filter_model);
    g_clear_object (&self->search_filter_model);

    gtk_widget_dispose_template (GTK_WIDGET (object), PINS_TYPE_APP_VIEW);

    G_OBJECT_CLASS (pins_app_view_parent_class)->dispose (object);
}

static void
pins_desktop_file_get_property (GObject *object, guint prop_id, GValue *value,
                                GParamSpec *pspec)
{
    PinsAppView *self = PINS_APP_VIEW (object);

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
pins_desktop_file_set_property (GObject *object, guint prop_id,
                                const GValue *value, GParamSpec *pspec)
{
    PinsAppView *self = PINS_APP_VIEW (object);

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
pins_app_view_class_init (PinsAppViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_app_view_dispose;
    object_class->get_property = pins_desktop_file_get_property;
    object_class->set_property = pins_desktop_file_set_property;

    properties[PROP_SHOW_ALL_APPS] = g_param_spec_boolean (
        "show-all-apps", "Show All Apps", "Whether all apps are shown", FALSE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    signals[ACTIVATE] = g_signal_new ("activate", G_TYPE_FROM_CLASS (klass),
                                      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                      G_TYPE_NONE, 1, G_TYPE_OBJECT);

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-app-view.ui");
    g_type_ensure (PINS_TYPE_APP_GRID);

    gtk_widget_class_bind_template_child (widget_class, PinsAppView,
                                          search_button);
    gtk_widget_class_bind_template_child (widget_class, PinsAppView,
                                          search_bar);
    gtk_widget_class_bind_template_child (widget_class, PinsAppView,
                                          search_entry);
    gtk_widget_class_bind_template_child (widget_class, PinsAppView,
                                          view_stack);
    gtk_widget_class_bind_template_child (widget_class, PinsAppView, app_grid);
}

void
pins_app_view_search_changed_cb (GtkSearchEntry *entry, PinsAppView *self)
{
    g_assert (PINS_IS_APP_VIEW (self));

    gtk_string_filter_set_search (
        self->search_filter, gtk_editable_get_text (GTK_EDITABLE (entry)));

    if (g_list_model_get_n_items (G_LIST_MODEL (self->search_filter_model))
        == 0)
        adw_view_stack_set_visible_child_name (self->view_stack,
                                               pages[PAGE_EMPTY]);
    else
        adw_view_stack_set_visible_child_name (self->view_stack,
                                               pages[PAGE_APPS]);
}

void
pins_app_view_item_activated_cb (GtkListView *self, guint position,
                                 PinsAppView *user_data)
{
    g_autoptr (PinsDesktopFile) desktop_file = NULL;

    g_assert (PINS_IS_APP_VIEW (user_data));

    desktop_file = g_list_model_get_item (
        G_LIST_MODEL (user_data->search_filter_model), position);

    g_signal_emit (user_data, signals[ACTIVATE], 0, desktop_file);
}

void
show_all_apps_notify_cb (PinsAppView *self, GParamSpec *pspec)
{
    GtkFilterChange change = self->show_all_apps
                                 ? GTK_FILTER_CHANGE_LESS_STRICT
                                 : GTK_FILTER_CHANGE_MORE_STRICT;

    gtk_filter_changed (GTK_FILTER (self->show_all_apps_filter), change);
}

gboolean
show_all_filter_match_func (gpointer desktop_file, gpointer user_data)
{
    PinsAppView *self = PINS_APP_VIEW (user_data);

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
pins_app_view_init (PinsAppView *self)
{
    g_autoptr (GSettings) settings = NULL;
    g_autoptr (GSimpleActionGroup) group = NULL;
    g_autoptr (GAction) action = NULL;
    GtkSortListModel *sort_model;

    settings = g_settings_new ("io.github.fabrialberio.pinapp");
    group = g_simple_action_group_new ();
    action = g_settings_create_action (settings, "show-all-apps");

    g_action_map_add_action (G_ACTION_MAP (group), action);
    gtk_widget_insert_action_group (GTK_WIDGET (self), "app-view",
                                    G_ACTION_GROUP (group));

    gtk_widget_init_template (GTK_WIDGET (self));

    g_settings_bind (settings, "show-all-apps", self, "show-all-apps",
                     G_SETTINGS_BIND_DEFAULT);

    g_signal_connect_object (self, "notify::show-all-apps",
                             G_CALLBACK (show_all_apps_notify_cb), self, 0);

    self->search_filter = gtk_string_filter_new (gtk_property_expression_new (
        PINS_TYPE_DESKTOP_FILE, NULL, "search-string"));

    self->show_all_apps_filter
        = gtk_custom_filter_new (&show_all_filter_match_func, self, NULL);

    self->show_all_apps_filter_model = gtk_filter_list_model_new (
        NULL, GTK_FILTER (self->show_all_apps_filter));

    sort_model = gtk_sort_list_model_new (
        G_LIST_MODEL (self->show_all_apps_filter_model),
        GTK_SORTER (gtk_custom_sorter_new (&sort_compare_func, NULL, NULL)));

    self->search_filter_model = gtk_filter_list_model_new (
        G_LIST_MODEL (sort_model), GTK_FILTER (self->search_filter));

    adw_view_stack_set_visible_child_name (self->view_stack,
                                           pages[PAGE_LOADING]);

    gtk_search_bar_connect_entry (self->search_bar,
                                  GTK_EDITABLE (self->search_entry));

    g_signal_connect_object (self->search_entry, "search-changed",
                             G_CALLBACK (pins_app_view_search_changed_cb),
                             self, 0);
    g_signal_connect_object (self->app_grid, "activate",
                             G_CALLBACK (pins_app_view_item_activated_cb),
                             self, 0);
}
