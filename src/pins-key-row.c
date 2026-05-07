/* pins-key-row.c
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

#include "pins-key-row.h"
#include "pins-locale-utils-private.h"

#define UNLOCALIZED_STRING _ ("(Unlocalized)")

struct _PinsKeyRow
{
    AdwEntryRow parent_instance;

    PinsShortcut *shortcut;
    gchar *key;
    gchar *unlocalized_key;
    GtkSingleSelection *locales_model;

    GtkButton *reset_button;
    GtkButton *remove_button;
    GtkMenuButton *locale_button;
    GtkPopover *locale_popover;
    GtkListView *locale_list_view;
};

G_DEFINE_TYPE (PinsKeyRow, pins_key_row, ADW_TYPE_ENTRY_ROW)

enum
{
    LOCALE_CHANGED,
    N_SIGNALS,
};

static guint signals[N_SIGNALS];

PinsKeyRow *
pins_key_row_new (void)
{
    return g_object_new (PINS_TYPE_KEY_ROW, NULL);
}

void
pins_key_row_update_reset_buttons_visibility (PinsKeyRow *self)
{
    gboolean reset_button_visible = FALSE, remove_button_visible = FALSE,
             unlocalized_with_other_locales;
    const gchar *editable_value = gtk_editable_get_text (GTK_EDITABLE (self));

    reset_button_visible
        = !pins_shortcut_is_user_only (self->shortcut)
          && pins_shortcut_is_key_edited (self->shortcut, self->key);

    unlocalized_with_other_locales
        = g_strcmp0 (self->key, self->unlocalized_key) == 0
          && gtk_widget_get_visible (GTK_WIDGET (self->locale_button));

    remove_button_visible
        = pins_shortcut_is_user_only (self->shortcut)
          && pins_shortcut_has_key (self->shortcut, self->key)
          && strlen (editable_value) == 0 && !unlocalized_with_other_locales;

    gtk_widget_set_visible (GTK_WIDGET (self->reset_button),
                            reset_button_visible);
    gtk_widget_set_visible (GTK_WIDGET (self->remove_button),
                            remove_button_visible);
}

void
pins_key_row_update_locale_button_visibility (PinsKeyRow *self)
{
    gboolean locale_button_visible;

    locale_button_visible
        = g_list_model_get_n_items (G_LIST_MODEL (self->locales_model)) > 1;

    gtk_widget_set_visible (GTK_WIDGET (self->locale_button),
                            locale_button_visible);
}

void
pins_key_row_text_changed_cb (GtkEditable *editable, PinsKeyRow *self)
{
    g_assert (PINS_IS_KEY_ROW (self));

    pins_shortcut_set_string (self->shortcut, self->key,
                              gtk_editable_get_text (editable));

    pins_key_row_update_reset_buttons_visibility (self);
}

void
pins_key_row_set_locale (PinsKeyRow *self, gchar *selected_locale)
{
    AdwButtonContent *button_content
        = ADW_BUTTON_CONTENT (gtk_menu_button_get_child (self->locale_button));

    self->key = _pins_join_key_locale (self->unlocalized_key, selected_locale);

    if (selected_locale != NULL)
        adw_button_content_set_label (button_content, selected_locale);
    else
        adw_button_content_set_label (button_content, "");

    g_signal_handlers_block_by_func (GTK_EDITABLE (self),
                                     pins_key_row_text_changed_cb, self);

    gtk_editable_set_text (
        GTK_EDITABLE (self),
        pins_shortcut_get_string (self->shortcut, self->key));

    g_signal_handlers_unblock_by_func (GTK_EDITABLE (self),
                                       pins_key_row_text_changed_cb, self);

    pins_key_row_update_reset_buttons_visibility (self);
    g_signal_emit (self, signals[LOCALE_CHANGED], 0);
}

void
pins_key_row_key_set_cb (PinsShortcut *shortcut, gchar *key, PinsKeyRow *self)
{
    gchar *shortcut_value;
    gchar *editable_value;

    if (g_strcmp0 (key, self->key) != 0)
        return;

    /// TODO: Sometimes this is slow

    shortcut_value = pins_shortcut_get_string (shortcut, key);
    editable_value = (gchar *)gtk_editable_get_text (GTK_EDITABLE (self));

    if (g_strcmp0 (shortcut_value, editable_value))
        {
            g_signal_handlers_block_by_func (
                GTK_EDITABLE (self), pins_key_row_text_changed_cb, self);

            gtk_editable_set_text (GTK_EDITABLE (self), shortcut_value);

            g_signal_handlers_unblock_by_func (
                GTK_EDITABLE (self), pins_key_row_text_changed_cb, self);

            pins_key_row_update_locale_button_visibility (self);
            pins_key_row_update_reset_buttons_visibility (self);
        }
}

void
pins_key_row_key_removed_cb (PinsShortcut *shortcut, gchar *key,
                             PinsKeyRow *self)
{
    g_auto (PinsSplitKey) split = _pins_split_key_locale (key);

    g_assert (PINS_IS_KEY_ROW (self));

    if (g_strcmp0 (key, self->key))
        return;

    if (split.locale != NULL)
        {
            if (!g_strcmp0 (split.key, self->unlocalized_key))
                {
                    pins_key_row_set_locale (self, NULL);
                    pins_key_row_update_locale_button_visibility (self);
                }
        }
}

void
pins_key_row_set_key (PinsKeyRow *self, PinsShortcut *shortcut, gchar *key,
                      gchar **locales)
{
    GtkStringList *string_list = GTK_STRING_LIST (
        gtk_single_selection_get_model (self->locales_model));

    self->shortcut = shortcut;
    self->key = key;
    self->unlocalized_key = key;

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), key);

    g_signal_connect_object (self->shortcut, "key-set",
                             G_CALLBACK (pins_key_row_key_set_cb), self, 0);
    g_signal_connect_object (self->shortcut, "key-removed",
                             G_CALLBACK (pins_key_row_key_removed_cb), self,
                             0);

    gtk_string_list_splice (
        string_list, 0, g_list_model_get_n_items (G_LIST_MODEL (string_list)),
        NULL);

    gtk_string_list_append (string_list, UNLOCALIZED_STRING);
    gtk_string_list_splice (string_list, 1, 0, (const gchar *const *)locales);

    pins_key_row_set_locale (self,
                             pins_shortcut_get_locale_for_key (shortcut, key));
    pins_key_row_update_locale_button_visibility (self);
    pins_key_row_update_reset_buttons_visibility (self);
}

gchar *
pins_key_row_get_key (PinsKeyRow *self)
{
    return self->key;
}

static void
pins_key_row_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), PINS_TYPE_KEY_ROW);

    G_OBJECT_CLASS (pins_key_row_parent_class)->dispose (object);
}

static void
pins_key_row_class_init (PinsKeyRowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_key_row_dispose;

    signals[LOCALE_CHANGED] = g_signal_new (
        "locale-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 0);

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-key-row.ui");
    gtk_widget_class_bind_template_child (widget_class, PinsKeyRow,
                                          reset_button);
    gtk_widget_class_bind_template_child (widget_class, PinsKeyRow,
                                          remove_button);
    gtk_widget_class_bind_template_child (widget_class, PinsKeyRow,
                                          locale_button);
    gtk_widget_class_bind_template_child (widget_class, PinsKeyRow,
                                          locale_popover);
    gtk_widget_class_bind_template_child (widget_class, PinsKeyRow,
                                          locale_list_view);
}

void
pins_key_row_reset_key_cb (PinsKeyRow *self, gpointer user_data)
{
    g_assert (PINS_IS_KEY_ROW (self));

    pins_shortcut_reset_key (self->shortcut, self->key);

    pins_key_row_update_reset_buttons_visibility (self);
}

void
locale_menu_item_setup_cb (GtkSignalListItemFactory *factory,
                           GtkListItem *item, PinsKeyRow *self)
{
    GtkBuilder *builder = gtk_builder_new_from_resource (
        "/io/github/fabrialberio/pinapp/pins-key-row-locale-menu-item.ui");
    GtkBox *row
        = GTK_BOX (gtk_builder_get_object (builder, "locale_menu_item"));

    gtk_list_item_set_child (item, GTK_WIDGET (row));
}

void
locale_menu_item_update_icon (PinsKeyRow *self, GtkListItem *item)
{
    GtkWidget *icon
        = gtk_widget_get_last_child (gtk_list_item_get_child (item));
    const gchar *locale, *current_locale;
    g_auto (PinsSplitKey) split = _pins_split_key_locale (self->key);

    locale = gtk_string_object_get_string (gtk_list_item_get_item (item));
    current_locale = split.locale;

    if (g_strcmp0 (locale, UNLOCALIZED_STRING) == 0)
        locale = NULL;

    gtk_widget_set_opacity (icon, g_strcmp0 (locale, current_locale) == 0);
}

void
locale_menu_item_bind_cb (GtkSignalListItemFactory *factory, GtkListItem *item,
                          PinsKeyRow *self)
{
    GtkWidget *row = gtk_list_item_get_child (item);
    GtkLabel *label = GTK_LABEL (gtk_widget_get_first_child (row));
    gchar *locale = (gchar *)gtk_string_object_get_string (
        gtk_list_item_get_item (item));

    g_assert (PINS_IS_KEY_ROW (self));

    g_signal_connect (self, "locale-changed",
                      G_CALLBACK (locale_menu_item_update_icon), item);
    locale_menu_item_update_icon (self, item);

    gtk_label_set_label (label, locale);
}

void
locale_menu_item_unbind_cb (GtkSignalListItemFactory *factory,
                            GtkListItem *item, PinsKeyRow *self)
{
    g_signal_handlers_disconnect_by_data (self, item);
}

void
locale_menu_item_activated_cb (GtkListView *list_view, guint position,
                               PinsKeyRow *self)
{
    if (position == 0)
        pins_key_row_set_locale (self, NULL);
    else
        pins_key_row_set_locale (
            self, (gchar *)gtk_string_list_get_string (
                      GTK_STRING_LIST (gtk_single_selection_get_model (
                          self->locales_model)),
                      position));

    gtk_popover_popdown (self->locale_popover);
}

static void
pins_key_row_init (PinsKeyRow *self)
{
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();

    self->locales_model
        = gtk_single_selection_new (G_LIST_MODEL (gtk_string_list_new (NULL)));

    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect_object (GTK_EDITABLE (self), "changed",
                             G_CALLBACK (pins_key_row_text_changed_cb), self,
                             0);

    g_signal_connect_object (self->reset_button, "clicked",
                             G_CALLBACK (pins_key_row_reset_key_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->remove_button, "clicked",
                             G_CALLBACK (pins_key_row_reset_key_cb), self,
                             G_CONNECT_SWAPPED);

    g_signal_connect_object (factory, "setup",
                             G_CALLBACK (locale_menu_item_setup_cb), self, 0);
    g_signal_connect_object (factory, "bind",
                             G_CALLBACK (locale_menu_item_bind_cb), self, 0);
    g_signal_connect_object (factory, "unbind",
                             G_CALLBACK (locale_menu_item_unbind_cb), self, 0);

    gtk_list_view_set_factory (self->locale_list_view, factory);
    gtk_list_view_set_model (self->locale_list_view,
                             GTK_SELECTION_MODEL (self->locales_model));

    g_signal_connect_object (self->locale_list_view, "activate",
                             G_CALLBACK (locale_menu_item_activated_cb), self,
                             0);
}
