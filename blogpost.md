# Pretty drawings and fast math

The SpaceMIT K3 is a really interesting new dev kit; it's a RISC-V dev board with an unusual big/little CPU design: 8 X100 compute cores at 2.4GHz, and 8 A100 vector cores running at 2GHz. The X100 cores fully support the RVA23 profile with 256-bit vector registers, while the A100 cores support _most_ of the RVA23 profile (everything except the hypervisor instructions) but they have 1024-bit vector registers. That bank of enormous vector registers really spurred my interest, and I figured I could write a small toy application to see what they're capable of. As it happens, I realized I'd lost the sources to a project I'd done years and years ago, and so I set out to make a new Newton-Rapheson fractal generator.

Newton's method of approximation is a useful way to find the roots of polynomials that either can't be factored, or where it's not useful to factor a polynomial (if the roots don't have closed forms, for instance.) For a polynomial function, `f(z)`, the method is to choose a starting point, `z_0`, and apply the following step function repeatedly:

```
f(z_{n+1}) = z_n - f(z_n) / f'(z_n)
```

Either this eventually converges on a root, it may never approach a root and simply continually bounce around, or it might diverge infinitely; generally speaking, it will approach the "nearest" root readily, in the Reals. The same cannot be said when `z` is a complex number. Since multiplication and division both behave very differently on the complex plane, Newton's approximation generates much more interesting results. The fractals shown in this post are plots of what root the N-R algorithm arrives at starting from a given point, colorized by the  root it approaches. This happens to be a neat, easy way to begin exploring optimizing math, and it even produces pretty pictures at the end.

There's plenty of tools that already exist to generate fractals, but I wanted a toy that was easy to convert to vector instructions; pre-existing projects would get in the way, and potentially mask clear performance impacts. To really show this behavior, I started with a very naïve implementation, and then started hunting for optimizations on the way to vectorizing my math.

### An aside on pre-requisites & toolchains

I built this project against g++ 15.2.0; working from Kubuntu, this project needed the usual `build-essentials`, as well as `meson` to start. In addition, on my x64 laptop I needed `gcc-riscv64-linux-gnu`, `g++-riscv64-linux-gnu`, `cpuid`, `libc6-dev-riscv64-cross` to cross-compile for RISC-V (henceforth 'rv64'), as well as `qemu-user`, `qemu-system-riscv64`, and `u-boot-qemu` to emulate the target and test my vectorizations.

## The straightforward implementation

I started off with `std::vector<std::complex<double>>` as the core storage structure; the actual rows of points or pixels are delineated at export and in initializing the structure. In this implementation, the implementation of applying the function is close to the simplest form it'd ever have (but I did end up optimizing away the extra elements):

```c++
// class polynomial_function ...
    std::complex<double> eval(std::complex<double> const &point) const {
        auto result = std::complex<double>{};
        for (size_t i = 0; i < m_coefficients.size(); i++) {
            result += (std::norm(m_coefficients[i]) > 0) ? m_coefficients[i] * ((i > 0) ? std::pow(point, i) : 1) : 0;
        }
        return result;
    }
```

However, for all that this was simple to write, it wasn't particularly high-performance.


| Unoptimized performance
| ----------------------------------------
| CPU core  | `time` output
| --------- | ------------------------------------------------------
| A100      | real    1m40.149s, user    1m39.823s, sys     0m0.232s
| X100      | real    0m50.415s, user    0m50.269s, sys     0m0.105s

| Optimized performance (-O2)
| ----------------------------------------
| CPU core  | `time` output
| --------- | ------------------------------------------------------
| A100      | real    0m37.280s, user    0m36.937s, sys     0m0.304s
| X100      | real    0m18.109s, user    0m18.000s, sys     0m0.101s

So, that's kinda disappointing. On the other hand, there's lots of room to improve... and we'll see it soon. Before I got to optimizing, I decided to scratch an itch & address the logistics of running code on the A100 cores.

## An aside on initialization

Up to this point, I'd been making use of a shell I'd assigned to the A100 cores, simply by writing its pid to `/proc/set_ai_thread`:

```sh
echo $$ > /prox/set_ai_thread
```

