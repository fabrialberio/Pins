from os import access, W_OK
from shutil import copy
from pathlib import Path
from typing import Callable, Optional
from dataclasses import dataclass

from gi.repository import Gtk, Adw, Gio, GObject

from .desktop_file import DesktopFile, DesktopEntry
from .file_pool import USER_POOL
from .key_file import FieldType, Localized, Locale, Field
from .config import APP_DATA


type KeyRow = Adw.EntryRow | Adw.SwitchRow


def row_for_key[T: FieldType](
        desktop_file: DesktopFile,
        field: Field[T]
    ) -> KeyRow:
    if field.type_ == bool:
        row = Adw.SwitchRow(
            title=field.key,
            active=desktop_file.get(field, default=False),
        )
        
        def on_notify_active(widget: Adw.SwitchRow, param: GObject.ParamSpecBoolean):
            value = widget.get_active()
            desktop_file.set(field, value)

        row.connect('notify::active', on_notify_active)
    elif field.type_ in [Localized[str], Localized[list[str]]]:
        row = Adw.EntryRow(
            title=Localized.split_key_locale(field.key)[0],
            text=desktop_file.get(field, default=Localized({})).unlocalized_or(''),
        )

        def on_changed(widget: Adw.EntryRow):
            value = widget.get_text()
            desktop_file.set(field, Localized({None: value}))

        row.connect('changed', on_changed)
    else:
        row = Adw.EntryRow(
            title=field.key,
            text=desktop_file.get(field, default=''),
        )

        def on_changed(widget: Adw.EntryRow):
            value = widget.get_text()
            desktop_file.set(field, value)

        row.connect('changed', on_changed)

    return row

def row_for_localized_key[T: Localized](
        desktop_file: DesktopFile,
        field: Field[T],
        locale: Locale,
    ) -> KeyRow:
    if field.type_ == Localized[list[str]]:
        inner_type = list[str]
    else:
        inner_type = str

    return row_for_key(
        desktop_file,
        Field(field.group_name, Localized.join_key_locale(field.key, locale), inner_type),
    )

class LocaleChooserRow(Adw.ComboRow):
    __gtype_name__ = 'LocaleChooserRow'

    def __init__(self, locale_list: list[Locale]) -> None:
        self.locales = locale_list

        model = Gtk.StringList()
        for l in self.locales:
            model.append(l)
        
        super().__init__(
            title = _('Locale'),
            model = model)

        self.add_prefix(Gtk.Image(icon_name='preferences-desktop-locale-symbolic'))

    def connect_rows(self, rows: list[KeyRow]):
        def update_rows(*args):
            for row in rows:
                key = row.get_title()
                unlocalized_key, locale = Localized.split_key_locale(key)
                row.set_visible(locale == self.get_selected_locale())

        self.connect('notify', update_rows)

    def set_default_locale(self):
        self.set_locale(Gtk.get_default_language().to_string())

    def get_selected_locale(self) -> Locale:
        return self.get_selected_item().get_string()

    def set_locale(self, locale: Locale):
        if locale in self.locales:
            self.set_selected(self.locales.index(locale))

