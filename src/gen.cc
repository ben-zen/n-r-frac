#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <execution>
#include <functional>
#include <format>
#include <limits>
#include <iostream>
#include <map>
#include <numeric>
#include <ranges>
#include <span>
#include <sstream>
#include <type_traits>
#include <vector>

#if defined(__riscv)
#include <riscv_vector.h>
#endif

template<typename Num>
class plot {
    Num m_real_min;
    Num m_real_max;
    Num m_imag_min;
    Num m_imag_max;
    size_t m_real_resolution;
    size_t m_imag_resolution;

    std::vector<Num> m_real{};
    std::vector<Num> m_imag{};

public:
    plot(Num real_min, Num real_max, Num imag_min, Num imag_max, size_t real_res, size_t imag_res) :
        m_real_min(real_min),
        m_real_max(real_max),
        m_imag_min(imag_min),
        m_imag_max(imag_max),
        m_real_resolution(real_res),
        m_imag_resolution(imag_res) {
        m_real.reserve(m_real_resolution * m_imag_resolution);
        m_imag.reserve(m_real_resolution * m_imag_resolution);
    };

    plot(plot<Num> const &other, std::vector<Num> &&real, std::vector<Num> &&imag) :
        m_real_min(other.m_real_min),
        m_real_max(other.m_real_max),
        m_imag_min(other.m_imag_min),
        m_imag_max(other.m_imag_max),
        m_real_resolution(other.m_real_resolution),
        m_imag_resolution(other.m_imag_resolution),
        m_real(std::move(real)),
        m_imag(std::move(imag)) {
        }

    void initialize() {
        Num r_step = (m_real_max - m_real_min) / (Num)m_real_resolution;
        Num j_step = (m_imag_max - m_imag_min) / (Num)m_imag_resolution;
        for (size_t r = 0; r < m_real_resolution; r++) {
            for (size_t j = 0; j < m_imag_resolution; j++) {
                m_real.push_back(m_real_min + r * r_step);
                m_imag.push_back(m_imag_min + j * j_step);
            }
        }
    }

    constexpr size_t pixels() const { return m_real_resolution * m_imag_resolution; }

    plot<Num> operator*(plot<double> const &rhs) {
        auto &&lhr = m_real;
        auto &&lhi = m_imag;
        auto &&rhr = rhs.m_real;
        auto &&rhi = rhs.m_imag;

        // Mezzanine Left `ml` & Mezzanine Right `mr` represent two intermediary steps
        std::vector<Num> mlr;
        std::vector<Num> mli;

        std::vector<Num> mrr;
        std::vector<Num> mri;

        mlr.reserve(pixels());
        mli.reserve(pixels());
        mrr.reserve(pixels());
        mri.reserve(pixels());

        // First: mlr = lhr * rhr
        for (auto &&[a, c] : std::views::zip(lhr, rhr)) {
            mlr.push_back(a * c);
        }

        // Now: mli = lhr * rhi
        for (auto &&[a, d] : std::views::zip(lhr, rhi)) {
            mli.push_back(a * d);
        }

        // This is actually positive multiplication for the imaginaries, you'll see why.
        for (auto &&[b, d] : std::views::zip(lhi, rhi)) {
            mrr.push_back(b * d);
        }

        // And lastly, the second real term times the first imaginary term.
        for (auto &&[b, c] : std::views::zip(lhi, rhr)) {
            mri.push_back(b * c);
        }

        // Now assemble the real and imaginary result terms!

        std::vector<Num> res_r;
        std::vector<Num> res_i;
        res_r.reserve(pixels());
        res_i.reserve(pixels());

        for (auto &&[l, r] : std::views::zip(mlr, mrr)) {
            res_r.push_back(l - r);
        }

        for (auto &&[l, r] : std::views::zip(mli, mri)) {
            res_i.push_back(l + r);
        }

        return plot<Num>(*this, std::move(res_r), std::move(res_i));
    }