But this means that that whole shell is dedicated to the secondary cores, and for most tasks that's just not desirable. Instead, I wanted to be able to shim the code execution as a whole. I found [brucehoult/k3_ai][k3_ai] and [c3rb3ru5d3d53c's blog post][cerberusdedsec] on hooking libc initialization, and decided to write my own implementation.

I originally wrote this code as a shim to run before main, but in the interest of ensuring that nothing has a chance to interact with the register buffers before the cores are set, I decided to move the code to run pre-libc initialization. Unfortunately, that means that I can't depend on any libc calls--so I have no access to `close()`, `getpid()`, `open()`, or `write()`. These libc functions are functionally wrappers around Linux syscalls, i.e. the following:

```asm
// as many arguments as a syscall has, those arguments are filled in.
// syscall(int num, arg1 = 0, arg2 = 0, arg3 = 0, arg4 = 0, arg5 = 0, arg6 = 0)

// x86_64
mov num, %rax
mov arg1, %rdi
mov arg2, %rsi
mov arg3, %rdx
mov arg4, %r10
mov arg5, %r8
mov arg6, %r9
syscall
// result in %rax, error in %rdx

// rv64
mov num, %a7
mov arg1, %a0
mov arg2, %a1
mov arg3, %a2
mov arg4, %a3
mov arg5, %a4
mov arg6, %a5
ecall
// return is %a0, error value in %a1
```

Practically speaking, I could probably use the libc functions, as they _probably_ don't depend on any library mechanisms... but on the one hand I don't like "probably" and on the other, this was a chance to have a closer look at the messy details.

