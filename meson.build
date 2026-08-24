project('nrfrac', ['c', 'cpp'], version: '0.1.0', default_options: ['cpp_std=gnu++23', 'c_std=gnu23'])

gtest_proj = subproject('gtest')
gtest_dep = gtest_proj.get_variable('gtest_dep')
gtest_main_dep = gtest_proj.get_variable('gtest_main_dep')

dependencies = []

includes = include_directories('include', 'subprojects/cimg')

if host_machine.cpu_family() == 'riscv64'
    libvecm_proj = subproject('veclibm')
    libvecm_dep = libvecm_proj.get_variable('libvecm_dep')
    dependencies += [libvecm_dep]
endif

subdir('src')
subdir('test')
