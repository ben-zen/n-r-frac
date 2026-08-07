# Pretty drawings and fast math

The SpaceMIT K3 is a really interesting new dev kit; it's a RISC-V dev board with an unusual big/little CPU design: 8 X100 compute cores at 2.4GHz, and 8 A100 vector cores running at 2GHz. The X100 cores fully support the RVA23 profile with 256-bit vector extensions, while the A100 cores support _most_ of the RVA23 profile (everything except the hypervisor instructions) but they have 1024-bit vector registers. That bank of enormous vector registers really spurred my interest, and I figured I could write a small toy application to see what they're capable of. As it happens, I realized I'd lost the sources to a project I'd done years and years ago, and so I set out to make a new Newton-Rapheson fractal generator.

Newton's method of approximation is a useful way to find the roots of polynomials that either can't be factored, or where it's not useful to factor a polynomial (if the roots don't have closed forms, for instance.) For a polynomial function, `f(z)`, the method is to choose a starting point, `z_0`, and apply the following step function repeatedly:

> f(z_{n+1}) = z_n - f(z_n) / f'(z_n)

Either this eventually converges on a root, or it diverges (in the Real numbers) ... and generally speaking, it will approach the "nearest" root readily, in the Reals. The same cannot be said when `z` is a complex number. Since multiplication and division both behave very differently on the complex plane, Newton's approximation generates much more interesting results. The fractals shown in this post are plots of what root the N-R algorithm arrives at starting from a given point, colorized by root. This happens to be a neat, easy way to begin exploring optimizing math, and it even produces pretty pictures at the end.

There's plenty of tools that already exist to generate fractals, but I wanted a toy that was easy to convert to vector instructions; pre-existing projects would get in the way, and potentially mask clear performance impacts. To really show this behavior, I started with a very naïve implementation, and then started hunting for optimizations on the way to vectorizing my math.

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
| CPU core  |   `time` output
| --------- | ------------------------------------------------------
| A100      | real    1m40.149s, user    1m39.823s, sys     0m0.232s
| X100      | real    0m50.415s, user    0m50.269s, sys     0m0.105s

| Optimized performance (-O2)
| ----------------------------------------
| CPU core  |   `time` output
| --------- | ------------------------------------------------------
| A100      | real    0m37.280s, user    0m36.937s, sys     0m0.304s
| X100      | real    0m18.109s, user    0m18.000s, sys     0m0.101s

So, that's kinda disappointing. On the other hand, there's lots of room to improve... and we'll see it soon. First, however, I paused my vectorizing to address the logistics of running code on specifically the A100 cores.

## An aside on initialization


