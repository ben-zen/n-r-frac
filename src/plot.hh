#pragma once

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <ranges>
#include <sstream>
#include <vector>

template<typename Num>
class polar_plot;

template<typename Num>
class plot {

    std::vector<Num> m_real{};
    std::vector<Num> m_imag{};

    Num m_real_min;
    Num m_real_max;
    Num m_imag_min;
    Num m_imag_max;
    size_t m_real_resolution;
    size_t m_imag_resolution;

    friend polar_plot<Num>;

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
        m_real(std::move(real)),
        m_imag(std::move(imag)),
        m_real_min(other.m_real_min),
        m_real_max(other.m_real_max),
        m_imag_min(other.m_imag_min),
        m_imag_max(other.m_imag_max),
        m_real_resolution(other.m_real_resolution),
        m_imag_resolution(other.m_imag_resolution) {
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

    plot<Num> operator+(plot<Num> const &rhs) const {
        std::vector<Num> reals;
        std::vector<Num> imags;
        reals.reserve(pixels());
        imags.reserve(pixels());

        for (auto &&[left, right] : std::views::zip(m_real, rhs.m_real)) {
            reals.push_back(left + right);
        }

        for (auto &&[left, right] : std::views::zip(m_imag, rhs.m_imag)) {
            imags.push_back(left + right);
        }

        return plot<Num>(*this, std::move(reals), std::move(imags));
    }

    plot<Num> operator*(plot<double> const &rhs) const {
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

    friend plot<Num> operator*(std::complex<Num> const &lhs, plot<Num> const &rhs) {
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

    std::string to_string() const {
        auto rows = std::views::zip(m_real, m_imag) | std::views::chunk(m_real_resolution);
        return std::format("{}", rows);
    }
};

template<typename Num>
class polar_plot {
    std::vector<Num> m_radius;
    std::vector<Num> m_theta; // I'm not entertaining mixed types here
    size_t m_real_resolution;
    size_t m_imag_resolution;
    Num m_real_min;
    Num m_real_max;
    Num m_imag_min;
    Num m_imag_max;

    // This should carry data to recover a plot, but is not intended to be a standalone object from a source plot.
    // polar_plot exists to simplify higher powers of complex numbers specifically.
    friend plot<Num>;

    static polar_plot<Num> from(plot<Num> const &cartesian) {
        auto x_res = cartesian.m_real_resolution;
        auto y_res = cartesian.m_imag_resolution;
        std::vector<Num> radius;
        std::vector<Num> theta;
        radius.reserve(cartesian.pixels());
        theta.reserve(cartesian.pixels());

        // Get the radius first
        std::vector<Num> x_squared;
        std::vector<Num> y_squared;
        std::vector<Num> r_squared;
        x_squared.reserve(cartesian.pixels());
        y_squared.reserve(cartesian.pixels());
        r_squared.reserve(cartesian.pixels());

        std::for_each(cartesian.m_real.begin(), cartesian.m_real.end(), [&x_squared](auto &&x) { x_squared.push_back(x * x); });
        std::for_each(cartesian.m_imag.begin(), cartesian.m_imag.end(), [&y_squared](auto &&y) { y_squared.push_back(y * y); });
        for (auto &&[x_sq, y_sq] : std::views::zip(x_squared, y_squared)) {
            r_squared.push_back(x_sq + y_sq);
        }

        std::for_each(r_squared.begin(), r_squared.end(), [&radius](auto &&r_sq) { radius.push_back(sqrt(r_sq)); });

        // Now that the radius is in hand, we can get the angle.
        for (auto &&[x, y, r] : std::views::zip(cartesian.m_real, cartesian.m_imag, radius)) {
            auto x_ratio = x / r;
            auto acos_xr = acos(x_ratio);
            constexpr auto two_pi = 2.0 * std::numbers::pi;
            if (y >= 0) {
                theta.push_back(acos_xr);
            } else {
                theta.push_back(two_pi - acos_xr);
            }
        }

        return polar_plot<Num>{std::move(radius), std::move(theta), x_res, y_res, cartesian.m_real_min, cartesian.m_real_max, cartesian.m_imag_min, cartesian.m_imag_max};
    }

    plot<Num> into() {
        std::vector<Num> cosines;
        std::vector<Num> sines;
        cosines.reserve(pixels());
        sines.reserve(pixels());
        std::for_each(m_theta.begin(), m_theta.end(), [&cosines, &sines](auto &t){ cosines.push_back(cos(t)); sines.push_back(sin(t)); });

        std::vector<Num> real;
        std::vector<Num> imag;
        real.reserve(pixels());
        imag.reserve(pixels());
        for (auto &&[cosine, sine, radius] : std::views::zip(cosines, sines, m_radius)) {
            real.push_back(cosine * radius);
            imag.push_back(sine * radius);
        }

        return plot<Num>{std::move(real), std::move(imag), m_real_min, m_real_max, m_imag_min, m_imag_max, m_real_resolution, m_imag_resolution};
    }

    constexpr size_t pixels() const { return m_real_resolution * m_imag_resolution; }

    polar_plot<Num> exp(uint power) const {
        // Here's the DeMoivre's Theorem fun!

        std::vector<Num> radius;
        std::vector<Num> theta;
        radius.reserve(pixels());
        theta.reserve(pixels());

        std::for_each(m_radius.begin(), m_radius.end(), [&radius, &power](auto &&r) { radius.push_back(pow(r, (Num)power)); });
        std::for_each(m_theta.begin(), m_theta.end(), [&theta, &power](auto &&t) { theta.push_back(t * (Num)power); });

        return polar_plot<Num>{std::move(radius), std::move(theta), m_real_resolution, m_imag_resolution, m_real_min, m_real_max, m_imag_min, m_imag_max};
    }

};

template<typename Char>
struct std::formatter<plot<double>, Char> {
    std::formatter<double, Char> num_format;
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext &ctx) {
        return num_format.parse(ctx);
    }

    template<class FmtContext>
    FmtContext::iterator format(plot<double> &plot, FmtContext &ctx) {
        std::ostringstream out;
        out << plot.to_string();
        return std::ranges::copy(std::move(out).str(), ctx.out()).out;
    }
};
