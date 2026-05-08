/* pins-shortcut-view.c
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

#include <glib/gi18n.h>

#include "pins-shortcut-view.h"

#include "pins-add-key-dialog.h"
#include "pins-key-row.h"
#include "pins-locale-utils-private.h"
#include "pins-pick-icon-popover.h"
#include "pins-shortcut-icon.h"

struct _PinsShortcutView
{
    AdwBin parent_instance;

    PinsShortcut *shortcut;
    GFile *opened_from_file;
    gchar **keys;

    AdwWindowTitle *window_title;
    GtkScrolledWindow *scrolled_window;
    PinsShortcutIcon *icon;
    GtkButton *reset_icon_button;
    PinsPickIconPopover *pick_icon_popover;
    PinsKeyRow *name_row;
    PinsKeyRow *comment_row;
    GtkSwitch *autostart_switch;
    GtkSwitch *invisible_switch;
    GtkListBox *keys_listbox;
    AdwButtonRow *add_key_button;
    GtkButton *delete_button;
    AdwBreakpoint *breakpoint;
};

G_DEFINE_TYPE (PinsShortcutView, pins_shortcut_view, ADW_TYPE_BREAKPOINT_BIN);

enum
{
    DUPLICATE,
    POP_REQUEST,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

void
pins_shortcut_view_setup_row (PinsKeyRow *row, PinsShortcut *shortcut,
                              gchar *key, gchar **all_keys,
                              gchar **all_locales)
{
    gchar **locales = { NULL };

    if (_pins_key_has_locales (all_keys, key))
        locales = all_locales;

    pins_key_row_set_key (row, shortcut, key, locales);
}

void
pins_shortcut_view_update_title (PinsShortcutView *self)
{
    const gchar *title_key;
    GtkAdjustment *adjustment
        = gtk_scrolled_window_get_vadjustment (self->scrolled_window);

    title_key = _pins_join_key_locale (
        G_KEY_FILE_DESKTOP_KEY_NAME,
        pins_shortcut_get_locale_for_key (self->shortcut,
                                          G_KEY_FILE_DESKTOP_KEY_NAME));

    if (gtk_adjustment_get_value (adjustment) > 0)
        adw_window_title_set_title (
            self->window_title,
            pins_shortcut_get_string (self->shortcut, title_key));
    else
        adw_window_title_set_title (self->window_title, "");
}

void
pins_shortcut_view_update_reset_icon_button_visible (PinsShortcutView *self)
{
    gboolean icon_edited = pins_shortcut_is_key_edited (
        self->shortcut, G_KEY_FILE_DESKTOP_KEY_ICON);

    gtk_widget_set_visible (
        GTK_WIDGET (self->reset_icon_button),
        icon_edited && !pins_shortcut_is_user_only (self->shortcut));
}

void
pins_shortcut_view_focus_key_row (PinsShortcutView *self, gchar *key)
{
    GtkListBoxRow *row;
    g_auto (PinsSplitKey) split, current;

    split = _pins_split_key_locale (key);
    key = split.key;

    for (int i = 0;
         (row = gtk_list_box_get_row_at_index (self->keys_listbox, i)) != NULL;
         i++)
        {
            current = _pins_split_key_locale (
                pins_key_row_get_key (PINS_KEY_ROW (row)));

            if (!g_strcmp0 (current.key, key))
                {
                    gtk_widget_grab_focus (GTK_WIDGET (row));

                    if (split.locale != NULL)
                        pins_key_row_set_locale (PINS_KEY_ROW (row),
                                                 split.locale);
                }
        }
}

void
autostart_switch_state_set_cb (PinsShortcutView *self, gboolean state)
{
    pins_shortcut_set_autostart (self->shortcut, state);
    gtk_switch_set_active (self->autostart_switch, state);
}

void
invisible_switch_state_set_cb (PinsShortcutView *self, gboolean state)
{
    pins_shortcut_set_boolean (self->shortcut,
                               G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, state);
    gtk_switch_set_active (self->invisible_switch, state);
}

void
pins_shortcut_view_key_set_cb (PinsShortcut *shortcut, gchar *key,
                               PinsShortcutView *self)
{
    g_assert (PINS_IS_SHORTCUT_VIEW (self));

    if (!g_strv_contains ((const gchar *const *)self->keys, key))
        {
            pins_shortcut_view_set_shortcut (self, self->shortcut,
                                             self->opened_from_file);
            pins_shortcut_view_focus_key_row (self, key);
        }

    if (!g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NAME))
        pins_shortcut_view_update_title (self);
    else if (!g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY))
        {
            gboolean value = pins_shortcut_get_boolean (
                self->shortcut, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY);

            if (gtk_switch_get_state (self->invisible_switch) != value)
                {
                    g_signal_handlers_block_by_func (
                        self->invisible_switch, invisible_switch_state_set_cb,
                        self);

                    gtk_switch_set_active (self->invisible_switch, value);

                    g_signal_handlers_unblock_by_func (
                        self->invisible_switch, invisible_switch_state_set_cb,
                        self);
                }
        }
    else if (!g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_ICON))
        pins_shortcut_view_update_reset_icon_button_visible (self);
}

void
pins_shortcut_view_key_removed_cb (PinsShortcut *shortcut, gchar *key,
                                   PinsShortcutView *self)
{
    pins_shortcut_view_set_shortcut (self, self->shortcut,
                                     self->opened_from_file);
}

void
pins_shortcut_view_setup_keys_listbox (PinsShortcutView *self)
{
    g_auto (GStrv) locales = _pins_locales_from_keys (self->keys);
    GHashTable *added_keys = g_hash_table_new (g_str_hash, g_str_equal);

    gtk_list_box_remove_all (self->keys_listbox);

    pins_shortcut_view_setup_row (self->name_row, self->shortcut,
                                  G_KEY_FILE_DESKTOP_KEY_NAME, self->keys,
                                  locales);
    pins_shortcut_view_setup_row (self->comment_row, self->shortcut,
                                  G_KEY_FILE_DESKTOP_KEY_COMMENT, self->keys,
                                  locales);

    g_hash_table_add (added_keys, g_strdup (G_KEY_FILE_DESKTOP_KEY_NAME));
    g_hash_table_add (added_keys, g_strdup (G_KEY_FILE_DESKTOP_KEY_COMMENT));

    for (int i = 0; i < g_strv_length (self->keys); i++)
        {
            gchar *current_key = _pins_split_key_locale (self->keys[i]).key;
            PinsKeyRow *row;

            if (g_hash_table_contains (added_keys, current_key))
                continue;

            g_hash_table_add (added_keys, current_key);

            row = pins_key_row_new ();
            pins_shortcut_view_setup_row (row, self->shortcut,
                                          g_strdup (current_key), self->keys,
                                          locales);

            gtk_list_box_append (self->keys_listbox, GTK_WIDGET (row));
        }

    g_hash_table_foreach (added_keys, (GHFunc)g_free, NULL);
    g_hash_table_destroy (added_keys);
}

void
pins_shortcut_view_set_shortcut (PinsShortcutView *self,
                                 PinsShortcut *shortcut,
                                 GFile *opened_from_file)
{
    if (self->shortcut != NULL)
        {
            g_signal_handlers_disconnect_by_func (
                self->shortcut, pins_shortcut_view_key_set_cb, self);
            g_signal_handlers_disconnect_by_func (
                self->shortcut, pins_shortcut_view_key_removed_cb, self);
            g_signal_handlers_disconnect_by_func (
                self->shortcut, autostart_switch_state_set_cb, self);
            g_signal_handlers_disconnect_by_func (
                self->shortcut, invisible_switch_state_set_cb, self);
        }

    self->shortcut = g_object_ref (shortcut);
    self->opened_from_file = opened_from_file;
    self->keys = pins_shortcut_get_keys (self->shortcut);

    pins_shortcut_view_update_title (self);
    pins_shortcut_view_update_reset_icon_button_visible (self);
    pins_shortcut_icon_set_shortcut (self->icon, self->shortcut);
    pins_pick_icon_popover_set_shortcut (self->pick_icon_popover,
                                         self->shortcut);
    gtk_switch_set_active (self->autostart_switch,
                           pins_shortcut_is_autostart (self->shortcut));
    gtk_switch_set_active (
        self->invisible_switch,
        pins_shortcut_get_boolean (self->shortcut,
                                   G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY));

    g_signal_connect_object (self->shortcut, "key-set",
                             G_CALLBACK (pins_shortcut_view_key_set_cb), self,
                             0);
    g_signal_connect_object (self->shortcut, "key-removed",
                             G_CALLBACK (pins_shortcut_view_key_removed_cb),
                             self, 0);
    g_signal_connect_object (self->autostart_switch, "state-set",
                             G_CALLBACK (autostart_switch_state_set_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->invisible_switch, "state-set",
                             G_CALLBACK (invisible_switch_state_set_cb), self,
                             G_CONNECT_SWAPPED);

    gtk_widget_set_visible (GTK_WIDGET (self->delete_button),
                            pins_shortcut_is_user_only (self->shortcut)
                                && self->opened_from_file == NULL);

    pins_shortcut_view_setup_keys_listbox (self);
}

PinsShortcut *
pins_shortcut_view_get_shortcut (PinsShortcutView *self)
{
    return self->shortcut;
}

static void
pins_shortcut_view_dispose (GObject *object)
{
    PinsShortcutView *self = PINS_SHORTCUT_VIEW (object);

    g_clear_object (&self->shortcut);

    gtk_widget_dispose_template (GTK_WIDGET (object), PINS_TYPE_SHORTCUT_VIEW);

    G_OBJECT_CLASS (pins_shortcut_view_parent_class)->dispose (object);
}

static void
pins_shortcut_view_class_init (PinsShortcutViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_shortcut_view_dispose;

    signals[DUPLICATE] = g_signal_new ("duplicate", G_TYPE_FROM_CLASS (klass),
                                       G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                       G_TYPE_NONE, 1, G_TYPE_OBJECT);
    signals[POP_REQUEST] = g_signal_new (
        "pop-request", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 0);

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-shortcut-view.ui");
    g_type_ensure (PINS_TYPE_SHORTCUT_ICON);
    g_type_ensure (PINS_TYPE_PICK_ICON_POPOVER);
    g_type_ensure (PINS_TYPE_KEY_ROW);

    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          window_title);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          scrolled_window);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          icon);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          pick_icon_popover);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          reset_icon_button);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          name_row);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          comment_row);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          autostart_switch);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          invisible_switch);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          keys_listbox);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          add_key_button);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          delete_button);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutView,
                                          breakpoint);
}

void
pins_shortcut_view_open_with_cb (GSimpleAction *action, GVariant *param,
                                 PinsShortcutView *self)
{
    GtkFileLauncher *launcher
        = gtk_file_launcher_new (pins_shortcut_get_user_file (self->shortcut));

    gtk_file_launcher_set_always_ask (launcher, TRUE);
    gtk_file_launcher_launch (
        launcher, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))), NULL,
        (void (*))gtk_file_launcher_launch_finish, NULL);

    g_signal_emit (self, signals[POP_REQUEST], 0);
}

void
pins_shortcut_view_open_folder_cb (GSimpleAction *action, GVariant *param,
                                   PinsShortcutView *self)
{
    GtkFileLauncher *launcher
        = gtk_file_launcher_new (pins_shortcut_get_user_file (self->shortcut));

    gtk_file_launcher_open_containing_folder (
        launcher, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))), NULL,
        (void (*))gtk_file_launcher_open_containing_folder_finish, NULL);
}

void
pins_shortcut_view_duplicate_cb (GSimpleAction *action, GVariant *param,
                                 PinsShortcutView *self)
{
    g_signal_emit (self, signals[DUPLICATE], 0, self->shortcut);
    g_signal_emit (self, signals[POP_REQUEST], 0);
}

void
reset_icon_button_clicked_cb (PinsShortcutView *self)
{
    pins_shortcut_reset_key (self->shortcut, G_KEY_FILE_DESKTOP_KEY_ICON);
}

void
add_key_button_clicked_cb (PinsShortcutView *self)
{
    PinsAddKeyDialog *dialog = pins_add_key_dialog_new (self->shortcut);

    adw_dialog_present (ADW_DIALOG (dialog),
                        GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));
}

void
delete_button_clicked_cb (PinsShortcutView *self)
{
    pins_shortcut_trash (self->shortcut);
}

void
breakpoint_apply_cb (PinsShortcutView *self)
{
    gtk_widget_remove_css_class (GTK_WIDGET (self->name_row), "title-1-row");
    gtk_widget_add_css_class (GTK_WIDGET (self->name_row), "title-2-row");
}

void
breakpoint_unapply_cb (PinsShortcutView *self)
{
    gtk_widget_remove_css_class (GTK_WIDGET (self->name_row), "title-2-row");
    gtk_widget_add_css_class (GTK_WIDGET (self->name_row), "title-1-row");
}

static void
pins_shortcut_view_init (PinsShortcutView *self)
{
    g_autoptr (GSimpleActionGroup) group = NULL;
    g_autoptr (GSimpleAction) open_with_action = NULL,
                              open_folder_action = NULL,
                              duplicate_action = NULL;

    group = g_simple_action_group_new ();

    open_with_action = g_simple_action_new ("open-with", NULL);
    g_signal_connect_object (open_with_action, "activate",
                             G_CALLBACK (pins_shortcut_view_open_with_cb),
                             self, 0);
    g_action_map_add_action (G_ACTION_MAP (group),
                             G_ACTION (open_with_action));

    open_folder_action = g_simple_action_new ("open-folder", NULL);
    g_signal_connect_object (open_folder_action, "activate",
                             G_CALLBACK (pins_shortcut_view_open_folder_cb),
                             self, 0);
    g_action_map_add_action (G_ACTION_MAP (group),
                             G_ACTION (open_folder_action));

    duplicate_action = g_simple_action_new ("duplicate", NULL);
    g_signal_connect_object (duplicate_action, "activate",
                             G_CALLBACK (pins_shortcut_view_duplicate_cb),
                             self, 0);
    g_action_map_add_action (G_ACTION_MAP (group),
                             G_ACTION (duplicate_action));

    gtk_widget_insert_action_group (GTK_WIDGET (self), "file",
                                    G_ACTION_GROUP (group));

    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect_object (self->reset_icon_button, "clicked",
                             G_CALLBACK (reset_icon_button_clicked_cb), self,
                             G_CONNECT_SWAPPED);

    g_signal_connect_object (self->add_key_button, "activated",
                             G_CALLBACK (add_key_button_clicked_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->delete_button, "clicked",
                             G_CALLBACK (delete_button_clicked_cb), self,
                             G_CONNECT_SWAPPED);

    g_signal_connect_object (self->breakpoint, "apply",
                             G_CALLBACK (breakpoint_apply_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->breakpoint, "unapply",
                             G_CALLBACK (breakpoint_unapply_cb), self,
                             G_CONNECT_SWAPPED);

    g_signal_connect_object (
        gtk_scrolled_window_get_vadjustment (self->scrolled_window),
        "value-changed", G_CALLBACK (pins_shortcut_view_update_title), self,
        G_CONNECT_SWAPPED);
}
