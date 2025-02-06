sizes = [16, 22, 24]
names = [
  'xfce4-battery-critical',
  'xfce4-battery-critical-charging',
  'xfce4-battery-full',
  'xfce4-battery-full-charging',
  'xfce4-battery-low',
  'xfce4-battery-low-charging',
  'xfce4-battery-missing',
  'xfce4-battery-ok',
  'xfce4-battery-ok-charging',
  'xfce4-battery-plugin',
]

foreach size : sizes
  foreach name : names
    install_data(
      '@0@x@0@'.format(size) / '@0@.png'.format(name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'apps',
    )
  endforeach
endforeach

foreach name : names
  install_data(
    'scalable' / '@0@.svg'.format(name),
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
  )
endforeach

install_data(
  '32x32' / 'xfce4-battery-plugin.png',
  install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '32x32' / 'apps',
)
