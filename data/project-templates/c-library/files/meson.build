project('${project_slug}', 'c',
  version: '0.1.0',
  license: '${license}',
  default_options: ['warning_level=2'])

lib = library('${project_slug}',
  'src/${project_ident}.c',
  include_directories: include_directories('include'))

test_exe = executable('test_${project_ident}',
  'tests/test_${project_ident}.c',
  link_with: lib,
  include_directories: include_directories('include'))

test('starter', test_exe)
