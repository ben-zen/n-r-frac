# Pretty drawings and fast math

![f(z) = z^3 - 1: roots at 1+0i, -0.5+0.866i, -0.5-0.866i](./z3-1.png)

I've been itching to work with some new hardware recently. The progress I'd been hearing about in RISC-V had me interested to see what was possible now; last time I'd looked, it was arriving in cool little dev kits with RV-32 cores. Well, mainline Linux support arrived for RISC-V, and I picked up this interesting new dev kit, the [Sipeed K3](https://sipeed.com/k3).

This machine has an [unusual big/little CPU design](https://www.spacemit.com/products/keystone/k3): 8 compute cores, 8 vector cores. The compute (X100) cores fully support the RVA23 profile with 256-bit vector registers, while the vector (A100) cores support _most_ of the RVA23 profile (everything except the hypervisor instructions) but in exchange they have 1024-bit vector registers. That bank of enormous vector registers really spurred my interest, and I figured I could write a small toy application to see what they're capable of. As it happens, I realized I'd lost the sources to a project I'd done years and years ago, and so I set out to make a new [Newton-Rapheson fractal](https://en.wikipedia.org/wiki/Newton_fractal) generator.

In short, Newton's method of approximation is a useful way to find the roots of a differentiable function (including in the complex plane, which we'll be using here); for a function `f(z)`, and a starting point `z_0`, the method is to iterate on the following operation:

```
f(z_{n+1}) = z_n - f(z_n) / f'(z_n)
```

For most points, this will eventually converge to a root of the function, assuming it has them. In some cases, it may behave abnormally at boundaries between "regions of stability" for roots; in the complex plane, this results in fascinating fractals as presented in the first figure.

There's plenty of tools that already exist to generate fractals, but I wanted a toy that was easy to convert to vector instructions; pre-existing projects would get in the way, and potentially mask clear performance impacts. To really show this behavior, I started with a very naïve implementation, and then started hunting for optimizations on the way to vectorizing my math.

### An aside on methodology

For the purposes of this investigation, the polynomial was `f(z) = z^3-1`, while it was graphed from -5-3i to 5+3i, with a horizontal resolution of 1000px and a vertical resolution of 600px. All numbers are based on purely computing the chart; graphics rendering was not considered at the time as it was secondary to the goal of getting math to go faster. It is therefore excluded from all the computations and benchmarking in this article. After I finished my optimizing work I added graphics synthesis to PPM, then used `image-magick` and `pngcrush` to arrive at the figures for this post.

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

| CPU core  | `time` output
| --------- | ------------------------------------------------------
| A100      | real    1m40.149s, user    1m39.823s, sys     0m0.232s
| X100      | real    0m50.415s, user    0m50.269s, sys     0m0.105s

With the most basic of improvements, compiling `-O2` (which we need to to get vector instructions on x64):

| CPU core  | `time` output
| --------- | ------------------------------------------------------
| A100      | real    0m37.280s, user    0m36.937s, sys     0m0.304s
| X100      | real    0m18.109s, user    0m18.000s, sys     0m0.101s

So, that's kinda disappointing. On the other hand, there's lots of room to improve... and we'll see it soon. Before I got to optimizing, I decided to scratch an itch & address the logistics of running code on the A100 cores.

## An aside on initialization (or, "on `asm` syntax")

The K3 has a few peculiarities to how it's best put to use. One of them is that it effectively blocks access to the A100 cores unless a thread is tagged. Up to this point, I'd been making use of a shell I'd assigned to the A100 cores, simply by writing its pid to `/proc/set_ai_thread`:

```sh
echo $$ > /proc/set_ai_thread
```