@Gtk.Template(resource_path='/io/github/fabrialberio/pinapp/file_page.ui')
class FilePage(Adw.BreakpointBin):
    __gtype_name__ = 'FilePage'

    compact_breakpoint = Gtk.Template.Child('compact_breakpoint')

    window_title = Gtk.Template.Child('window_title')
    header_bar = Gtk.Template.Child('header_bar')
    back_button = Gtk.Template.Child('back_button')
    pin_button = Gtk.Template.Child('pin_button')

    file_menu_button = Gtk.Template.Child('file_menu_button')
    unpin_button = Gtk.Template.Child('unpin_button')
    rename_button = Gtk.Template.Child('rename_button')
    duplicate_button = Gtk.Template.Child('duplicate_button')

    scrolled_window = Gtk.Template.Child('scrolled_window')

    view_stack = Gtk.Template.Child('view_stack')
    file_view = Gtk.Template.Child('file_view')
    error_view = Gtk.Template.Child('error_view')

    app_icon = Gtk.Template.Child('icon')
    banner_listbox = Gtk.Template.Child('banner_listbox')

    localized_group = Gtk.Template.Child('localized_group')
    strings_group = Gtk.Template.Child('strings_group')
    bools_group = Gtk.Template.Child('bools_group')

    banner_expanded = True

    @property
    def allow_leave(self) -> bool:
        if self.file is not None:
            return not self.file.edited()
        else:
            return True


    def __init__(self):
        super().__init__()

        self.file: DesktopFile = None

        self.back_button.connect('clicked', lambda _: self.on_leave())
        self.pin_button.connect('clicked', lambda _: self.pin_file())
        self.unpin_button.connect('clicked', lambda _: self.unpin_file())
        self.rename_button.connect('clicked', lambda _: self.rename_file())
        self.duplicate_button.connect('clicked', lambda _: self.duplicate_file())
        self.localized_group.get_header_suffix().connect('clicked', lambda _: self._add_key(is_localized=True))
        self.strings_group.get_header_suffix().connect('clicked', lambda _: self._add_key())
        self.bools_group.get_header_suffix().connect('clicked', lambda _: self._add_key(is_bool=True))

        def _set_banner_expanded(value: bool):
            self.banner_expanded = value
            self._update_app_banner()

        self.compact_breakpoint.connect('apply', lambda _: _set_banner_expanded(False))
        self.compact_breakpoint.connect('unapply', lambda _: _set_banner_expanded(True))
        self.scrolled_window.get_vadjustment().connect('value-changed', lambda _: self._update_window_title())

    def pin_file(self):
        '''Saves a file to the user folder. Used when the file does not exist or it does not have write access.'''
        if self.file.path.exists():
            self.emit('file-changed')
            self.load_file(self.file.as_user_pool())
        else:
            self.emit('file-leave')

    def on_leave(self, callback: 'Optional[Callable[[FilePage], None]]' = None):
        '''Called when the page is about to be closed, e.g. when `Escape` is pressed or when the app is closed'''
        if self.allow_leave or self.file is None:
            self.emit('file-leave')
            if callback is not None:
                callback(self)
        else:
            if self.file.user_pool() and self.file.path.exists():
                self.file.save()
                self.emit('file-changed')
                self.emit('file-leave')

                if callback is not None:
                    callback(self)
            else:
                builder = Gtk.Builder.new_from_resource('/io/github/fabrialberio/pinapp/file_page_dialogs.ui')

                dialog = builder.get_object('save_changes_dialog')

                def on_resp(widget, resp):
                    if resp == 'discard':
                        self.emit('file-leave')
                        if callback is not None:
                            callback(self)
                    elif resp == 'pin':
                        self.pin_file()
                        if callback is not None:
                            callback(self)

                dialog.connect('response', on_resp)
                dialog.set_transient_for(self.get_root())
                dialog.present()

    def unpin_file(self):
        '''Deletes a file. It is used when the file has write access.'''
        if not self.file.user_pool():
            raise Exception('File is not in user pool')

        builder = Gtk.Builder.new_from_resource('/io/github/fabrialberio/pinapp/file_page_dialogs.ui')
        
        dialog = builder.get_object('confirm_delete_dialog')

        def callback(widget, resp):
            if resp == 'delete':
                USER_POOL.remove_all(self.file.path.name)
                self.emit('file-leave')
                self.emit('file-changed')

        dialog.connect('response', callback)
        dialog.set_transient_for(self.get_root())
        dialog.present()

    def rename_file(self):
        builder = Gtk.Builder.new_from_resource('/io/github/fabrialberio/pinapp/file_page_dialogs.ui')
        dialog = builder.get_object('rename_dialog')
        name_entry = builder.get_object('name_entry')
        name_entry.set_text(self.file.path.stem)

        def get_path():
            return USER_POOL.default_dir / Path(f'{Path(name_entry.get_text())}{DesktopFile.SUFFIX}')

        def path_is_valid() -> bool:
            path = name_entry.get_text()

            return '/' not in path and not get_path().exists()

        name_entry.connect('changed', lambda _: dialog.set_response_enabled(
            'rename',
            path_is_valid()
        ))

        def on_resp(widget, resp):
            if resp == 'rename':
                new_path = get_path()

                if self.file.path.exists():
                    self.file.path.rename(new_path)
                    self.load_path(new_path)
                else:
                    self.file.path = new_path

                self.emit('file-changed')

        dialog.connect('response', on_resp)
        dialog.set_transient_for(self.get_root())
        dialog.show()

    def duplicate_file(self):
        assert self.file is not None

        new_path = USER_POOL.new_file_path(self.file.path.stem, DesktopFile.SUFFIX)

        copy(self.file.path, new_path)

        self.load_path(new_path)
        self.emit('file-changed')

    def load_path(self, path: Path):
        try:
            file = DesktopFile(path)
        except (FileExistsError, PermissionError):
            self.view_stack.set_visible_child(self.error_view)
        else:
            self.load_file(file)

    def load_file(self, file: DesktopFile):
        self.view_stack.set_visible_child(self.file_view)
        self.file = file

        self.scrolled_window.get_vadjustment().set_value(0)

        self.file_menu_button.set_visible(self.file.user_pool())
        self.pin_button.set_visible(not self.file.user_pool())

        self.duplicate_button.set_sensitive(self.file.path.exists())
        self.unpin_button.set_sensitive(self.file.path.exists())

        self.update_page()

    def update_page(self):
        if self.file is None:
            raise ValueError
        
        localized_rows: list[KeyRow] = []
        string_rows: list[KeyRow] = []
        bool_rows: list[KeyRow] = []

        all_locales = set()

        tree = self.file.tree()

        # Type-hinted values
        for key, field in DesktopEntry.items():
            if key not in tree[DesktopEntry.GROUP_NAME].keys():
                continue

            row = row_for_key(self.file, field)

            if field.type_ in [Localized[str], Localized[list[str]]]:
                localized: Localized = self.file.get(field, Localized({}))

                all_locales |= set(localized.locales)

                for l in localized.locales:
                    localized_rows.append(row_for_localized_key(
                        self.file,
                        field,
                        l,
                    ))

            elif key in ['Name', 'Comment']:
                continue
            elif key == 'Icon':
                # TODO: Open file action
                #self.icon_row.add_action('folder-open-symbolic', lambda _: self._upload_icon())
                #self.icon_row.connect('changed', lambda _: self._update_icon())

                string_rows.append(row)
            elif field.type_ in [str, list]:
                string_rows.append(row)
            elif field.type_ == bool:
                bool_rows.append(row)


        if all_locales:
            self.locale_chooser_row = LocaleChooserRow(sorted(list(all_locales)))
            self.locale_chooser_row.connect_rows(localized_rows)
            self.locale_chooser_row.set_default_locale()

            self._update_pref_group(self.localized_group, [self.locale_chooser_row] + localized_rows)
            self.localized_group.set_visible(True)
        else:
            self.localized_group.set_visible(False)

        self._update_pref_group(
            pref_group=self.strings_group,
            new_children=string_rows, 
            empty_message=_('No string values present'))

        self._update_pref_group(
            pref_group=self.bools_group, 
            new_children=bool_rows, 
            empty_message=_('No boolean values present'))

        self._update_window_title()
        self._update_app_banner()

    def _update_window_title(self):
        if self.scrolled_window.get_vadjustment().get_value() > 0:
            self.header_bar.set_show_title(True)
            self.window_title.set_title(self.file.get(DesktopEntry.NAME, Localized('')).unlocalized_or(''))
        else:
            self.header_bar.set_show_title(False)

    def _update_icon(self):
        self.app_icon.set_pixel_size(128)
        self.app_icon.set_app_icon_name(self.file.get(DesktopEntry.ICON))

    def _upload_icon(self):
        def callback(dialog, response):
            if response == Gtk.ResponseType.ACCEPT:
                path = Path(dialog.get_file().get_path())

                # Copy file inside app data directory, so it persists after reboot
                new_path = APP_DATA / 'icons' / path.name
                copy(path, new_path)

                self.icon_row.field.set(str(new_path))
                self._update_icon()
                self.update_page()

        dialog = Gtk.FileChooserNative(
            title=_('Upload icon'),
            action=Gtk.FileChooserAction.OPEN,
            accept_label=_('Open'),
            cancel_label=_('Cancel'))

        if (path := Path(self.file.get(DesktopEntry.ICON, ''))).exists():
            dialog.set_current_folder(Gio.File.new_for_path(str(path.parent)))

        dialog.connect('response', callback)
        dialog.set_modal(True)
        dialog.set_transient_for(self.get_root())
        dialog.show()

    def _update_app_banner(self):
        if self.file is None:
            return

        while (row := self.banner_listbox.get_row_at_index(0)) != None:
            self.banner_listbox.remove(row)

        name_row = row_for_key(self.file, DesktopEntry.NAME)
        name_row.set_margin_bottom(6)
        name_row.add_css_class('app-banner-entry')

        name_row.connect('changed', lambda _: self._update_window_title())

        if self.banner_expanded:
            name_row.set_size_request(0, 64)
            name_row.add_css_class('title-1-row')
        else:
            name_row.add_css_class('title-2-row')

        comment_row = row_for_key(self.file, DesktopEntry.COMMENT)
        comment_row.add_css_class('app-banner-entry')

        self.banner_listbox.append(name_row)
        self.banner_listbox.append(comment_row)
        self._update_icon()

    def _add_key(self, is_bool=False, is_localized=False):
        builder = Gtk.Builder.new_from_resource('/io/github/fabrialberio/pinapp/file_page_dialogs.ui')

        add_key_dialog = builder.get_object('add_key_dialog')
        locale_entry = builder.get_object('locale_entry')
        key_entry = builder.get_object('key_entry')

        if is_localized:
            # TODO: Can't add localized keys
            locale = self.locale_chooser_row.get_selected_locale()
            locale_entry.set_visible(True)
            locale_entry.set_text(locale)

        key_entry.connect('changed', lambda _: add_key_dialog.set_response_enabled(
            'add', 
            bool(key_entry.get_text())))

        def callback(widget, resp):
            if resp == 'add':
                key = key_entry.get_text()
                value = False if is_bool else ''

                if is_localized: 
                    return
                    field = LocalizedField(key, self.file.desktop_entry._section, type(value))
                    field.set_localized(locale_entry.get_text(), value)
                else:
                    return
                    field = Field(key, self.file.desktop_entry._section, type(value))
                    field.set(value)

                self.update_page()

        add_key_dialog.connect('response', callback)
        add_key_dialog.set_transient_for(self.get_root())
        add_key_dialog.present()

    def _update_pref_group(self, pref_group: Adw.PreferencesGroup, new_children: list[Gtk.Widget], empty_message: 'str | None'=None):
        '''Removes all present children of the group and adds the new ones'''

        listbox = (
            pref_group
            .get_first_child()  # Main group GtkBox
            .get_last_child()   # GtkBox containing the listbox
            .get_first_child()) # GtkListbox

        if listbox != None:
            while (row := listbox.get_first_child()) != None:
                pref_group.remove(row)

        if len(new_children) > 0:
            for c in new_children:
                pref_group.add(c)
        elif empty_message != None:
            pref_group.add(Adw.ActionRow(
                title=empty_message,
                title_lines=1,
                css_classes=['dim-label'],
                halign=Gtk.Align.CENTER,
            ))