/* pins-file-view.c
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

#include "pins-file-view.h"

#include "pins-add-key-dialog.h"
#include "pins-app-icon.h"
#include "pins-key-row.h"
#include "pins-locale-utils-private.h"
#include "pins-pick-icon-dialog.h"

struct _PinsFileView
{
    AdwBin parent_instance;

    PinsDesktopFile *desktop_file;
    gchar **keys;

    AdwHeaderBar *header_bar;
    AdwWindowTitle *window_title;
    GtkScrolledWindow *scrolled_window;
    PinsAppIcon *icon;
    GtkButton *reset_icon_button;
    GtkButton *pick_icon_button;
    PinsKeyRow *name_row;
    PinsKeyRow *comment_row;
    GtkSwitch *autostart_switch;
    GtkSwitch *invisible_switch;
    GtkListBox *keys_listbox;
    AdwButtonRow *add_key_button;
    GtkButton *delete_button;
    AdwBreakpoint *breakpoint;
};

G_DEFINE_TYPE (PinsFileView, pins_file_view, ADW_TYPE_BREAKPOINT_BIN);

void
pins_file_view_setup_row (PinsKeyRow *row, PinsDesktopFile *desktop_file,
                          gchar *key, gchar **all_keys, gchar **all_locales)
{
    gchar **locales = { NULL };

    if (_pins_key_has_locales (all_keys, key))
        locales = all_locales;

    pins_key_row_set_key (row, desktop_file, key, locales);
}

void
pins_file_view_update_title (PinsFileView *self)
{
    adw_window_title_set_title (
        self->window_title,
        pins_desktop_file_get_string (self->desktop_file,
                                      G_KEY_FILE_DESKTOP_KEY_NAME));
}

void
pins_file_view_update_reset_icon_button_visible (PinsFileView *self)
{
    gboolean icon_edited = pins_desktop_file_is_key_edited (
        self->desktop_file, G_KEY_FILE_DESKTOP_KEY_ICON);

    gtk_widget_set_visible (
        GTK_WIDGET (self->reset_icon_button),
        icon_edited && !pins_desktop_file_is_user_only (self->desktop_file));
}

void
pins_file_view_focus_key_row (PinsFileView *self, gchar *key)
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
autostart_switch_state_set_cb (PinsFileView *self, gboolean state)
{
    pins_desktop_file_set_autostart (self->desktop_file, state);
    gtk_switch_set_active (self->autostart_switch, state);
}

void
invisible_switch_state_set_cb (PinsFileView *self, gboolean state)
{
    pins_desktop_file_set_boolean (self->desktop_file,
                                   G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, state);
    gtk_switch_set_active (self->invisible_switch, state);
}

void
pins_file_view_key_set_cb (PinsDesktopFile *desktop_file, gchar *key,
                           PinsFileView *self)
{
    g_assert (PINS_IS_FILE_VIEW (self));

    if (!g_strv_contains ((const gchar *const *)self->keys, key))
        {
            pins_file_view_set_desktop_file (self, self->desktop_file);
            pins_file_view_focus_key_row (self, key);
        }

    if (!g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NAME))
        pins_file_view_update_title (self);
    else if (!g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY))
        {
            gboolean value = pins_desktop_file_get_boolean (
                self->desktop_file, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY);

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
        pins_file_view_update_reset_icon_button_visible (self);
}

void
pins_file_view_key_removed_cb (PinsDesktopFile *desktop_file, gchar *key,
                               PinsFileView *self)
{
    pins_file_view_set_desktop_file (self, self->desktop_file);
}

void
pins_file_view_update_title_visible_cb (GtkAdjustment *adjustment,
                                        PinsFileView *self)
{
    g_assert (PINS_IS_FILE_VIEW (self));

    adw_header_bar_set_show_title (self->header_bar,
                                   gtk_adjustment_get_value (adjustment) > 0);
}

void
pins_file_view_setup_keys_listbox (PinsFileView *self)
{
    g_auto (GStrv) locales = _pins_locales_from_keys (self->keys);
    GHashTable *added_keys = g_hash_table_new (g_str_hash, g_str_equal);

    gtk_list_box_remove_all (self->keys_listbox);

    pins_file_view_setup_row (self->name_row, self->desktop_file,
                              G_KEY_FILE_DESKTOP_KEY_NAME, self->keys,
                              locales);
    pins_file_view_setup_row (self->comment_row, self->desktop_file,
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
            pins_file_view_setup_row (row, self->desktop_file,
                                      g_strdup (current_key), self->keys,
                                      locales);

            gtk_list_box_append (self->keys_listbox, GTK_WIDGET (row));
        }

    g_hash_table_foreach (added_keys, (GHFunc)g_free, NULL);
    g_hash_table_destroy (added_keys);
}

void
pins_file_view_set_desktop_file (PinsFileView *self,
                                 PinsDesktopFile *desktop_file)
{
    if (self->desktop_file != NULL)
        {
            g_signal_handlers_disconnect_by_func (
                self->desktop_file, pins_file_view_key_set_cb, self);
            g_signal_handlers_disconnect_by_func (
                self->desktop_file, pins_file_view_key_removed_cb, self);
            g_signal_handlers_disconnect_by_func (
                self->desktop_file, autostart_switch_state_set_cb, self);
            g_signal_handlers_disconnect_by_func (
                self->desktop_file, invisible_switch_state_set_cb, self);
        }

    self->desktop_file = g_object_ref (desktop_file);
    self->keys = pins_desktop_file_get_keys (self->desktop_file);

    pins_file_view_update_title (self);
    pins_file_view_update_reset_icon_button_visible (self);
    pins_app_icon_set_desktop_file (self->icon, self->desktop_file);
    gtk_switch_set_active (
        self->autostart_switch,
        pins_desktop_file_is_autostart (self->desktop_file));
    gtk_switch_set_active (
        self->invisible_switch,
        pins_desktop_file_get_boolean (self->desktop_file,
                                       G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY));

    g_signal_connect_object (self->desktop_file, "key-set",
                             G_CALLBACK (pins_file_view_key_set_cb), self, 0);
    g_signal_connect_object (self->desktop_file, "key-removed",
                             G_CALLBACK (pins_file_view_key_removed_cb), self,
                             0);
    g_signal_connect_object (self->autostart_switch, "state-set",
                             G_CALLBACK (autostart_switch_state_set_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->invisible_switch, "state-set",
                             G_CALLBACK (invisible_switch_state_set_cb), self,
                             G_CONNECT_SWAPPED);

    /// TODO: Deleting files opened from a folder doesn't work
    gtk_widget_set_visible (
        GTK_WIDGET (self->delete_button),
        pins_desktop_file_is_user_only (self->desktop_file));

    pins_file_view_setup_keys_listbox (self);
}

PinsDesktopFile *
pins_file_view_get_desktop_file (PinsFileView *self)
{
    return self->desktop_file;
}

static void
pins_file_view_dispose (GObject *object)
{
    PinsFileView *self = PINS_FILE_VIEW (object);

    g_clear_object (&self->desktop_file);

    gtk_widget_dispose_template (GTK_WIDGET (object), PINS_TYPE_FILE_VIEW);

    G_OBJECT_CLASS (pins_file_view_parent_class)->dispose (object);
}

static void
pins_file_view_class_init (PinsFileViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_file_view_dispose;

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-file-view.ui");
    g_type_ensure (PINS_TYPE_APP_ICON);
    g_type_ensure (PINS_TYPE_KEY_ROW);

    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          header_bar);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          window_title);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          scrolled_window);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView, icon);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          pick_icon_button);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          reset_icon_button);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          name_row);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          comment_row);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          autostart_switch);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          invisible_switch);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          keys_listbox);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          add_key_button);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          delete_button);
    gtk_widget_class_bind_template_child (widget_class, PinsFileView,
                                          breakpoint);
}

void
reset_icon_button_clicked_cb (PinsFileView *self)
{
    pins_desktop_file_reset_key (self->desktop_file,
                                 G_KEY_FILE_DESKTOP_KEY_ICON);
}

void
pick_icon_button_clicked_cb (PinsFileView *self)
{
    PinsPickIconDialog *dialog
        = pins_pick_icon_dialog_new (self->desktop_file);

    adw_dialog_present (ADW_DIALOG (dialog),
                        GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));
}

void
add_key_button_clicked_cb (PinsFileView *self)
{
    PinsAddKeyDialog *dialog = pins_add_key_dialog_new (self->desktop_file);

    adw_dialog_present (ADW_DIALOG (dialog),
                        GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));
}

void
delete_button_clicked_cb (PinsFileView *self)
{
    pins_desktop_file_trash (self->desktop_file);
}

void
breakpoint_apply_cb (PinsFileView *self)
{
    gtk_widget_remove_css_class (GTK_WIDGET (self->name_row), "title-1-row");
    gtk_widget_add_css_class (GTK_WIDGET (self->name_row), "title-2-row");
}

void
breakpoint_unapply_cb (PinsFileView *self)
{
    gtk_widget_remove_css_class (GTK_WIDGET (self->name_row), "title-2-row");
    gtk_widget_add_css_class (GTK_WIDGET (self->name_row), "title-1-row");
}

static void
pins_file_view_init (PinsFileView *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect_object (self->reset_icon_button, "clicked",
                             G_CALLBACK (reset_icon_button_clicked_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->pick_icon_button, "clicked",
                             G_CALLBACK (pick_icon_button_clicked_cb), self,
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
        "value-changed", G_CALLBACK (pins_file_view_update_title_visible_cb),
        self, 0);
}
