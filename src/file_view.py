from gi.repository import Gtk, Gio, Adw, GObject

from .desktop_entry import DesktopFile

@Gtk.Template(resource_path='/com/github/fabrialberio/pinapp/file_view.ui')
class FileView(Gtk.Box):
    __gtype_name__ = 'FileView'
    
    back_button = Gtk.Template.Child('back_button')
    save_button = Gtk.Template.Child('save_button')
    main_view = Gtk.Template.Child('main_box')

    app_icon = Gtk.Template.Child('app_icon')
    app_name_entry = Gtk.Template.Child('app_name')
    app_comment_entry = Gtk.Template.Child('app_comment')

    strings_group = Gtk.Template.Child('strings_group')
    bools_group = Gtk.Template.Child('bools_group')

    def __init__(self):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)

        self.file = None

        GObject.type_register(FileView)
        GObject.signal_new('file-back', FileView, GObject.SIGNAL_RUN_FIRST, GObject.TYPE_NONE, ())
        GObject.signal_new('file-save', FileView, GObject.SIGNAL_RUN_FIRST, GObject.TYPE_NONE, ())
        GObject.signal_new('file-edit', FileView, GObject.SIGNAL_RUN_FIRST, GObject.TYPE_NONE, ())
        GObject.signal_new('add-string-field', FileView, GObject.SIGNAL_RUN_FIRST, GObject.TYPE_NONE, ())
        GObject.signal_new('add-bool-field', FileView, GObject.SIGNAL_RUN_FIRST, GObject.TYPE_NONE, ())

        self.back_button.connect('clicked', lambda _: self.emit('file-back'))
        self.save_button.connect('clicked', lambda _: self.emit('file-save'))
        self.strings_group.get_header_suffix().connect('clicked', lambda _: self.emit('add-string-field'))
        self.bools_group.get_header_suffix().connect('clicked', lambda _: self.emit('add-bool-field'))

        self.build_ui()

    def load_file(self, file: DesktopFile):
        self.file = file

        self.app_icon.set_from_icon_name(self.file.appsection.Icon.get())
        self.app_name_entry.set_text(self.file.appsection.Name.get() or '')
        self.app_comment_entry.set_text(self.file.appsection.Comment.get() or '')

        file_items = self.file.appsection.items()

        string_rows = [StringRow(title=k, default_value=v.get()) for k, v in file_items if type(v.get()) == str]
        self._update_preferences_group(self.strings_group, string_rows)

        bool_rows = [BoolRow(title=k, default_state=v.get()) for k, v in file_items if type(v.get()) == bool]
        self._update_preferences_group(self.bools_group, bool_rows)

        self.save_button.set_sensitive(True)


    def build_ui(self):
        ...


    @classmethod
    def _update_preferences_group(self, preferences_group: Adw.PreferencesGroup, new_children: list[Gtk.Widget], max_children = 10000):
        '''Removes all present children of the group and adds the new ones'''

        listbox = (
            preferences_group
            .get_first_child()  # Main group GtkBox
            .get_last_child()   # GtkBox containing the listbox
            .get_first_child()) # GtkListbox

        old_children: list[Gtk.Widget] = []

        i = 0
        while listbox.get_row_at_index(i) is not None:
            old_children.append(listbox.get_row_at_index(i))
            if i > max_children:
                raise IndexError('Listbox has too many children, maybe something is wrong')
            i += 1

        for c in old_children:
            preferences_group.remove(c)

        for c in new_children:
            preferences_group.add(c)

class StringRow(Adw.EntryRow):
    # TODO: Replace this with Adw.EntryRow when possible
    def __init__(
            self, 
            title: str,
            default_value: str,
            monospace: bool = False,) -> None:

        super().__init__(
            title=title.capitalize(),
            css_classes = ['monospace'] if monospace else None,)

        self.set_text(default_value)

class BoolRow(Adw.ActionRow):
    def __init__(
            self,
            title: str,
            default_state: bool = False
        ) -> None:

        self.switch = Gtk.Switch(
            active=default_state,
            valign=Gtk.Align.CENTER,
        )

        super().__init__(
            title=title,
            activatable_widget=self.switch,
        )
        super().add_suffix(self.switch)