But this means that that whole shell is dedicated to the secondary cores, and that gets unwieldy when trying to automate testing, for instance. Instead, I wanted to be able to pin the process at launch. I found [brucehoult/k3_ai][k3_ai] with the cool library and [c3rb3ru5d3d53c's blog post][cerberusdedsec] on hooking libc initialization, and decided to write my own implementation.

I originally wrote this code as a shim to run before main, but in the interest of ensuring that nothing has a chance to interact with the register buffers before the cores are set, I decided to move the code to run pre-libc initialization. Unfortunately, that means that I can't depend on any libc calls--so I have no access to `close()`, `getpid()`, `open()`, or `write()`. These libc functions are functionally wrappers around Linux syscalls, loading parameters into registers and lodging the request with the kernel.

As an example, here's the implementation of `getpid()` from [my `pre_crt` library][n-r-frac-pre-crt]:

```c
#if defined(__x86_64__)
__attribute__((always_inline))
inline
int
getpid() {
    int retval = -1;
    asm volatile(
        "movl %[getpid], %%eax \n "
        "syscall"
        : "+a"(retval) // output
        : [getpid]"i"(SYS_getpid)); // input
    return retval;
}
#elif defined (__riscv)
__attribute__((always_inline))
inline
int
getpid() {
    register int r_a0 asm ("a0") = 0;
    asm volatile (
        "li a7, %[getpid] \n"
        "ecall"
        : "=r"(r_a0)
        : [getpid]"i"(SYS_getpid) //
    );
    return r_a0;
}
#endif
```

I found [Félix Cloutier's guide to GCC extended asm][cloutier] absolutely indespensible here. Unfortunately, due to the differences in age and in ethos of asm syntax, I had to write two different forms of assembly to handle the different architectures. The RISC-V syntax causes some build warnings by default, since it appears to the compiler that the variables aren't read; I'd happily take suggestions on how to get it closer to the fairly elegant x86_64 form.

After confirming that I was getting those 1024-bit vector registers when I launched the `-a100` variant executable, I could return to vectorizing. Geting Meson to handle the multiple objects was fun, though; I needed to declare an explicit no-opt object for the pre-runtime code. Optimizations were eating my syscalls!

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

At this point in development, the structure is a `std::vector<std::complex<double>>`, which is laid out like so:

```
[ r_0 ][ i_0 ][ r_1 ][ i_1 ]...[ r_n ][ i_n ]
```

This means that each action to load real values and imaginary values has to sort reals & imaginaries into separate structures. Realistically, as long as their indices can be lined up, there's no reason not to store the values in two vectors:

```
[ r_0 ][ r_1 ][ r_2 ][ r_3 ]...[ r_n ]
[ i_0 ][ i_1 ][ i_2 ][ i_3 ]...[ i_n ]
```

This allows for loading values directly into registers, as discussed in an [article on modified data layouts for vectorized complex math][popovici] I found in my research for this project. My next step was to set aside my `std::vector<std::complex<T>>` approach for `plot<T>`, which stores a pair of `std::vector<double>`s. While RVV (**R**ISC-**V** **V**ector extension) does have instructions to deinterlace the data on load, that seems likely to cause more confusion and still require extra loads and writes. For my purposes, dividing the storage of individual values works more than well enough.

One of the downsides of setting aside `std::complex<>` is the loss of operators, but since there's no SIMD `std::complex<>` instructions, this was something of a moot point. As an abstraction, the `plot` replaced the individual terms in computing the polynomial; this avoided needing to expose the contents of the plot, and means that all the SIMD details get contained in a standalone module. It also means that bringing other functions than (strictly finite) polynomials can operate entirely on plots, and I'll already have the math worked out.

To implement our full algorithm, we have five functions that need to be vectorized: addition, subtraction, multiplication, division, and powers. We'll leave powers for later, because that takes some lateral thinking. 

Addition (viz. subtraction) is straightforward; like terms sum with like, we move on. Multiplication, on the other hand, presents the first particularly interesting diversion:

```
x = (a + bi), y = (c + di)
x * y = (a + bi) * (c + di)
x * y = (a * c) - (b * d) + (a * d)i + (b * c)i
```

Implementing this in a time-efficient manner is somewhat expensive on RAM, but we have a lot to work with. I made use of "mezzanine" `std::vector<T>`s to store individual terms, allowing for a [fairly legible approach][n-r-frac-plot].

With the core functions rewritten, GCC was happily generating AVX2 calls on x64, but on RISC-V I was still seeing scalar math. That being said, even just better memory layouts led to improved results: 

| CPU core | `time` output
| -------- | ----------------------------------
| A100     | 11.65s user 3.81s system 99% cpu 15.489 total
| X100     | 5.89s user 2.13s system 99% cpu 8.054 total

Looking at user time, that's 5.59s faster on the A100 core (32% speed-up), and saving .48s on the X100 core (... 7.5% improvement). Time to fix the lack of vector results.

## Intrinsics and platform-specifics

From my reading, I knew that on most platforms there's fixed-size vector registers, for instance SSE uses 128b registers, while AVX2 is 256b; RVV is a different beast, using the same instruction set for different register sizes, so the same code can run on cores with different register sizes--you just determine the size of each iteration at runtime. The RVV C intrinsics capture that variable register sizing fairly elegantly:

```cpp
template<>
inline
void
riscv_vec_add<double>(std::vector<double> &result, std::vector<double> const &lhs, std::vector<double> const &rhs) {

    auto lr_start = lhs.cbegin();
    auto rr_start = rhs.cbegin();
    auto dr_start = result.begin();
    // I'll start with getting the length of the vectors being added:
    auto rrem = lhs.size();

    do {
        auto vl = __riscv_vsetvl_e64m8(rrem);
        rrem -= vl;
        // Get the next set of left & right operands, and destination values.
        auto lr_end = lr_start + vl;
        auto rr_end = rr_start + vl;
        auto dr_end = dr_start + vl;
        auto lr_range = std::span(lr_start, vl);
        auto rr_range = std::span(rr_start, vl);
        auto dr_range = std::span(dr_start, vl);

        vfloat64m8_t vec_lhs = __riscv_vle64_v_f64m8(lr_range.data(), vl);
        vfloat64m8_t vec_rhs = __riscv_vle64_v_f64m8(rr_range.data(), vl);
        vfloat64m8_t vec_sum = __riscv_vfadd_vv_f64m8(vec_lhs, vec_rhs, vl);

        // Store the results, figure out how to do this with reals
        __riscv_vse64_v_f64m8(dr_range.data(), vec_sum, vl);

        lr_start = lr_end;
        rr_start = rr_end;
        dr_start = dr_end;
    } while (lr_start != lhs.cend());
}
```

`__riscv_vsetvl_e64m8()` is a function that takes the number of elements to be computed over, and provides the number that can be consumed in an operation; the `e64m8` section declares that elements are 64 bits wide (i.e. `double`), while each variable will span 8 vector registers; RVV's operations can be run across banks of registers, making as much use of load and store stages as possible. Depending on the cost of load/store time-wise, this may be a point to consider tuning. After adding implementations for the basic arithmetic operations, I ran the tests again:

| CPU core | `time` output
| -------- | ---------------------------------
| A100     | 7.75s user 3.80s system 99% cpu 11.573 total
| X100     | 5.88s user 2.28s system 99% cpu 8.170 total

This was a very impressive jump; another 33% improvement on the A100 core, and there's still so much logic to optimize.

An interesting trait of multiplication here is, RVV has instructions that support a scalar input as well as a vector input without splatting the scalar across a vector register set; I implemented multiplication for both Vec * Vec & Scalar * Vec, and was able to use the same syntax for both cases. That made for easier to read code, as well as more efficient vectorization.

## Coordinate systems and higher powers

I mentioned previously that powers would require some lateral thinking. In the complex plane, exponents create, well, exponentially greater headaches. However, we have a bit of an out in the form of [de Moivre's Theorem][demoivres] here:

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

This requires transcendental functions that just aren't vectorized in the standard `libm`! At this point I was worried that I'd run out of road; I'm not up to the task of vectorizing that math right now, but it turns out a team called Rivos released a [library][veclibm] and [published an article about it.][veclibm-article]. Since they archived the project, I have forked it and renamed it [`libvecm`][libvecm]. After some effort I was going to document here but decided might be a separate blog post another day, I got `libvecm` building in Meson even when hosted on x64, and running tests under `qemu`.

## Vectorizing transcendentals and beyond

I wasn't really sure how much more optimization I could really squeeze out of this code. Replacing all of the trigonometric functions from libm with libvecm, I was hoping I'd get at least some improvements, but I wasn't quite ready for...

| CPU core | `time` output
| -------- | -------------------------------
| A100     | 4.25s user 4.03s system 99% cpu 8.295 total
| X100     | 4.44s user 2.47s system 99% cpu 6.922 total

... another 45% improvement in user time on the A100? Wild.

### Extending libvecm: `rvvlm_pow()` with a scalar exponent

`libvecm` has a lot of useful functions, but it doesn't really expose the best feature of RVV: scalar inputs for vector operations where you have one parameter that's a vector, and the other's a scalar that gets applied to the entire vector. I want an easy way to do some form of `pow(in_vec, exp, out_vec);` where `exp` is a scalar value. Right now, I'd need to splat it out across an entire `plot`-sized vector to achieve that, and that's just foolish. As such, it's time to extend `libvecm` with some new capabilities.

In practice, while my freshly written `rvvlm_powS()` handles the case I built it for, this is currently a fairly fragile operation. Depending on how I feel about future projects, I may end up rewriting `libvecm` into a C++ library to be able to template this instead of dealing with their macro choices. As it is, I already have the starts of a better set of functions built on templates that may simplify writing some of the more explicit intrinsics for their quasi-LMUL-independent code. I did end up needing to splat out the `exp` value, but only across a vector register group. I can cope with that.

| CPU core | `time` output
| -------- | -------------------------------
| A100     | 3.79s user 4.05s system 99% cpu 7.858 total
| X100     | 4.08s user 2.49s system 99% cpu 6.573 total

That change broke the 4s barrier on the A100... and now it's really clearly faster than the X100 core. At least in user time. (I'll deal with system time another... time.)

Now we're starting to get into the points where caring about how memory is allocated starts to matter, or writing faster accessors. At some point, there's also potentially rewriting functions to do more in each loop. While this sort of manual intrinsic usage is generally somewhere between unnecessary, excessive, or foolish on x86_64, the history of RISC-V optimized compilers is short, and the history of support for the vector instructions is even shorter, so we're letting them sit a little closer to the surface for now. Besides, this is part of the fun!

### Moving to views

I rewrote the logic to use `std::views::zip` and `std::views::chunk` to stop needing to manually move addresses around, and it made the code a lot cleaner, as well as having interesting behaviors on computation times:

| CPU core | `time` output
| -------- | -------------------------------
| A100     | 3.81s user 4.02s system 99% cpu 7.838 total
| X100     | 4.14s user 2.30s system 99% cpu 6.441 total

In both cores, slightly more more time was spent in user actions and less in system time, but also less time was used overall; 0.02s on the A100 core, but 0.13s on the X100 ... without reaching for a flame graph just yet, my estimation is that a lot of that system overhead is simply memory allocation. The view zip & chunk logic has a lot of extra/interim data structures, which has a non-trivial cost when compared to moving iterators. The upside of these data structures, however, is their utility for parallelization... and 1% extra time in the single-threaded case is not a major concern, especially when it did materally reduce _overall_ time spent. For now, I'm putting any of those investigations into future work, and moving on to drawing the pretty pictures.

## Getting pretty pictures

I did end up building some pretty graphics out of this. You've already seen one at the start of the article, but here's a more complex example:

![f(z) = z^8 + 15z^4 - 16, plotted from -2-2i to 2+2i](./z8_15z4_16.png)

To automate selecting colors for roots, I took the polar plot of roots and applied it to the HSV color space. Each root has a hue mapped to its angle, while to distinguish similar angles but different points, saturation is adjusted for the magnitude of a root. The shading of the plot depends on how close a given pixel is to its intended root, with points that didn't converge to any root painted black.

I'd originally drawn this plot with the same cutoff as I used for computing the roots themselves, but I found that that produced graphs which were just too coarse. Instead, I took into account proximity to the goal value, with a gradation between 10^-6^ to 10^-8^ translating into the shade of a pixel. This adjusted window produced the graphics you see here, highlighting the striking instability of the roots with 0 imaginary component, versus the very stable regions for other roots.

Wtih that, this project achieved both of my primary initial goals. A lot of the code in it can be extracted into separate libraries for better reuse, and there's definitely room to improve, but for the moment this is satisfying.

## Future work

I'm still tweaking the logic a bit, and I haven't implemented a parser for polynomials so far (or any arguments really), so the program needs to be recompiled each time with a new function. It's not ideal, but it's been a great little test-bench to play around with a new architecture, and try a few new things. Some extension points, in no particular order:

- Command line arguments
- Parallelizing computation
- Improve memory reuse; `std::pmr::vector<>` doesn't seem to be as useful as I'd expected.
- Investigate different vector register allocations
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
[n-r-frac-pre-crt]: https://github.com/ben-zen/n-r-frac/blob/dev/src/pre_crt.c
[n-r-frac-plot]: https://github.com/ben-zen/n-r-frac/blob/dev/include/plot.hh#L539
[demoivres]: https://en.wikipedia.org/wiki/De_Moivre's_formula
[libvecm]: https://github.com/ben-zen/libvecm