    friend plot<Num> operator*(const std::complex<Num> &lhs, plot<Num> const &rhs) {
        // This is just a simpler version of the plot * plot case.
        auto &a = lhs.real();
        auto &b = lhs.imag();

        auto &rhr = rhs.m_real;
        auto &rhi = rhs.m_imag;

        std::vector<Num> mlr;
        std::vector<Num> mli;

        std::vector<Num> mrr;
        std::vector<Num> mri;

        mlr.reserve(rhs.pixels());
        mli.reserve(rhs.pixels());
        mrr.reserve(rhs.pixels());
        mri.reserve(rhs.pixels());

        for (auto &c : rhr) {
            mlr.push_back(a * c);
        }

        for (auto &d : rhi) {
            mli.push_back(a * d);
        }

        for (auto &d : rhi) {
            mrr.push_back(b * d);
        }

        for (auto &c : rhr) {
            mri.push_back(b * c);
        }

        std::vector<Num> res_r;
        std::vector<Num> res_i;

        res_r.reserve(rhs.pixels());
        res_i.reserve(rhs.pixels());

        for (auto &[l, r] : std::views::zip(mlr, mrr)) {
            res_r.push_back(l - r);
        }

        for (auto &[out, l, r] : std::views::zip(res_i, mli, mri)) {
            res_i.push_back(l + r);
        }

        return plot<Num>(rhs, std::move(res_r), std::move(res_i));
    }
};


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

    std::complex<double> eval(std::complex<double> const &point) const {
        std::vector<std::complex<double>> terms;
        terms.reserve(m_coefficients.size());

        for (auto const &[coefficient, power] : std::views::zip(m_coefficients, m_powers)) {
            terms.push_back((std::norm(coefficient) > 0) ? coefficient * ((power > 0) ? std::pow(point, power) : 1) : 0);
        }

        return std::accumulate(terms.begin(), terms.end(), std::complex<double>{0.0, 0.0});
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
std::vector<std::complex<double>> step(std::vector<std::complex<double>> &inputs, polynomial_function &f) {

    std::vector<std::complex<double>> evaluated;
    evaluated.reserve(inputs.size());

    //std::transform(std::execution::par_unseq, inputs.cbegin(), inputs.cend(), evaluated.begin(), [f](std::complex<double> const &v) -> std::complex<double> { return f.eval(v); });
    std::for_each(inputs.cbegin(), inputs.cend(), [&evaluated, f](std::complex<double> const &v) { evaluated.push_back(f.eval(v)); });
    // std::cout << "f(z_n): " << f.to_string() << std::endl;
    // print_values(evaluated, 10);

    auto f_deriv = f.derivative();
    std::vector<std::complex<double>> deriv_evaluated;
    deriv_evaluated.reserve(inputs.size());
    // std::transform(std::execution::par_unseq, inputs.cbegin(), inputs.cend(), deriv_evaluated.begin(), [f_deriv](std::complex<double> const &v) -> std::complex<double> { return f_deriv.eval(v); });
    std::for_each(inputs.cbegin(), inputs.cend(), [&deriv_evaluated, f_deriv](std::complex<double> const &v) { deriv_evaluated.push_back(f_deriv.eval(v)); });
    // std::cout << "f'(z_n): " << f_deriv.to_string() << std::endl;
    // print_values(deriv_evaluated, 10);

    std::vector<std::complex<double>> finals;
    finals.reserve(inputs.size());

    for (auto point: std::views::zip(inputs, evaluated, deriv_evaluated)) {
        finals.push_back( std::get<0>(point) - ((std::abs(std::get<2>(point)) > 1e-14) ? std::get<1>(point) / std::get<2>(point) : 0));
    }
    // std::cout << "z_(n+1) = z_n - f(z_n)/f'(z_n) :" << std::endl;
    // print_values(finals, 10);

    return finals;
}

std::vector<std::complex<double>> find_roots(std::vector<std::complex<double>> &inputs, size_t order) {
    struct comp {
        bool operator()(const std::complex<double> &lhs, const std::complex<double> &rhs) const {
            return std::abs(lhs - rhs) < 1e-14;
        }
    };
    std::vector<std::pair<std::complex<double>, size_t>> possible_roots;
    std::for_each(inputs.begin(), inputs.end(), [&possible_roots](auto &&c){
        auto r = std::find_if(possible_roots.begin(), possible_roots.end(), [c](auto &&r){
            return std::abs(r.first - c) < 1e-14;
        });
        if (r != possible_roots.end()) {
            r->second = r->second + 1;
        } else {
            possible_roots.emplace_back(c, 1);
        }
    });

    std::sort(possible_roots.begin(), possible_roots.end(), [](auto &lhs, auto &rhs){
        return lhs.second > rhs.second;
    });

    return std::vector<std::complex<double>>(std::from_range, std::ranges::views::take(possible_roots, order) | std::views::transform([](auto const p) -> std::complex<double> { return p.first; }));
}

// Plan to eventually capture convergence as a factor
std::vector<std::complex<double>> compute_fractal(std::complex<double> const &lower_left, std::complex<double> const &upper_right, size_t horiz_px, size_t vert_px, polynomial_function &func) {

    auto width = upper_right.real() - lower_left.real();
    auto height = upper_right.imag() - lower_left.imag();

    auto h_step = width / (double)horiz_px;
    auto v_step = height / (double)vert_px;

    // Prepare the input vector
    std::vector<std::complex<double>> values;

    for (size_t v = 0; v < vert_px; v++) {
        for (size_t h = 0; h < horiz_px; h++) {
            values.push_back({lower_left.real() + h * h_step, upper_right.imag() - v * v_step});
        }
    }

    plot<double> vals(lower_left.real(), upper_right.real(), lower_left.imag(), upper_right.imag(), horiz_px, vert_px);
    vals.initialize();

    plot<double>vals_two(lower_left.real(), upper_right.real(), lower_left.imag(), upper_right.imag(), horiz_px, vert_px);
    vals_two.initialize();

    for (int i = 0; i < 30; i++) {
        auto vals_three = vals * vals_two;

        std::cout << std::format("multiplication iteration {}, pixels: {}", i, vals_three.pixels()) << std::endl;
    }


    // There's improvements to be made here around finding each step's convergence.
    for (int i = 0; i < 30; i++) {
        std::cout << "Iteration " << i << std::endl;
        auto next_values = step(values, func);

        values = std::move(next_values);
    }

    return values;
}

extern "C" {
    bool configured_for_ai_thread();
}

int main() {
    std::cout << std::format("Configured for AI thread: {}\n"
                             "Vector width: {} bits\n",
                             configured_for_ai_thread(),
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

    auto values = compute_fractal(lower_left, upper_right, horiz_px, vert_px, func);

    std::cout << std::endl << "Final result:" << std::endl;
    // print_values(values, horiz_px);
    auto roots = find_roots(values, func.order());

    std::cout << "roots: " << std::endl;
    std::for_each(roots.begin(), roots.end(), [](auto r){ std::cout << r << std::endl;});

    // Use each root's angle to determine its color.


}
