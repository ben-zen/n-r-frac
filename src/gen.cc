// SPDX-FileCopyrightText: 2026 Ben Lewis
//
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <format>
#include <iostream>
#include <ranges>
#include <sstream>
#include <vector>

#if defined(__riscv)
#include <riscv_vector.h>
#endif

#include "plot.hh"

void print_values(std::vector<std::complex<double>> &values, size_t h_px) {
    std::ranges::for_each(values | std::views::chunk(h_px), [](auto pixels) {
        for (auto px : pixels) {
            std::cout << px;
        }
        std::cout << std::endl;
    });
}

class polynomial_function {
private:
    std::vector<std::complex<double>> m_coefficients;
    std::vector<int> m_powers;

public:
    polynomial_function(std::vector<std::complex<double>> &&coefficients, std::vector<int> &&powers) : m_coefficients(coefficients), m_powers(powers) {
    }

    std::string to_string() const {
        std::stringstream output;
        auto order = std::views::iota(0);

        for (auto elem : std::views::zip(m_coefficients, order)) {
            if (std::get<0>(elem) == std::complex<double>{}) {
                continue;
            }

            output << std::get<0>(elem);
            if (std::get<1>(elem) != 0) {
                output << "z^" << std::get<1>(elem);
            }
        }

        return output.str();
    }

    plot<double> eval(plot<double> const &points) const {
        plot<double> result(points, std::vector<double>(points.pixels(), 0.0), std::vector<double>(points.pixels(), 0.0));

        for (auto const &[coefficient, power] : std::views::zip(m_coefficients, m_powers)) {
            // c_n * (z ^ n). Since powers will always be integral, we have a few choices to make.
            auto points_exp = points.pow(power);

            auto term = coefficient * points_exp;

            result += term;
        }

        return result;
    }

    polynomial_function derivative() const {
        if (m_coefficients.size() == 0) {
            return polynomial_function{{}, {}};
        }

        std::vector<std::complex<double>> derivative_coefficients;
        std::vector<int> derivative_powers;
        for (auto const &[coefficient, power] : std::views::zip(m_coefficients, m_powers)) {
            auto d_c = coefficient * (double)power;
            auto d_p = power - 1;
            if (d_p >= 0) {
                derivative_coefficients.push_back(d_c);
                derivative_powers.push_back(d_p);
            }
        }

        return polynomial_function(std::move(derivative_coefficients), std::move(derivative_powers));
    }

    size_t order() const {
        return m_powers.empty() ? 0 : *std::max_element(m_powers.begin(), m_powers.end());
    }
};

template<typename T, typename Char>
struct std::formatter<std::complex<T>, Char> {
    std::formatter<T, Char> num_format;
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext &ctx) {
        return num_format.parse();
    }

    template<class FmtContext>
    FmtContext::iterator format(std::complex<T> const &c, FmtContext &ctx) {
        auto out = ctx.out();
        auto real = c.real();
        auto imag = c.imag();

        if (real != 0 || imag != 0) {
            if (real != 0 && imag != 0) {
                out = std::format_to(out, "(");
            }

            if (real != 0) {
                out = num_format.format(real, ctx);
            }

            if (imag != 0) {
                if (real != 0) {
                    out = std::format_to(out, "+");
                }

                out = num_format.format(imag, ctx);
                out = std::format_to(out, "i");
            }

            if (real != 0 && imag != 0) {
                out = std::format_to(out, ")");
            }
        } else {
            out = num_format.format(T{}, ctx);
        }

        return out;
    }
};

// template<>
// struct std::formatter<polynomial_function, char> {
//     template<class FmtContext>
//     FmtContext::iterator format(polynomial_function f, FmtContext &ctx) const {
//
//     }
// }

//
// The Newton-Raphson method:
//
// z_(n+1) = z_n - (f(z_n))/(f'(z_n))
//

// Yes this implementation requires four times each frame's memory, shut up.
plot<double> step(plot<double> &inputs, polynomial_function &f) {


    auto f_eval = f.eval(inputs);

    auto f_deriv = f.derivative();
    auto f_deriv_eval = f_deriv.eval(inputs);

    auto offsets = f_eval / f_deriv_eval;

    return inputs - offsets;
}


// Plan to eventually capture convergence as a factor
plot<double> compute_fractal(std::complex<double> const &lower_left, std::complex<double> const &upper_right, size_t horiz_px, size_t vert_px, polynomial_function &func) {

    plot<double> values(lower_left.real(), upper_right.real(), lower_left.imag(), upper_right.imag(), horiz_px, vert_px);
    values.initialize();

    // There's improvements to be made here around finding each step's convergence.
    for (int i = 0; i < 30; i++) {
        std::cout << "Iteration " << i;
        const auto start = std::chrono::steady_clock::now();
        auto next_values = step(values, func);
        const auto stop = std::chrono::steady_clock::now();

        std::cout << " ... " << std::fixed << std::setprecision(9) << stop - start << std::endl;
        values = std::move(next_values);
    }

    return values;
}

extern "C" {
    bool configured_for_ai_thread();
}

int main() {
    std::cout
        << std::format("Configured for A100: {}\n",
#if not defined(DISABLE_A100)
                       configured_for_ai_thread()
#else
                       false
#endif

        )
        << std::format("Vector width: {} bits\n",
#if defined(__riscv)
                       __riscv_vlenb() * 8
#elif defined(__x86_64__)
#if defined(__AVX512F__)
                       512
#elif defined(__AVX2__)
                       256
#elif defined(__SSE2__)
                       128
#else
                        64
#endif // __AVX512F__, etc.
#else
#endif
                             );

    // Provide two window points: lower left, upper right
    std::complex<double> lower_left { -5, -3 };
    std::complex<double> upper_right { 5, 3 };

    size_t horiz_px = 1000;
    size_t vert_px = 600;


    polynomial_function func{{{-1.0, 0.0}, {1.0, 0.0}}, {0, 3}};
    //polynomial_function func{{{-16.0, 0.0}, {15.0, 0,0}, {1.0, 0.0}}, {0, 4, 8}};

    const auto start_compute = std::chrono::steady_clock::now();
    auto values = compute_fractal(lower_left, upper_right, horiz_px, vert_px, func);
    const auto end_compute = std::chrono::steady_clock::now();

    std::cout << "Computed fractal in " << end_compute - start_compute << std::endl;

    // std::cout << std::endl << "Final result:" << std::endl << values.to_string() << std::endl;

    const auto start_roots = std::chrono::steady_clock::now();
    auto evaluate_final = func.eval(values);
    auto roots = values.find_roots(evaluate_final, func.order());
    const auto end_roots = std::chrono::steady_clock::now();

    std::cout << "roots (compute time: " << end_roots - start_roots <<  "): " << std::endl;
    std::for_each(roots.begin(), roots.end(), [](auto r){ std::cout << std::format("{:.1f} {} {:.5f}i", r.real(), ((r.imag() >= 0) ? "+" : "-"), std::abs(r.imag())) << std::endl;});

    // Use each root's angle to determine its color.

}
