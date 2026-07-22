project('${project_slug}', 'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++20', 'warning_level=2'])

inc = include_directories('include')

executable('${executable_name}',
  'src/main.cpp',
  include_directories: inc,
  install: true)

test_exe = executable('test_${executable_name}',
  'tests/test_main.cpp',
  include_directories: inc)

test('starter', test_exe)
