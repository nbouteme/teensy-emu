teemu3_sources = [
  'main.cpp',
  'teemu3-window.cpp',
  'teensy_emu.cpp'
]

cc = meson.get_compiler('c')

teemu3_deps = [
  dependency('gtkmm-3.0', version: '>= 3.18'),
  cc.find_library('dl'),
  dependency('threads')
]

gnome = import('gnome')

teemu3_sources += gnome.compile_resources('teemu3-resources',
  'teemu3.gresource.xml',
  c_name: 'teemu3'
)

executable('teemu3', teemu3_sources,
  dependencies: teemu3_deps,
  link_args: ['-rdynamic'],
  install: true
)
