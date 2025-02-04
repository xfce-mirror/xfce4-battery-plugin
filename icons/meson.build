foreach size: ['16x16', '22x22', '24x24', '32x32', 'scalable']
  install_subdir(
    size,
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / size / 'apps',
    exclude_files: [
      'Makefile.am',
    ],
    strip_directory: true,
  )
endforeach
