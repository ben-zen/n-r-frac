So why another simple Newton-Rapheson fractal generator?

I built one of these years and years ago, as an initial proof of concept to show that I
knew what I was doing, in one sense or another. Now, though, I wanted to see what I could
do with RVV instructions, and I had a feeling I could eventually get this to be a better
looping codebase.

Plus, maybe it'll get me to figure out adding features to Rust; I'm starting out in C++ because
I have fewer concerns about actually having a supported compiler with all the extensions I need,
but I expect I may re-do it again after the fact.

So, the math involved is pretty simple. It's Newtonian approximation, applied to the complex
plane. Start with each point valued as, well, itself, and then iterate on Newton's method to approach
a root:

``` z_(n+1) = f(z_n) / f'(z_n) ```

Due to quirks of how numbers in the complex plane work, this creates cool fractal imagery when the roots of a 
given polynomial produce intermeshed bubbles where the root a given point approaches when
approximated may differ from other, nearby points... even if that starting value is closer
to another root.

So, first off. The unoptimized version, running on a K3 AI core: kinda tragic. It pegs
a single core at 100% for .... well, a very long time. Doing math that this core is supposed
to be brilliant at. (It is, my code's just bad.)

## Non-parallelized code times
| Binary    | CPU core  |   `time` output
| --------- | --------- | ------------------------------------------------------
| a.unopt   | A100      | real    1m40.149s, user    1m39.823s, sys     0m0.232s
| a.opt     | A100      | real    0m37.280s, user    0m36.937s, sys     0m0.304s
| a.unopt   | X100      | real    0m50.415s, user    0m50.269s, sys     0m0.105s
| a.opt     | X100      | real    0m18.109s, user    0m18.000s, sys     0m0.101s

## Improvements?

Looking at [GCC documentation](https://gcc.gnu.org/onlinedocs/gcc-8.3.0/gcc/RISC-V-Options.html),
I want to set `-march=` and `-mabi=` appropriately, so I'm investigating the cores' features on
[spaceMIT's website](https://www.spacemit.com/community/document/info?lang=en&nodepath=hardware/key_stone/k3/k3_docs/k3_ds.md)

The A100 cores are RVA23 (less the hypervisor) plus "SpacemiT-IME" instructions?

> RVA23* Optional Extensions (not included in the RVA23 base specification):
>
>    Vector Crypto: `zvkng`, `zvksg`
>    Other Extensions: `zvbc`, `zfh`, `zbc`, `zvfh`, `zfbmin`, `zvfbfmin`, `zvfbfwma`
>    System and Security: `sdtri`, `svvptc`, `sspm`, `smepmp`, `smstateen`, `smcntrpmf`


`-march=rva23` seems like I should be able to get away with it, but spacemiT themselves use `-march=rv64gcv`
... so I'll try both!


`g++ -std=c++23 -O3 -march=rva23` ... didn't build. So it's `-march=rv64gcv` in the end!

Build times on an A100 core: real    0m14.333s, user    0m13.636s, sys     0m0.678s
Build times on an X100 core: real    0m7.121s,  user    0m6.772s,  sys     0m0.329s


To the above table, I'll add a new pair of entries:

| Binary  | CPU core | `time` output
| ------- | -------- | ------------------------------------------------------
| a.flags | A100     | real    0m37.762s, user    0m37.472sk sys     0m0.252s
| a.flags | X100     | real    0m18.846s, user    0m18.700s, sys     0m0.137s

I'm gonna have a poke around in the objdump contents of `./a.flags`.

In the interim, I tried exploring the path of a blogger who got one of these boards. I was
able to shove a binary onto an A100 core and get a vector intrinsic to spit out the register width,
so that's pretty cool. The real fun is going to come with figuring out how to make the best use of
those cores & those operations, because a single register is then wide enough for 16 complex numbers
at once. If the large multiplication problems are able to be loaded 16 at a time, or even just some of
these operations getting a boost...

## Thoughts on how to speed up the algorithm

I originally wrote this to use `std::transform`, but found that wasn't working. Now that I've
gotten the algorithm down, maybe it can be sped up through traditional means first.

## Aside on initializing

Part of what makes the K3 interesting is that the A100 cores have vector registers twice the size of AVX512's, and 4 times larger than the X100 general compute cores on the same chip. However, by default all processes run on the X100 cores, and the kernel will not schedule tasks on the A100 cores without special configuration--because for most tasks, they are inferior. See the data above for the difference in the unoptimized approach above for an example of that disparity! No, the real reason to move to those cores is to also vectorize the math--but first, the code's gotta run on these shiny new cores.

I'm taking a moment to flex by writing a shim to redirect the process to the A100 cores before libc's initialization has run; I want to run these commands as early as possible in process bring-up to make sure no code touches the vector registers before I can affinitize correctly, so we don't end up with any torn state or lost data. I'll readily acknowledge, there's pretty much no risk of libc doing anything untoward, but this was a lot of fun to write, and sometimes you just gotta flourish.

What I've got now is a file, `pre_crt.c`, which is built as a separate C static library, and links into the host application. It establishes a pre-init hook that migrates the process to the A100 cores by writing the new process' PID to `/proc/set_ai_thread`. This identifies the process as a vector-core process before anything might touch the vector registers, and all it took was writing my own syscalls to `getpid()`, `openat()`, `write()`, & `close()`.

## Returning to speeding up the algorithm

I saw this [article on modified data layouts for vectorized complex math](https://aiichironakano.github.io/cs653/Popovici-ComplexSIMD-HPEC17.pdf) the other day, and that inspired me to go back to what I was thinking about with how to handle computations. In the paper, the authors present an approach using split arrays for real and imaginary parts, joining them on presentation essentially. Looking at the math I need to do, there's three distinct operations at play:

```f(z) = c_0 + c_1 * z + c_2 * z^2 + ... c_n + z^n```

consists of a summation of a series of terms, each of which is composed of a constant complex number times a power of an input value. The summation is the relatively easy part (I'm going to take the stance that errors via significant figures are not a sufficient issue for this project for me to care for now, but that may need to be addressed in the future), it's the multiplication and all the exponentials that get messy.

How this seems it should work out is, I need a struct that provides two buffers, one for all real values, one for all imaginary values -- and another struct which provides two buffers: one for radii, and one for angles---hey wait a minute! (I'm going to start with just building a structure for standard coordinates and see if I can extend it from there. Starting from a "first principles" approach is going to waste a lot of time on false starts, and I think there are a couple of fun directions to go in.)

Before I even start thinking about vectorizing the entire data store, however, how about we speed up just the end of the polynomial function summation I'm doing right now? Let's make `polynomial_function::eval()` faster.

## Vectorize that sum

coefficients & powers as vectors being applied to single input data.

I started by just vectorizing the summation for each value. In this case, I'm pretty sure I'm not making sufficient use of the vector cores, because this is dependent on the number of terms in the polynomial; that's not going to fill those massive vectors, but doing stepwise operations on the entire plot is more likely to succeed.

| CPU core | `time` output
| -------- | ---------------------------------
| A100     | 17.24s user 0.12s system 99% cpu 17.373 total
| X100     | 6.37s user 0.06s system 99% cpu 6.444 total

This first attempt definitely got me somewhere. I think it's time to switch up the order of computation a bit, and make some data model changes.

## Going actually into DeMoivre's Theorem and all that

Up until now, my data's been stored in a big long `vector<complex<double>>` that just has pretend bounds. For some operations, it's fine. For others... well, multiplication's really bad. Addition's fine, for instance, because the terms add cleanly. Multiplication, however, brings in the spectre of term expansion, and now data being stored interleaved creates a whole new issue that doesn't vectorize well.

Enter Popovici's article above. If I store the real and imaginary terms as separate arrays, whole sets of computations become much easier. We're going to extend that here to also include polar coordinates, because that will make vectorized application of DeMoivre's Theorem possible, and save me many headaches wrt powers above 2.

> Converting to polar coordinates:
> for z = a + bi
> r = sqrt(a^2 + b^2)
> if b >= 0
> t = arccos(a / r)
> else
> t = 2pi - arccos(a / r)

> Converting from polar coordinates:
> z(r, t) = r * cos(t) + r * i * sin(t)

> DeMoivre's theorem:
> z ^ n = r ^ n cos(n * t) + r ^ n * i * sin(n * t)

I'll start with just implementing the cartesian format, though, for my sanity's sake.

I'm also going to just accept the memory hit of keeping a copy of the entire plot's space per term for both polar and cartesian; optimization for memory can come later, especially when considering allocators. These are also not so large that I need to care.

> Cartesian multiplication of complex numbers
> terms are x = a + bi, y = c + di
> x * y = ( a + bi ) * ( c + di )
> = ( a * c ) + ( bi * c ) + ( a * di ) + ( bi * di )
> = ( a * c ) + ( b * c )i + ( a * c )i - ( b * d )
> = ( a * c - b * d) + (a * d + b * c)i 

Conceptually, that'd look like this:
```
plot<double> multiply(plot<double> const &lhs, plot<double> const &rhs) {
    // Each plot is composed of a list of reals, and a list of imaginaries. Call these
    auto lhr = lhs.real;
    auto lhi = lhs.imaginary;
    auto rhr = rhs.real;
    auto rhi = rhs.imaginary;
    
    // we need to walk through all of these at once, so we're going to zip them all up and walk them as a single list.
    // the code's going to use intermediary plots (or probably just imitate them, not have all the logic behind them)
    plot<double> mezzanine_left;
    plot<double> mezzanine_right;
    auto mlr = mezzanine_left.real;
    auto mli = mezzanine_left.imaginary;
    auto mrr = mezzanine_right.real;
    auto mri = mezzanine_right.imaginary;
    
    // mezzanine_left will take the "a" terms:
    mlr = lhr * rhr; // ( a * c )
    mli = lhr * rhi; // ( a * di )
    
    // mezzanine_right will take the "bi" terms:
    mrr = lhi * rhi // ( b * d )
    mki = lhi * rhr // ( b * c )i
    
    plot<double> result;
    auto rr = result.real;
    auto ri = result.imaginary;
    
    rr = mlr - mrr; // ac - bd
    ri = mli + mri; // (bc + ad)i
    return result;
}
```

That doesn't look infeasible. I might even just write that first.

I've written a few steps to actually implementing polynomials in true vectorized fashion; the next task is addition, and then adding polar math for higher powers.

(I should talk about the actual implementing of it, and the choices I made along the way. Include extending the ideas in the paper to apply to polar math, and why.)

## Upon completing naïve optimizations

So at this point, I've rewritten my core logic to use as many vectorizable instructions as possible. At least, maybe. I'm looking into using a vectorized libm for pow, since that would make exponents much, much faster. I also haven't actually looked at the generated binaries yet; a lot of my later improvements have been in helping arrange the data in the optimal position to do this math, now I need to see that it's actually generating appropriate instructions.

| CPU core | `time` output
| -------- | ----------------------------------
| A100     | 11.65s user 3.81s system 99% cpu 15.489 total
| X100     | 5.89s user 2.13s system 99% cpu 8.054 total

That's an improvement -- 5.59s faster on the A100 core (32% speed-up), and saving .48s on the X100 core (... 7.5% improvement). So this had a drastic improvement on vector cores, but we're still not beating the compute cores, and I suspect there's some better vectorized layouts to look at.

For this, I care about thse symbols:
```
00000000000061ac  w    F .text  0000000000000ab4              plot<double>::operator/(plot<double> const&) const
00000000000073f2  w    F .text  00000000000006c0              plot<double>::pow_exp(unsigned int) const
0000000000005a72  w    F .text  000000000000073a              plot<double>::operator*(plot<double> const&) const
0000000000007ab2  w    F .text  0000000000000d4e              polynomial_function::eval(plot<double> const&) const
00000000000029e8 g     F .text  0000000000000404              step(plot<double>&, polynomial_function&)
```

as well as `acos@GLIBC_2.27`, `sin@GLIBC_2.27` and other transcendental functions that can be vectorized better by libraries. That'll be my next goal, because I suspect there's better operations to do.

I've dumped these with the command line `objdump -C --disassemble="plot<double>::pow_exp(unsigned int) const" -S ./nrfrac-a100 > pow_exp.S` for example, as `objdump` will follow the demangled symbol. The first thing I notice is there's not a lot of vector ops in the division operator. Lots of looping, so I may look at manually vectorizing those sections.

First improvement: calling a simple operation, but vectorized. I'm thinking addition and/or subtraction are good to start. I started with addition just to get a feel for it, and to build the first operation I'll need for the rest of my functions.

| CPU core | `time` output
| -------- | ----------------------------------
| A100     | 11.20s user 3.89s system 99% cpu 15.111 total
| X100     | 5.81s user 2.15s system 99% cpu 7.996 total

It's faster, but definitely not by much. I _do_ also wonder how much of this is about memory allocators...

Either way, just for fun, I'm gonna go ahead and vectorize the rest. (I think there's a few other things that can accelerate this... )

Out of curiosity, just after converting multiplication to vectorized operations, I timed my code and I was surprised by the outcome.

| CPU core | `time` output
| -------- | -----------------------------------
| A100     | 10.11s user 3.79s system 99% cpu 13.925 total
| X100     | 5.79s user 2.21s system 99% cpu 8.000 total

I'll admit, after the lackluster improvement of the last operation, I wasn't expecting over a second's savings on the vector core, but maybe the compiler's able to better keep stuff in the vector registers? Once I'm done vectorizing the code, I'll pull out objdump and have a look.

I'm currently rethinking the order of running operations inside the loop, but that's a question for after I've implemented the basic vector operations.

Results after adding vectorized basic math operations (addition, subtraction, division, multiplication), but no further maths, nor conversions:

| CPU core | `time` output
| -------- | ---------------------------------
| A100     | 7.75s user 3.80s system 99% cpu 11.573 total
| X100     | 5.88s user 2.28s system 99% cpu 8.170 total

A funny characteristic of this approach is that it's starting to actually have a slightly negative impact on the X100 cores, even as we're shaving multiple seconds off the vector core time. I'd guess it's from more memory allocator interactions, just before even breaking out the performance measurement tools. The approach I'm taking is allocating megabytes of memory at a time, and then freeing it an instant later. This is a situation where a custom allocator that acquires arenas makes perfect sense, since the memory consumption of a given iteration tends to be pretty fixed.

## Moving on from naïve optimizations

At this point, the standard arithmetic operations are vectorized. Exponentiation remains as an interesting problem, but that means getting vectorized transcendental functions for `cos()`, `sin()`, and `acos()`. This is tractable, but I should also weigh a few options at once here:

- Transcendental vectorization
- Arena allocator
- Switch to C++ ranges/views
- Ditch `std::vector`s where they're not needed
- Parallelize computation across cores

I'm not sure about the arena allocation, so I think I'll start with just... vectorizing more!

## An aside on build systems

Every step of this project comes with a side project, and bringing in `libvecm` (`veclibm`? I like `libvecm` more and that's what it's exported as, so that's what I'll call it) is no different. The library was built to answer the question of "how efficient _can_ we make vector math on RISC-V?" in their paper, ["An Open-Source RISC-V Vector Math Library"](https://www.ac.uma.es/arith2024/papers/An%20Open-Source%20RISC-V%20Vector%20Math%20Library.pdf). They've put their work in a public archive on GitHub, so I'm forking it to migrate it into my preferred system, Meson.

The side project is probably going to end up involving my other major side project that I've been stepping around, unfortunately: cross-compiling and ensuring Meson's configuration checker can run.

I've been playing this double-game up until now in my project; I'd write the code on my x86_64 laptop, push it to github, pull it on the dev kit (I could side-step this bit, but I hadn't cared to yet), and build it. I'd patch it, then update the sources on my main dev box & re-commit. Since I actually can natively build on the device, it simplifies a lot of testing, and meant I could just use a native build chain, instead of caring about build architecture versus host architecture. To really pull off the meson changes, and to stop using this crutch, I need to figure out cross-compiling in Meson.

### Meson's cross-compilation strategy

I wrote `riscv64-linux-gnu.txt` based on the Meson examples, and was inspired by [Chromium docs on unit testing with QEMU](https://www.chromium.org/chromium-os/developer-library/guides/testing/qemu-unit-tests-design/) ... but not enough to implement their `binfmt_misc` approach just yet. Maybe once I've already stood up RISC-V builds. (That would actually remove the need for the `exe_wrapper` directive, still the line I like the least.) To my pleasant surprise, with the exception of the syntax of my first attempt at the wrapper, it worked on the first try.

Configure a cross build with `meson setup --buildtype=$BUILD --cross-file riscv64-linux-gnu.txt build/$BUILD-rv64 src` and then from that folder, run `meson compile`, and enjoy your rv64 binaries!

```
ben at enhydra in ~/src/fractal-gen/build/release-rv64 on dev!
± uname -a
Linux enhydra 7.0.0-28-generic #28-Ubuntu SMP PREEMPT_DYNAMIC Sun Jun 21 01:01:36 UTC 2026 x86_64 GNU/Linux

ben at enhydra in ~/src/n-r-frac/build/release-rv64 on dev!
± file ./nrfrac-x100
./nrfrac-x100: ELF 64-bit LSB pie executable, UCB RISC-V, RVC, double-float ABI, version 1 (GNU/Linux), dynamically linked, interpreter /lib/ld-linux-riscv64-lp64d.so.1, BuildID[sha1]=1d86f5923f2d0ce0c013b2b5963f914288e5041a, for GNU/Linux 4.15.0, with debug_info, not stripped
```

To make this all work, I installed `qemu-user` and `qemu-system-riscv64` on top of the RISC-V toolchain. NOTE TO SELF: expand on this & get a list of packages needed to pull off these shenanigans.

With that out of the way, it's time to return from the secondary side project (tertiary project) to the secondary project of integrating `libvecm` with my Meson build system.

### Meson as its own submodule

I'm setting up meson in the `libvecm` folder, and I'm using the cross file in the parent project for setting it up:

```
# in subprojects/veclibm:
meson setup --buildtype=release --cross-file=../../riscv64-linux-gnu.txt --reconfigure ./build/release-rv64 .
```

The `exe_wrapper` directive doesn't work yet in my cross-file, but after reading through the cmake sources, I was able to convert `libvecm` to build with the cross-compiler toolchain, complete with tests! The tests even run under QEMU, just a lot slower than they do on the K3:

```
± qemu-riscv64 -L /usr/riscv64-linux-gnu ./test/src/vecm_test
# starts up & runs
# lots of output
[----------] Global test environment tear-down
[==========] 141 tests from 81 test suites ran. (138257 ms total)
[  PASSED  ] 141 tests.
```

I also set up the dependency export for `libvecm`; with some additional work, I now have its builds integrated into my overall project, and running cleanly. (Well, the `libvecm` build sure isn't clean, but that's going to be cleanup for _after_ this. That library is messy.)

## Vectorizing transcendentals and beyond

At this point, I've optimized the basic math operations. Unfortunately for me, while that's important and speeds up both applying the coefficient of a term and the summation of all the terms of a polynomial faster, it doesn't address the exponent, or the fact that the current `pow()` method relies on scalar computation for the polar conversions and in fact all its math. So, now that I have vectorized libm functions available, it's time to resolve that!

As I was building this to test, I was really concerned it'd turn out significantly slower on the physical machine after the qemu runs got much slower as I vectorized my code... but that's probably more about the QEMU RVV implementation being slow (and only being 128b, so we're not really doing anything faster). The results on-device of just vectorizing polar conversions:

| CPU core | `time` output
| -------- | -------------------------------
| A100     | 4.25s user 4.03s system 99% cpu 8.295 total
| X100     | 4.44s user 2.47s system 99% cpu 6.922 total

Well. So it's ... better user time, but massively increased system time? I've been measuring entirely based on user time for now, but I'll have to deal with that rising system time eventually. (My guess is that this is partially due to increased cost of memory allocations, since I'm doing a bunch more for the math right now.) Still, exciting! This marks the first time the A100 core has spent less user time computing than the X100, and we're still not done. The higher-order power function needs to be vectorized still, and I expect that'll provide yet another boon to these numbers.

# Warehouse of templates & ideas

| CPU core | `time` output
| -------- | -------------------------------
| A100     | 
| X100     |

## Thinking about how to portray results

(This section is to be expanded upon once I'm done vectorizing. I don't want to get distracted with the graphics side while I can still do more computational improvement.)

I think each root should have its angle define the color in HSV, and the proximity to the root in its final iteration defines the Value.
