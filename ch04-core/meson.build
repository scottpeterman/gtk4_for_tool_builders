project('gtklab', 'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++17', 'warning_level=2'])

gtk = dependency('gtk4', version: '>= 4.8')
glib = dependency('glib-2.0', version: '>= 2.74')

gtk_floor_args = [
  '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_8',
  '-DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_8',
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_74',
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_74',
  '-Werror=deprecated-declarations',
]
add_project_arguments(gtk_floor_args, language: 'cpp')

inc = include_directories('src')

# The core: no GTK. Everything here is testable without a display.
core = static_library('gtklab_core',
  'src/core/session.cpp',
  'src/core/settings.cpp',
  include_directories: inc,
  dependencies: glib)
core_dep = declare_dependency(link_with: core,
  include_directories: inc,
  dependencies: glib)

executable('gtklab', 'src/main.cpp', dependencies: [gtk, core_dep])

# doctest is one header, vendored in tests/ (MIT licence).
tests = executable('gtklab-tests',
  'tests/test_main.cpp',
  'tests/test_session.cpp',
  'tests/test_settings.cpp',
  dependencies: core_dep)
test('core', tests)
