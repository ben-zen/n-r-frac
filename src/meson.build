

no_opt_sources = ['pre_crt.c']

no_opt_lib = static_library('no_opt', no_opt_sources, c_args: ['-O0', '-g'])

sources = ['gen.cc']

if host_machine.cpu_family() == 'x86_64'
    cpp_args = ['-mavx2']
elif host_machine.cpu_family() == 'riscv64'
    cpp_args = ['-mrvv-max-lmul=dynamic']
endif

cpp_args += ['-lm', '-g']

executable(
    'nrfrac-a100',
    sources,
    dependencies: dependencies,
    link_with: [no_opt_lib],
    cpp_args: cpp_args,
)

x100_args = ['-DDISABLE_A100']
executable(
    'nrfrac-x100',
    sources,
    dependencies: dependencies,
    c_args: x100_args,
    cpp_args: [cpp_args, x100_args]
)
