/* pins-directories.h
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

#include "pins-directories.h"

gchar *
parse_filename (gchar *parse_name)
{
    // Necessary to expand "~/" prefix to actual user home dir
    return g_file_get_path (g_file_parse_name (parse_name));
}

gchar *
pins_desktop_file_user_path (void)
{
    g_autoptr (GSettings) settings
        = g_settings_new ("io.github.fabrialberio.pinapp");

    return parse_filename (g_settings_get_string (settings, "user-path"));
}

gchar *
pins_desktop_file_autostart_path (void)
{
    g_autoptr (GSettings) settings
        = g_settings_new ("io.github.fabrialberio.pinapp");

    return parse_filename (g_settings_get_string (settings, "autostart-path"));
}

gchar **
pins_desktop_file_search_paths (void)
{
    GStrvBuilder *builder = g_strv_builder_new ();
    g_autoptr (GSettings) settings
        = g_settings_new ("io.github.fabrialberio.pinapp");
    g_auto (GStrv) search_paths
        = g_settings_get_strv (settings, "search-paths");

    g_strv_builder_add (builder, pins_desktop_file_user_path ());

    for (int i = 0; i < g_strv_length (search_paths); i++)
        g_strv_builder_add (builder,
                            g_build_filename (parse_filename (search_paths[i]),
                                              "applications", NULL));

    return g_strv_builder_end (builder);
}

void
pins_inject_icon_search_paths (void)
{
    GStrvBuilder *builder = g_strv_builder_new ();
    g_autoptr (GSettings) settings
        = g_settings_new ("io.github.fabrialberio.pinapp");
    g_auto (GStrv) search_paths
        = g_settings_get_strv (settings, "search-paths");
    g_autoptr (GtkIconTheme) theme
        = gtk_icon_theme_get_for_display (gdk_display_get_default ());

    for (int i = 0; i < g_strv_length (search_paths); i++)
        g_strv_builder_add (builder,
                            g_build_filename (parse_filename (search_paths[i]),
                                              "icons", NULL));

    g_strv_builder_addv (
        builder, (const gchar **)gtk_icon_theme_get_search_path (theme));

    gtk_icon_theme_set_search_path (
        theme, (const gchar *const *)g_strv_builder_end (builder));
}
