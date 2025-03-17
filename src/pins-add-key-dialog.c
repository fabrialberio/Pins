/* pins-add-key-dialog.c
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

#include "pins-add-key-dialog.h"

#include <glib/gi18n.h>

struct _PinsAddKeyDialog
{
    AdwAlertDialog parent_instance;

    PinsDesktopFile *desktop_file;
    AdwEntryRow *key_row;
};

G_DEFINE_TYPE (PinsAddKeyDialog, pins_add_key_dialog, ADW_TYPE_ALERT_DIALOG);

enum
{
    CANCEL,
    ADD,
    N_RESPONSES,
};

static gchar *responses[N_RESPONSES] = { "cancel", "add" };

PinsAddKeyDialog *
pins_add_key_dialog_new (PinsDesktopFile *desktop_file)
{
    PinsAddKeyDialog *dialog = g_object_new (PINS_TYPE_ADD_KEY_DIALOG, NULL);

    dialog->desktop_file = g_object_ref (desktop_file);

    return dialog;
}

void
pins_add_key_dialog_dispose (GObject *object)
{
    PinsAddKeyDialog *self = PINS_ADD_KEY_DIALOG (object);

    g_clear_object (&self->desktop_file);

    gtk_widget_dispose_template (GTK_WIDGET (object),
                                 PINS_TYPE_ADD_KEY_DIALOG);

    G_OBJECT_CLASS (pins_add_key_dialog_parent_class)->dispose (object);
}

static void
pins_add_key_dialog_class_init (PinsAddKeyDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_add_key_dialog_dispose;

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-add-key-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class, PinsAddKeyDialog,
                                          key_row);
}

void
response_cb (PinsAddKeyDialog *self, gchar *response)
{
    if (!g_strcmp0 (response, responses[ADD]))
        {
            const gchar *key
                = gtk_editable_get_text (GTK_EDITABLE (self->key_row));

            pins_desktop_file_set_string (self->desktop_file, key, "");
        }
}

void
key_entry_activated_cb (PinsAddKeyDialog *self, AdwEntryRow *key_row)
{
    if (adw_alert_dialog_get_response_enabled (ADW_ALERT_DIALOG (self),
                                               responses[ADD]))
        {
            g_signal_emit_by_name (self, "response", responses[ADD]);
            adw_dialog_close (ADW_DIALOG (self));
        }
}

void
update_response_enabled (PinsAddKeyDialog *self)
{
    gboolean text_is_valid = FALSE;
    const gchar *text = gtk_editable_get_text (GTK_EDITABLE (self->key_row));

    text_is_valid = strlen (text) > 0
                    && !g_strv_contains (
                        (const gchar *const *)pins_desktop_file_get_keys (
                            self->desktop_file),
                        text);

    adw_alert_dialog_set_response_enabled (ADW_ALERT_DIALOG (self),
                                           responses[ADD], text_is_valid);
}

static void
pins_add_key_dialog_init (PinsAddKeyDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect_object (self, "response", G_CALLBACK (response_cb), self,
                             0);
    g_signal_connect_object (self->key_row, "entry-activated",
                             G_CALLBACK (key_entry_activated_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->key_row, "changed",
                             G_CALLBACK (update_response_enabled), self,
                             G_CONNECT_SWAPPED);
}
