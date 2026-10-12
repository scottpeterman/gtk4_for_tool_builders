project('lab08-tabs', 'cpp',
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

executable('lab', 'src/main.cpp', dependencies: gtk)
