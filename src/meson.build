pinapp_sources = [
    'main.c',
    'pins-add-key-dialog.c',
    'pins-application.c',
    'pins-directories.c',
    'pins-file-view.c',
    'pins-home-view.c',
    'pins-key-row.c',
    'pins-locale-utils.c',
    'pins-pick-icon-popover.c',
    'pins-shortcut-filter.c',
    'pins-shortcut-icon.c',
    'pins-shortcut-iterator.c',
    'pins-shortcut-tile.c',
    'pins-shortcut.c',
    'pins-shortcuts-grid.c',
    'pins-window.c',
]

pinapp_deps = [
    dependency('gtk4'),
    dependency('libadwaita-1', version: '>= 1.4'),
]

pinapp_sources += gnome.compile_resources('pins-resources', 'pins.gresource.xml', c_name: 'pins')

executable(
    'pinapp',
    pinapp_sources,
    dependencies: pinapp_deps,
    install: true,
)
