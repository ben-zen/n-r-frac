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
--------------------------------------------------------------------------------
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

| Binary    | CPU core  | `time` output
--------------------------------------------------------------------------------
| a.flags   | X100      | real    0m18.846s, user    0m18.700s, sys     0m0.137s
| a.flags   | A100      | real    0m37.762s, user    0m37.472sk sys     0m0.252s

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

## Going actually into DeMoivre's Theorem and all that

I like that idea, but I think there's also potential uses for mass-converting these vectors to polar coordinates for the polynomial aspect.

Conceptually, that'd look like this:

term order > 1? handle powers first:

Remember, `x * y` is actually `(a + bi) * (c + di) = (a * c - d * b) + (a * d + b * c)i`, or 6 arithmetic operations. Converting to polar coordinates is going to be its own mess: `r = sqrt(a ^ 2 + b ^ 2)` and `t = arccos(a/r)` (with a minor detail: if `b < 0`, it'll be `2pi - t`.)

I think the best outcome here is that I'll switch to do DeMoivre's theorem for powers 3 & above, but for lower powers... just do the basic arithmetic.

Then you multiply the value by its coefficient, and off you go.

