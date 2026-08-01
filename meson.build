project('nrfrac', ['c', 'cpp'], version: '0.1.0', default_options: ['cpp_std=gnu++23', 'c_std=gnu23'])

dependencies = []

if host_machine.cpu_family() == 'riscv64'
    gtest_proj = subproject('gtest')
    libvecm_proj = subproject('veclibm')
    libvecm_dep = libvecm_proj.get_variable('libvecm_dep')
    dependencies += [libvecm_dep]
endif

subdir('src')