I found [Félix Cloutier's guide to GCC extended asm][cloutier] absolutely indespensible here. Unfortunately, due to the differences in age and in ethos of asm syntax, I had to write two different forms of assembly to handle the different architectures. The RISC-V syntax causes some build warnings by default, since it appears to the compiler that the variables aren't read; I'd happily take suggestions on how to get it closer to the fairly elegant x86_64 form.

With that hand-rolled assembly worked out, I was able to hand-compile `pre-crt.c` and specially target different cores. Geting Meson to handle the multiple objects was fun; I needed to declare an explicit no-opt object for the pre-runtime code. It turns out, an object which isn't externally linked and which isn't directly referenced by any known symbols just gets optimized out, and that was where my syscalls were going!

After confirming that I was getting those 1024-bit vector registers when I launched the `-a100` variant executable, I could return to vectorizing.

## An initial foray

Before I started on the largest optimizations, I decided to test out my ideas with the last step of polynomial computation first. The functions I'm using are of the form:

```
f(z) = c_0 + c_1 * z + c_2 * z^2 + ... + c_n * z^n
```

As a very first task, I did a vector-to-scalar sum of the terms' imaginary and real components, after performing each term's computation separately. Since this is relatively few terms, it meant that I was definitely leaving a lot of performance benefit on the table. I eventually revised this logic away, but in the interim, it gave me my first significant performance win:

| CPU core | `time` output
| -------- | ---------------------------------
| A100     | 17.24s user 0.12s system 99% cpu 17.373 total
| X100     | 6.37s user 0.06s system 99% cpu 6.444 total

I will note, from here on out the system time consumption starts to increase dramatically for the A100 cores; I suspect this is due to memory management issues, but I haven't explored it yet. I have focused for the time being in this project on optimizing the user time. Additionally, this and all further builds are done with `-O2`.

## "Six of one, half a dozen of the other!"

Currently, the structure is a `std::vector<std::complex<double>>`; it's laid out like so:

```
[ r_0 ][ i_0 ][ r_1 ][ i_1 ]...[ r_n ][ i_n ]
```

This means that each action to load real values and imaginary values has to sort reals & imaginaries into separate structures. Realistically, as long as their indices can be lined up, there's no reason not to store the values in two vectors:

```
[ r_0 ][ r_1 ][ r_2 ][ r_3 ]...[ r_n ]
[ i_0 ][ i_1 ][ i_2 ][ i_3 ]...[ i_n ]
```

This allows for loading values directly into registers, as discussed in an [article on modified data layouts for vectorized complex math][popovici] I found in my research for this project. My next step was to set aside my `std::vector<std::complex<T>>` approach for `plot<T>`, which stores a pair of `std::vector<double>`s. While RVV (**R**ISC-**V** **V**ector extension) does have instructions to deinterlace the data on load, that seems likely to cause more confusion and still require extra loads and writes. For my purposes, obscuring the storage of individual values works more than well enough.

One of the downsides of setting aside `std::complex<>` is the loss of operators, but since there's no SIMD `std::complex<>` instructions, this was something of a moot point. As an abstraction, the `plot` replaced the individual terms in computing the polynomial; this avoided needing to expose the contents of the plot, and means that all the SIMD details get contained in a standalone module. It also means that bringing other functions than (strictly finite) polynomials can operate entirely on plots, and I'll already have the math worked out.

To implement our full algorithm, we have five functions that need to be vectorized: addition, subtraction, multiplication, division, and exponentiation. We'll leave exponentiation for later; operations on the Complex plane do not always have direct solutions, but there is an elegant option available.

Addition (viz. subtraction) is straightforward; like terms sum with like, we move on. Multiplication, on the other hand, presents the first particularly interesting diversion:

```
x = (a + bi), y = (c + di)
x * y = (a + bi) * (c + di)
x * y = (a * c) - (b * d) + (a * d)i + (b * c)i
```

Implementing this in a time-efficient manner is somewhat expensive on RAM, but we have a lot to work with. I made use of "mezzanine" `std::vector<T>`s to store individual terms, allowing for an overall elegant approach (with the following pseudo-code):

```
multiply(plot lhs, plot rhs) :
    // Each plot is composed of a pair of vectors, real & imaginary. I used "lh" and "rh" as mnemonics, with "r" and "i" suffixes
    // used throughout
    auto lhr = lhs.real;
    auto lhi = lhs.imaginary;
    auto rhr = rhs.real;
    auto rhi = rhs.imaginary;
    
    // The operation of multiplying terms works best with "mezzanine" containers, so we'll introduce "ml" and "mr" mnemonics here.
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

The pseudocode more or less directly translated into the C++ for the first pass; looping instead of directly multiplying the `std::vector<>`s since I didn't bother writing an operator for that, still provided a bit of a boost. I left the exponents for later, and decided to see how much better the code was with just this change. After all, I was seeing plenty of AVX2 calls in the x64 output; unfortunately, GCC doesn't really have much in the way of RVV support yet, so I had to get to that later.

With three of the core operations out of the way, I implemented a similar mezzanine-based approach for division, and I was able to see some not-insignificant improvements in runtime:

| CPU core | `time` output
| -------- | ----------------------------------
| A100     | 11.65s user 3.81s system 99% cpu 15.489 total
| X100     | 5.89s user 2.13s system 99% cpu 8.054 total

That's decent, less than I'd hoped for but certainly an improvement. Looking at user time, 5.59s faster on the A100 core (32% speed-up), and saving .48s on the X100 core (... 7.5% improvement). While that's a massive improvement over the last run, there's absolutely more to do.

## Intrinsics and platform-specifics

From my reading, I knew that on most platforms there's fixed-size vector registers, for instance SSE uses 128b registers, while AVX2 is 256b; RVV is a different beast, using the same instruction set for different register sizes, so the same code can run on cores with different register sizes--you just determine the size of each iteration at runtime. Combined with techniques to cluster multiple registers into a set to perform the same operation over even larger vectors, the platform pushes engineers to think in terms of flexible code, and write your logic in a way that abstracts the exact step size.

After examining the output from `objdump` for the functions I'd implemented, I decided to start with basic arithmetic; after completing vectorization only for the basic operations, I actually made _significant_ progress:

| CPU core | `time` output
| -------- | ---------------------------------
| A100     | 7.75s user 3.80s system 99% cpu 11.573 total
| X100     | 5.88s user 2.28s system 99% cpu 8.170 total

This was a very impressive jump, and shows we're starting to really close the gap between the cores.

An interesting trait of multiplication here is, RVV has instructions that support a scalar input as well as a vector input without splatting the scalar across a vector register set; I implemented multiplication for both Vec * Vec & Scalar * Vec, and was able to use the same syntax for both cases. That made for easier to read code, as well as more efficient vectorization.

While this was good for the basic arithmetic, to tackle exponentiation in a reasonable manner we'll want to step out of Cartesian coordinates and apply DeMoivre's Theorem.

## Coordinate systems and higher powers

Since we're only dealing with integral powers for this binary, we can make use of DeMoivre's Theorem to simplify our exponents:

```
For z = r∠θ, n ∈ ℕ
z^n = r^n ∠(nθ)
z^n = r^n * cos(nθ) + r^n * i * sin(nθ)
```

Starting from the form `a + bi`, then, involves some additional conversions:

```
r = sqrt(a^2 + b^2),
    ⎧ if b >= 0, arccos (a / r)
t = ⎨
    ⎩ if b < 0, 2 * π - arccos (a / r)
```

This requires transcendental functions that just aren't vectorized in the standard `libm`! At this point I was worried that I'd run out of road; I'm not up to the task of vectorizing that math right now, but it turns out a team called Rivos released a [library][veclibm] and [published an article about it.][veclibm-article]. Since they archived the project, I have forked it and renamed it `libvecm`. Here's also where we can take a brief diversion to discuss how I'd been building this project up to this point.

## An aside on build systems

The side project is probably going to end up involving my other major side project that I've been stepping around, unfortunately: cross-compiling and ensuring Meson's configuration checker can run.

I've been playing this double-game up until now in my project; I'd write the code on my x86_64 laptop, push it to github, pull it on the dev kit (I could side-step this bit, but I hadn't cared to yet), and build it. I'd patch it, then update the sources on my main dev box & re-commit. Since I actually can natively build on the device, it simplifies a lot of testing, and meant I could just use a native build chain, instead of caring about build architecture versus host architecture. To really pull off the meson changes, and to stop using this crutch, I need to figure out cross-compiling in Meson.

### Meson's cross-compilation strategy

I wrote `riscv64-linux-gnu.txt` based on the Meson examples, and was inspired by [Chromium docs on unit testing with QEMU][cr-qemu] ... but not enough to implement their `binfmt_misc` approach just yet. I still haven't gotten the `exe_wrapper` directive right yet, so that's getting more appealing.

Configure a cross build with `meson setup --buildtype=$BUILD --cross-file riscv64-linux-gnu.txt build/$BUILD-rv64 src` and then from that folder, run `meson compile`, and enjoy your rv64 binaries!

```
ben at enhydra in ~/src/fractal-gen/build/release-rv64 on dev!
± uname -a
Linux enhydra 7.0.0-28-generic #28-Ubuntu SMP PREEMPT_DYNAMIC Sun Jun 21 01:01:36 UTC 2026 x86_64 GNU/Linux

ben at enhydra in ~/src/n-r-frac/build/release-rv64 on dev!
± file ./nrfrac-x100
./nrfrac-x100: ELF 64-bit LSB pie executable, UCB RISC-V, RVC, double-float ABI, version 1 (GNU/Linux), dynamically linked, interpreter /lib/ld-linux-riscv64-lp64d.so.1, BuildID[sha1]=1d86f5923f2d0ce0c013b2b5963f914288e5041a, for GNU/Linux 4.15.0, with debug_info, not stripped
```

To make this all work, I installed `qemu-user` and `qemu-system-riscv64` on top of the RISC-V cross-compilation toolchain above. I tried to limit the fiddling required here; packages required are listed up above.
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

As I was building this to test, I was really concerned it'd turn out significantly slower on the physical machine after the qemu runs got much slower as I vectorized my code... but that's probably more about the QEMU RVV implementation being slow (and only being 128b, so we're not really doing anything faster). The results on-device after vectorizing polar conversions & root-finding:

| CPU core | `time` output
| -------- | -------------------------------
| A100     | 4.25s user 4.03s system 99% cpu 8.295 total
| X100     | 4.44s user 2.47s system 99% cpu 6.922 total

Well. So it's ... better user time, but massively increased system time? I've been measuring entirely based on user time for now, but I'll have to deal with that rising system time eventually. (My guess is that this is partially due to increased cost of memory allocations, since I'm doing a bunch more for the math right now.) Still, exciting! This marks the first time the A100 core has spent less user time computing than the X100, and we're still not done. The higher-order power function needs to be vectorized still, and I expect that'll provide yet another boon to these numbers.

### Extending libvecm: `rvvlm_pow()` with a scalar exponent

`libvecm` has a lot of useful functions, but it doesn't really expose the best feature of RVV: scalar inputs for vector operations where you have one parameter that's a vector, and the other's a scalar that gets applied to the entire vector. I want an easy way to do some form of `pow(in_vec, exp, out_vec);` where `exp` is a scalar value. Right now, I'd need to splat it out across an entire `plot`-sized vector to achieve that, and that's just foolish. As such, it's time to extend `libvecm` with some new capabilities.

In practice, while my freshly written `rvvlm_powS()` handles the case I built it for, this is currently a fairly fragile operation. Depending on how I feel about future projects, I may end up rewriting `libvecm` into a C++ library to be able to template this instead of dealing with their macro choices. As it is, I already have the starts of a better set of functions built on templates that may simplify writing some of the more explicit intrinsics for their quasi-LMUL-independent code. I did end up needing to splat out the `exp` value, but only across a vector register group. I can cope with that.

| CPU core | `time` output
| -------- | -------------------------------
| A100     | 3.79s user 4.05s system 99% cpu 7.858 total
| X100     | 4.08s user 2.49s system 99% cpu 6.573 total

That's more like it! I realize the system time is much higher on these cores; that's going to matter a lot less once I set up an arena allocator. By just keeping all allocated plot elements and reusing them across calls, we should be able to avoid additional memory management burden. Since my guess is that these A100 cores, in addition to being 400MHz slower, have less-efficient routes to make system calls (perhaps the underlying malloc calls are having to transition CPU cores to be handled?) I'm expecting to see a dramatic reduction in system time cost for both cores, but especially the A100.

Now we're starting to get into the points where caring about how memory is allocated starts to matter, or writing faster accessors. At some point, there's also potentially rewriting functions to do more in each loop. While this sort of manual intrinsic usage is generally somewhere between unnecessary, excessive, or foolish on x86_64, the history of RISC-V optimized compilers is short, and the history of support for the vector instructions is even shorter, so we're letting them sit a little closer to the surface for now. Besides, this is part of the fun!

### Moving to views

I rewrote the logic to use `std::views::zip` and `std::views::chunk` to stop needing to manually move addresses around, and it made the code a lot cleaner, as well as having interesting behaviors on computation times:

| CPU core | `time` output
| -------- | -------------------------------
| A100     | 3.81s user 4.02s system 99% cpu 7.838 total
| X100     | 4.14s user 2.30s system 99% cpu 6.441 total

In both cores, more time was spent in user actions than system time, but less time was used overall; 0.02s on the A100 core, but 0.13s on the X100 ... without reaching for a flame graph just yet, my estimation is that a lot of that system overhead is simply memory allocation. The vector logic has a lot of extra/interim data structures, which has a non-trivial cost when compared to moving iterators. The upside of these data structures, however, is their utility for parallelization... and 1% extra time in the single-threaded case is not a major concern, especially when it did materally reduce _overall_ time spent.

## Future work

- Parallelizing computation
- `std::pmr` for memory allocation in all the vectors.
- `perf` and flamegraph investigation
- `libvecm` cleanup and possible rewrite in C++
- Generalized N-R fractals

---

[k3_ai]: https://github.com/brucehoult/k3_ai/
[cerberusdedsec]: https://github.com/c3rb3ru5d3d53c/c3rb3ru5d3d53c.github.io/blob/master/content/posts/docs/hooking-libc.en.md.md
[cloutier]: https://www.felixcloutier.com/documents/gcc-asm.html
[popovici]: https://aiichironakano.github.io/cs653/Popovici-ComplexSIMD-HPEC17.pdf
[veclibm]: https://github.com/rivosinc/veclibm (Archived project)
[veclibm-article]: https://www.ac.uma.es/arith2024/papers/An%20Open-Source%20RISC-V%20Vector%20Math%20Library.pdf
[cr-qemu]: https://www.chromium.org/chromium-os/developer-library/guides/testing/qemu-unit-tests-design/
