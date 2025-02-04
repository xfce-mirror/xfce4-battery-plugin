plugin_sources = [
  'battery.c',
  'libacpi.h',
  'libacpi.c',
]

extra_link_args = []

if host_machine.system() == 'darwin'
  plugin_sources += [
    'libsmc.c',
    'libsmc.h',
  ]
  extra_link_args += [
    '-Wl,-dead_strip_dylibs',
    '-Wl,-framework,CoreFoundation',
    '-Wl,-framework,IOKit',
  ]
endif
if host_machine.system() == 'netbsd'
  extra_link_args += [
    '-lprop',
    '-Wl,-rpath=/usr/X11R7/lib',
  ]
endif

plugin_sources += xfce_revision_h
plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

plugin_lib = shared_library(
  'battery',
  plugin_sources,
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-battery-plugin'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gio,
    gtk,
    libxfce4util,
    libxfce4ui,
    libxfce4panel,
  ],
  link_args: extra_link_args,
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

i18n.merge_file(
  input: 'battery.desktop.in',
  output: 'battery.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)
