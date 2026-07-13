So why another simple Newton-Rapheson fractal generator?

I built one of these years and years ago, as an initial proof of concept to show that I
knew what I was doing, in one sense or another. Now, though, I wanted to see what I could
do with RVV instructions, and I had a feeling I could eventually get this to be a better
looping codebase.

Plus, maybe it'll get me to figure out adding features to Rust.

So, first off. The unoptimized version, running on a K3 AI core: kinda tragic. It pegs
a single core at 100% for .... well, a very long time. Doing math that this core is supposed
to be brilliant at. (It is, my code's just bad.)

## Non-parallelized code times
| Binary    | CPU core  |   `time` output
--------------------------------------------------------------------------------
| a.unopt   | AI        | real    1m40.149s, user    1m39.823s, sys     0m0.232s
| a.opt     | AI        | real    0m37.280s, user    0m36.937s, sys     0m0.304s
| a.unopt   | CPU       | real    0m50.415s, user    0m50.269s, sys     0m0.105s
| a.opt     | CPU       | real    0m18.109s, user    0m18.000s, sys     0m0.101s

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
