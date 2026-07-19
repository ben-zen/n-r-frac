project('nrfrac', ['c', 'cpp'], version: '0.1.0', default_options: ['cpp_std=gnu++23', 'c_std=gnu23'])

no_opt_sources = ['pre_crt.c']

no_opt_lib = static_library('no_opt', no_opt_sources, c_args: ['-O0'])

sources = ['gen.cc']

executable(
    'nrfrac',
    sources,
    link_with: [no_opt_lib]
)
