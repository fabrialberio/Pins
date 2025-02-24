/* pins-application.c
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

#include "pins-application.h"
#include "pins-desktop-file.h"
#include "pins-window.h"

struct _PinsApplication
{
    AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE (PinsApplication, pins_application, ADW_TYPE_APPLICATION)

PinsApplication *
pins_application_new (const char *application_id, GApplicationFlags flags)
{
    g_return_val_if_fail (application_id != NULL, NULL);

    return g_object_new (PINS_TYPE_APPLICATION, "application-id",
                         application_id, "flags", flags, NULL);
}

static void
pins_application_activate (GApplication *app)
{
    GtkWindow *window
        = gtk_application_get_active_window (GTK_APPLICATION (app));

    g_assert (PINS_IS_APPLICATION (app));

    if (window == NULL)
        window = g_object_new (PINS_TYPE_WINDOW, "application", app, NULL);

    gtk_window_present (GTK_WINDOW (window));
}

static void
pins_application_open (GApplication *app, GFile **files, gint n_files,
                       const gchar *hint)
{
    PinsWindow *window;
    PinsDesktopFile *desktop_file;
    GError *err = NULL;

    g_return_if_fail (g_file_query_exists (files[0], NULL));

    g_application_activate (app);

    window = PINS_WINDOW (
        gtk_application_get_active_window (GTK_APPLICATION (app)));

    desktop_file = pins_desktop_file_new_full (files[0], NULL, &err);
    if (err != NULL)
        {
            g_critical ("Error opening file at `%s`: %s",
                        g_file_get_path (files[0]), err->message);
            return;
        }

    pins_window_load_file (window, desktop_file);
}

static void
pins_application_class_init (PinsApplicationClass *klass)
{
    GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

    app_class->activate = pins_application_activate;
}

static void
pins_application_about_action (GSimpleAction *action, GVariant *parameter,
                               gpointer user_data)
{
    static const char *developers[] = { "Fabrizio", NULL };
    // This would be better handled as _("translator-credits")
    static const char *translators
        = "Irénée Thirion (French) <irenee.thirion@e.email>\n "
          "Sabri Ünal (Turkish) <libreajans@gmail.com>\n "
          "Fyodor Sobolev (Russian)\n "
          "David Lapshin (Russian)\n "
          "Alexmelman88 (Russian)\n "
          "josushu0 (Spanish)\n "
          "oscfdezdz (Spanish)\n "
          "gregorni (German)\n "
          "Mejans (Occitan)\n "
          "Vistaus (Dutch)\n"
          "johnpeteras19 (Portoguese) <johnppetersa@gmail.com>\n"
          "Fabrizio Alberio (Italian)";

    PinsApplication *self = user_data;
    GtkWindow *window = NULL;

    g_assert (PINS_IS_APPLICATION (self));

    window = gtk_application_get_active_window (GTK_APPLICATION (self));

    // clang-format off
    adw_show_about_dialog (GTK_WIDGET (window),
        "application-name", "Pins",
        "application-icon", g_application_get_application_id (G_APPLICATION (self)),
        "developer-name", "Fabrizio Alberio",
        "version", "2.0.1",
        "developers", developers,
        "copyright", "Copyright © 2024 Fabrizio",
        "license-type", GTK_LICENSE_GPL_3_0,
        "website", "https://github.com/fabrialberio/pinapp",
        "issue-url", "https://github.com/fabrialberio/pinapp/issues",
        "translator-credits", translators,
        NULL);
    // clang-format on
}

static void
pins_application_quit_action (GSimpleAction *action, GVariant *parameter,
                              gpointer user_data)
{
    PinsApplication *self = user_data;

    g_assert (PINS_IS_APPLICATION (self));

    g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
    { "quit", pins_application_quit_action },
    { "about", pins_application_about_action },
};

static void
pins_application_init (PinsApplication *self)
{
    g_action_map_add_action_entries (G_ACTION_MAP (self), app_actions,
                                     G_N_ELEMENTS (app_actions), self);
    gtk_application_set_accels_for_action (
        GTK_APPLICATION (self), "app.quit",
        (const char *[]){ "<primary>q", NULL });
    gtk_application_set_accels_for_action (
        GTK_APPLICATION (self), "win.new-app",
        (const char *[]){ "<primary>n", NULL });

    g_signal_connect_object (G_APPLICATION (self), "open",
                             G_CALLBACK (pins_application_open), self, 0);
}
