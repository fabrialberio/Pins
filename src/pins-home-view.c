/* pins-home-view.c
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

#include "pins-home-view.h"

#include "pins-shortcut-filter.h"
#include "pins-shortcut.h"
#include "pins-shortcuts-grid.h"

struct _PinsHomeView
{
    AdwBin parent_instance;

    PinsShortcutFilter *shortcut_filter;

    GtkSearchBar *search_bar;
    GtkSearchEntry *search_entry;
    GtkToggleButton *edited_search_chip;
    GtkToggleButton *system_search_chip;
    GtkToggleButton *hidden_search_chip;
    GtkToggleButton *autostart_search_chip;
    AdwViewStack *view_stack;
    PinsShortcutsGrid *shortcuts_grid;
};

G_DEFINE_TYPE (PinsHomeView, pins_home_view, ADW_TYPE_BIN);

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

static guint signals[N_SIGNALS];
static gchar *pages[N_PAGES] = {
    "apps",
    "empty",
    "loading",
};

void
app_iterator_loading_cb (PinsHomeView *self, gboolean is_loading)
{
    g_assert (PINS_IS_HOME_VIEW (self));

    if (is_loading)
        adw_view_stack_set_visible_child_name (self->view_stack,
                                               pages[PAGE_LOADING]);
    else
        adw_view_stack_set_visible_child_name (self->view_stack,
                                               pages[PAGE_APPS]);
}

void
app_iterator_file_created_cb (PinsHomeView *self, PinsShortcut *shortcut)
{
    g_signal_emit (self, signals[ACTIVATE], 0, shortcut);
}

void
pins_home_view_set_app_iterator (PinsHomeView *self,
                                 PinsAppIterator *app_iterator)
{
    adw_view_stack_set_visible_child_name (self->view_stack,
                                           pages[PAGE_LOADING]);

    pins_shortcut_filter_set_model (self->shortcut_filter,
                                    G_LIST_MODEL (app_iterator));

    pins_shortcuts_grid_set_model (self->shortcuts_grid,
                                   G_LIST_MODEL (self->shortcut_filter));

    g_signal_connect_object (app_iterator, "loading",
                             G_CALLBACK (app_iterator_loading_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (app_iterator, "file-created",
                             G_CALLBACK (app_iterator_file_created_cb), self,
                             G_CONNECT_SWAPPED);

    pins_app_iterator_load (app_iterator);
}

static void
pins_home_view_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), PINS_TYPE_HOME_VIEW);

    G_OBJECT_CLASS (pins_home_view_parent_class)->dispose (object);
}

static void
pins_home_view_class_init (PinsHomeViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_home_view_dispose;

    signals[ACTIVATE] = g_signal_new ("activate", G_TYPE_FROM_CLASS (klass),
                                      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                      G_TYPE_NONE, 1, G_TYPE_OBJECT);

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-home-view.ui");
    g_type_ensure (PINS_TYPE_SHORTCUTS_GRID);

    gtk_widget_class_bind_template_child (widget_class, PinsHomeView,
                                          search_bar);
    gtk_widget_class_bind_template_child (widget_class, PinsHomeView,
                                          search_entry);
    gtk_widget_class_bind_template_child (widget_class, PinsHomeView,
                                          edited_search_chip);
    gtk_widget_class_bind_template_child (widget_class, PinsHomeView,
                                          system_search_chip);
    gtk_widget_class_bind_template_child (widget_class, PinsHomeView,
                                          hidden_search_chip);
    gtk_widget_class_bind_template_child (widget_class, PinsHomeView,
                                          autostart_search_chip);
    gtk_widget_class_bind_template_child (widget_class, PinsHomeView,
                                          view_stack);
    gtk_widget_class_bind_template_child (widget_class, PinsHomeView,
                                          shortcuts_grid);
}

gboolean
search_chip_transform_to_func (GBinding *binding, const GValue *from_value,
                               GValue *to_value, gpointer user_data)
{
    g_autoptr (PinsShortcutFilter) shorctut_filter
        = PINS_SHORTCUT_FILTER (g_binding_dup_target (binding));

    PinsShortcutFilterCategory category = GPOINTER_TO_INT (user_data);

    if (g_value_get_boolean (from_value))
        {
            g_value_set_uint (to_value, category);
            return TRUE;
        }
    else
        {
            pins_shortcut_filter_reset_category (shorctut_filter);
            // shortcut_filter->category has already been updated.
            return FALSE;
        }
}

gboolean
search_chip_transform_from_func (GBinding *binding, const GValue *from_value,
                                 GValue *to_value, gpointer user_data)
{
    PinsShortcutFilterCategory category = GPOINTER_TO_INT (user_data);

    g_value_set_boolean (to_value, g_value_get_uint (from_value) == category);

    return TRUE;
}

void
pins_home_view_items_changed_cb (GListModel *list, guint position,
                                 guint removed, guint added,
                                 PinsHomeView *self)
{
    g_assert (PINS_IS_HOME_VIEW (self));

    if (g_list_model_get_n_items (G_LIST_MODEL (self->shortcut_filter)) == 0)
        adw_view_stack_set_visible_child_name (self->view_stack,
                                               pages[PAGE_EMPTY]);
    else
        adw_view_stack_set_visible_child_name (self->view_stack,
                                               pages[PAGE_APPS]);
}

void
pins_home_view_search_changed_cb (GtkSearchEntry *entry, PinsHomeView *self)
{
    g_assert (PINS_IS_HOME_VIEW (self));

    pins_shortcut_filter_set_search (
        self->shortcut_filter, gtk_editable_get_text (GTK_EDITABLE (entry)));
}

void
pins_home_view_search_mode_notify_cb (GtkSearchBar *search_bar,
                                      GParamSpec *pspec, PinsHomeView *self)
{
    if (!gtk_search_bar_get_search_mode (search_bar))
        pins_shortcut_filter_reset_category (self->shortcut_filter);
}

void
pins_home_view_item_activated_cb (GtkListView *self, guint position,
                                  PinsHomeView *user_data)
{
    g_autoptr (PinsShortcut) shortcut = NULL;

    g_assert (PINS_IS_HOME_VIEW (user_data));

    shortcut = g_list_model_get_item (
        G_LIST_MODEL (user_data->shortcut_filter), position);

    g_signal_emit (user_data, signals[ACTIVATE], 0, shortcut);
}

static void
pins_home_view_init (PinsHomeView *self)
{
    g_autoptr (GSettings) settings = NULL;
    g_autoptr (GSimpleActionGroup) group = NULL;
    g_autoptr (GAction) action = NULL;

    settings = g_settings_new ("io.github.fabrialberio.pinapp");
    group = g_simple_action_group_new ();
    action = g_settings_create_action (settings, "show-all-apps");

    g_action_map_add_action (G_ACTION_MAP (group), action);
    gtk_widget_insert_action_group (GTK_WIDGET (self), "home-view",
                                    G_ACTION_GROUP (group));

    gtk_widget_init_template (GTK_WIDGET (self));

    self->shortcut_filter = pins_shortcut_filter_new ();

    g_settings_bind (settings, "show-all-apps", self->shortcut_filter,
                     "show-all-apps", G_SETTINGS_BIND_DEFAULT);

    adw_view_stack_set_visible_child_name (self->view_stack,
                                           pages[PAGE_LOADING]);

    gtk_search_bar_connect_entry (self->search_bar,
                                  GTK_EDITABLE (self->search_entry));

    g_object_bind_property_full (
        self->edited_search_chip, "active", self->shortcut_filter, "category",
        G_BINDING_BIDIRECTIONAL, search_chip_transform_to_func,
        search_chip_transform_from_func,
        GINT_TO_POINTER (PINS_SHORTCUT_FILTER_CATEGORY_EDITED), NULL);
    g_object_bind_property_full (
        self->system_search_chip, "active", self->shortcut_filter, "category",
        G_BINDING_BIDIRECTIONAL, search_chip_transform_to_func,
        search_chip_transform_from_func,
        GINT_TO_POINTER (PINS_SHORTCUT_FILTER_CATEGORY_SYSTEM), NULL);
    g_object_bind_property_full (
        self->hidden_search_chip, "active", self->shortcut_filter, "category",
        G_BINDING_BIDIRECTIONAL, search_chip_transform_to_func,
        search_chip_transform_from_func,
        GINT_TO_POINTER (PINS_SHORTCUT_FILTER_CATEGORY_HIDDEN), NULL);
    g_object_bind_property_full (
        self->autostart_search_chip, "active", self->shortcut_filter,
        "category", G_BINDING_BIDIRECTIONAL, search_chip_transform_to_func,
        search_chip_transform_from_func,
        GINT_TO_POINTER (PINS_SHORTCUT_FILTER_CATEGORY_AUTOSTART), NULL);

    g_signal_connect_object (self->shortcut_filter, "items-changed",
                             G_CALLBACK (pins_home_view_items_changed_cb),
                             self, 0);
    g_signal_connect_object (self->search_entry, "search-changed",
                             G_CALLBACK (pins_home_view_search_changed_cb),
                             self, 0);
    g_signal_connect_object (self->search_bar, "notify::search-mode-enabled",
                             G_CALLBACK (pins_home_view_search_mode_notify_cb),
                             self, 0);
    g_signal_connect_object (self->shortcuts_grid, "activate",
                             G_CALLBACK (pins_home_view_item_activated_cb),
                             self, 0);
}
