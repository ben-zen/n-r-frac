project('nrfrac', ['c', 'cpp'], version: '0.1.0', default_options: ['cpp_std=gnu++23', 'c_std=gnu23'])

no_opt_sources = ['pre_crt.c']

no_opt_lib = static_library('no_opt', no_opt_sources, c_args: ['-O0'])

sources = ['gen.cc']

if host_machine.cpu_family() == 'x86_64'
    cpp_args = ['-mavx2']
elif host_machine.cpu_family() == 'riscv64'
    cpp_args = ['-mrvv-max-lmul=dynamic']
endif

cpp_args += ['-lm']

executable(
    'nrfrac',
    sources,
    link_with: [no_opt_lib],
    cpp_args: cpp_args,
)
