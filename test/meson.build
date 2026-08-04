test_sources = [
  'arena_tests.cc'
]

test_dependencies = [
    gtest_dep,
    gtest_main_dep,
]

nrtest_exe = executable(
  'nrtest',
  test_sources,
  include_directories: includes,
  dependencies: test_dependencies,
)

test('basic tests', nrtest_exe)
