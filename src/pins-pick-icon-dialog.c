/* pins-pick-icon-dialog.c
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

#include "pins-pick-icon-dialog.h"
#include "pins-desktop-file.h"

#include <glib/gi18n.h>

struct _PinsPickIconDialog
{
    AdwDialog parent_instance;

    PinsDesktopFile *desktop_file;
    GtkFilterListModel *model;
    GtkStringFilter *search_filter;

    GtkButton *load_from_file_button;
    GtkSearchEntry *search_entry;
    GtkGridView *grid_view;
};

G_DEFINE_TYPE (PinsPickIconDialog, pins_pick_icon_dialog, ADW_TYPE_DIALOG);

PinsPickIconDialog *
pins_pick_icon_dialog_new (PinsDesktopFile *desktop_file)
{
    PinsPickIconDialog *dialog
        = g_object_new (PINS_TYPE_PICK_ICON_DIALOG, NULL);

    dialog->desktop_file = g_object_ref (desktop_file);

    return dialog;
}

void
pins_pick_icon_dialog_dispose (GObject *object)
{
    PinsPickIconDialog *self = PINS_PICK_ICON_DIALOG (object);

    g_clear_object (&self->desktop_file);

    gtk_widget_dispose_template (GTK_WIDGET (object),
                                 PINS_TYPE_PICK_ICON_DIALOG);

    G_OBJECT_CLASS (pins_pick_icon_dialog_parent_class)->dispose (object);
}

static void
pins_pick_icon_dialog_class_init (PinsPickIconDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_pick_icon_dialog_dispose;

    gtk_widget_class_set_template_from_resource (
        widget_class,
        "/io/github/fabrialberio/pinapp/pins-pick-icon-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class, PinsPickIconDialog,
                                          load_from_file_button);
    gtk_widget_class_bind_template_child (widget_class, PinsPickIconDialog,
                                          search_entry);
    gtk_widget_class_bind_template_child (widget_class, PinsPickIconDialog,
                                          grid_view);
}

void
load_icon_dialog_closed_cb (GObject *dialog, GAsyncResult *res,
                            gpointer user_data)
{
    PinsPickIconDialog *self = PINS_PICK_ICON_DIALOG (user_data);
    g_autoptr (GFile) sandbox_file = NULL;
    g_autoptr (GFile) file = NULL;

    sandbox_file
        = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (dialog), res, NULL);

    if (sandbox_file == NULL)
        {
            adw_dialog_close (ADW_DIALOG (self));
            return;
        }

    file
        = g_file_new_build_filename (g_get_user_data_dir (), "user-icons",
                                     g_file_get_basename (sandbox_file), NULL);

    g_file_make_directory_with_parents (g_file_get_parent (file), NULL, NULL);
    g_file_copy (sandbox_file, file, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);

    pins_desktop_file_set_string (self->desktop_file,
                                  G_KEY_FILE_DESKTOP_KEY_ICON,
                                  g_file_get_path (file));

    adw_dialog_close (ADW_DIALOG (self));
}

void
load_from_file_button_clicked_cb (PinsPickIconDialog *self)
{
    GtkFileDialog *dialog = gtk_file_dialog_new ();

    gtk_file_dialog_set_title (dialog, _ ("Load Icon from File"));
    gtk_file_dialog_open (dialog,
                          GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                          NULL, load_icon_dialog_closed_cb, self);

    gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

void
search_changed_cb (PinsPickIconDialog *self, GtkSearchEntry *entry)
{
    gtk_string_filter_set_search (
        self->search_filter, gtk_editable_get_text (GTK_EDITABLE (entry)));
}

void
icon_activated_cb (PinsPickIconDialog *self, guint position)
{
    g_autoptr (GtkStringObject) string_object = GTK_STRING_OBJECT (
        g_list_model_get_item (G_LIST_MODEL (self->model), position));

    pins_desktop_file_set_string (
        self->desktop_file, G_KEY_FILE_DESKTOP_KEY_ICON,
        gtk_string_object_get_string (string_object));

    adw_dialog_close (ADW_DIALOG (self));
}

void
icon_setup_cb (GtkSignalListItemFactory *factory, GtkListItem *item)
{
    GtkWidget *icon = gtk_image_new ();
    gtk_image_set_pixel_size (GTK_IMAGE (icon), 32);
    gtk_widget_add_css_class (icon, "icon-dropshadow");

    gtk_list_item_set_child (item, icon);
}

void
icon_bind_cb (GtkSignalListItemFactory *factory, GtkListItem *item)
{
    GtkStringObject *string_object = gtk_list_item_get_item (item);
    GtkWidget *icon = gtk_list_item_get_child (item);
    const gchar *string = gtk_string_object_get_string (string_object);

    gtk_image_set_from_icon_name (GTK_IMAGE (icon), string);
    gtk_widget_set_tooltip_text (icon, string);
}

static void
pins_pick_icon_dialog_init (PinsPickIconDialog *self)
{
    GtkIconTheme *theme;
    GtkStringList *string_list = NULL;
    GtkSortListModel *sort_model = NULL;
    g_autoptr (GtkNoSelection) selection_model = NULL;
    g_autoptr (GtkListItemFactory) factory
        = gtk_signal_list_item_factory_new ();

    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect_object (self->load_from_file_button, "clicked",
                             G_CALLBACK (load_from_file_button_clicked_cb),
                             self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->search_entry, "search-changed",
                             G_CALLBACK (search_changed_cb), self,
                             G_CONNECT_SWAPPED);

    theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());

    string_list = gtk_string_list_new (
        (const gchar *const *)gtk_icon_theme_get_icon_names (theme));

    sort_model = gtk_sort_list_model_new (
        G_LIST_MODEL (string_list),
        GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (
            GTK_TYPE_STRING_OBJECT, NULL, "string"))));

    self->search_filter = gtk_string_filter_new (
        gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string"));

    self->model = gtk_filter_list_model_new (G_LIST_MODEL (sort_model),
                                             GTK_FILTER (self->search_filter));

    selection_model = gtk_no_selection_new (G_LIST_MODEL (self->model));

    g_signal_connect_object (self->grid_view, "activate",
                             G_CALLBACK (icon_activated_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (factory, "setup", G_CALLBACK (icon_setup_cb),
                             NULL, 0);
    g_signal_connect_object (factory, "bind", G_CALLBACK (icon_bind_cb), NULL,
                             0);

    gtk_grid_view_set_factory (self->grid_view, factory);
    gtk_grid_view_set_model (self->grid_view,
                             GTK_SELECTION_MODEL (selection_model));
}
