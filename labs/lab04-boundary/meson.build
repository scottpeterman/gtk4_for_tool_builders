project('lab04-boundary', 'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++17', 'warning_level=2'])

gtk = dependency('gtk4', version: '>= 4.8')

gtk_floor_args = [
  '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_8',
  '-DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_8',
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_74',
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_74',
  '-Werror=deprecated-declarations',
]
add_project_arguments(gtk_floor_args, language: 'cpp')

# The core is header-only here. The test target is given no GTK dependency,
# so the boundary is checked by the compiler.
core_inc = include_directories('src')

executable('lab', 'src/main.cpp', include_directories: core_inc, dependencies: gtk)

test('counter model',
  executable('lab-tests', 'tests/test_counter.cpp', include_directories: core_inc))
