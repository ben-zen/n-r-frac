// SPDX-FileCopyrightText: 2026 Ben Lewis
//
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cmath>
#include <format>
#include <iostream>
#include <numbers>
#include <ranges>
#include <span>
#include <sstream>
#include <utility>
#include <vector>

#if defined(__riscv)
#include <riscv_vector.h>
#endif

# if defined(__riscv)
template<typename Num>
inline
void
riscv_vec_add(std::vector<Num> &result, std::vector<Num> const &lhs, std::vector<Num> const &rhs);

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

template<typename Num>
inline
void
riscv_vec_sub(std::vector<Num> &result, std::vector<Num> const &lhs, std::vector<Num> const &rhs);

template<>
inline
void
riscv_vec_sub<double>(std::vector<double> &result, std::vector<double> const &lhs, std::vector<double> const &rhs) {

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
        vfloat64m8_t vec_sum = __riscv_vfsub_vv_f64m8(vec_lhs, vec_rhs, vl);

        // Store the results, figure out how to do this with reals
        __riscv_vse64_v_f64m8(dr_range.data(), vec_sum, vl);

        lr_start = lr_end;
        rr_start = rr_end;
        dr_start = dr_end;
    } while (lr_start != lhs.cend());
}

template<typename Num>
inline
void
riscv_vec_mul(std::vector<Num> &result, std::vector<Num> const &lhs, std::vector<Num> const &rhs);

template<>
inline
void
riscv_vec_mul<double>(std::vector<double> &result, std::vector<double> const &lhs, std::vector<double> const &rhs) {

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
        vfloat64m8_t vec_product = __riscv_vfmul_vv_f64m8(vec_lhs, vec_rhs, vl);

        // Store the results, figure out how to do this with reals
        __riscv_vse64_v_f64m8(dr_range.data(), vec_product, vl);

        lr_start = lr_end;
        rr_start = rr_end;
        dr_start = dr_end;
    } while (lr_start != lhs.cend());
}

template<typename Num>
inline
void
riscv_vec_mul(std::vector<Num> &result, Num const &lhs, std::vector<Num> const &rhs);

template<>
inline
void
riscv_vec_mul<double>(std::vector<double> &result, double const &lhs, std::vector<double> const &rhs) {

    auto rr_start = rhs.cbegin();
    auto dr_start = result.begin();
    // I'll start with getting the length of the vectors being added:
    auto rrem = lhs.size();

    do {
        auto vl = __riscv_vsetvl_e64m8(rrem);
        rrem -= vl;
        // Get the next set of left & right operands, and destination values.
        auto rr_end = rr_start + vl;
        auto dr_end = dr_start + vl;
        auto rr_range = std::span(rr_start, vl);
        auto dr_range = std::span(dr_start, vl);

        vfloat64m8_t vec_rhs = __riscv_vle64_v_f64m8(rr_range.data(), vl);
        vfloat64m8_t vec_product = __riscv_vfmul_vf_f64m8(vec_rhs, lhs, vl);

        // Store the results, figure out how to do this with reals
        __riscv_vse64_v_f64m8(dr_range.data(), vec_product, vl);

        rr_start = rr_end;
        dr_start = dr_end;
    } while (rr_start != rhs.cend());
}

template<typename Num>
inline
void
riscv_vec_div(std::vector<Num> &result, std::vector<Num> const &lhs, std::vector<Num> const &rhs);

template<>
inline
void
riscv_vec_div<double>(std::vector<double> &result, std::vector<double> const &lhs, std::vector<double> const &rhs) {

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
        vfloat64m8_t vec_quotient = __riscv_vfdiv_vv_f64m8(vec_lhs, vec_rhs, vl);

        // Store the results, figure out how to do this with reals
        __riscv_vse64_v_f64m8(dr_range.data(), vec_quotient, vl);

        lr_start = lr_end;
        rr_start = rr_end;
        dr_start = dr_end;
    } while (lr_start != lhs.cend());
}
#endif

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

    std::tuple<std::vector<Num>, std::vector<Num>> to_polar() const {
        std::vector<Num> radius;
        std::vector<Num> theta;
        radius.reserve(pixels());
        theta.reserve(pixels());

        // Get the radius first
        std::vector<Num> x_squared;
        std::vector<Num> y_squared;
        std::vector<Num> r_squared;
        x_squared.reserve(pixels());
        y_squared.reserve(pixels());
        r_squared.reserve(pixels());

        std::for_each(m_real.begin(), m_real.end(), [&x_squared](auto &&x) { x_squared.push_back(x * x); });
        std::for_each(m_imag.begin(), m_imag.end(), [&y_squared](auto &&y) { y_squared.push_back(y * y); });
        for (auto &&[x_sq, y_sq] : std::views::zip(x_squared, y_squared)) {
            r_squared.push_back(x_sq + y_sq);
        }

        std::for_each(r_squared.begin(), r_squared.end(), [&radius](auto &&r_sq) { radius.push_back(sqrt(r_sq)); });

        // Now that the radius is in hand, we can get the angle.
        for (auto &&[x, y, r] : std::views::zip(m_real, m_imag, radius)) {
            auto x_ratio = x / r;
            auto acos_xr = acos(x_ratio);
            constexpr auto two_pi = 2.0 * std::numbers::pi;
            if (y >= 0) {
                theta.push_back(acos_xr);
            } else {
                theta.push_back(two_pi - acos_xr);
            }
        }

        return {std::move(radius), std::move(theta)};
    }

    plot<Num> from_polar(std::vector<Num> &radius, std::vector<Num> &theta) const {
        std::vector<Num> cosines;
        std::vector<Num> sines;
        cosines.reserve(pixels());
        sines.reserve(pixels());
        std::for_each(theta.begin(), theta.end(), [&cosines, &sines](auto &t){ cosines.push_back(cos(t)); sines.push_back(sin(t)); });

        std::vector<Num> real;
        std::vector<Num> imag;
        real.reserve(pixels());
        imag.reserve(pixels());
        for (auto &&[cosine, sine, rad] : std::views::zip(cosines, sines, radius)) {
            real.push_back(cosine * rad);
            imag.push_back(sine * rad);
        }

        return plot<Num>(*this, std::move(real), std::move(imag));
    }

    std::tuple<std::vector<Num>, std::vector<Num>> add_internal(plot<Num> const &rhs) const {
#if defined (__riscv)
        std::vector<Num> reals(pixels(), (Num)0);
        std::vector<Num> imags(pixels(), (Num)0);

        // Since I'm doing RISC-V intrinsics, this gets a little messy. I'll want separate template functions
        // for each width; I'm also going to see if going to floats works okay, since doubles are probably more
        // precision than needed.


        riscv_vec_add(reals, m_real, rhs.m_real);
        riscv_vec_add(imags, m_imag, rhs.m_imag);
#else // defined (__riscv)
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
#endif // defined (__riscv)

        return {std::move(reals), std::move(imags)};
    }

    plot<Num> pow_exp(uint power) const {
        auto [radius, angle] = to_polar();

        std::vector<Num> pow_radius;
        std::vector<Num> pow_angle;
        pow_radius.reserve(pixels());
        pow_angle.reserve(pixels());

        std::for_each(radius.begin(), radius.end(), [&pow_radius, &power](auto &&r) { pow_radius.push_back(std::pow(r, (Num)power)); });
        std::for_each(angle.begin(), angle.end(), [&pow_angle, &power](auto &&t) { pow_angle.push_back(t * (Num)power); });

        return from_polar(pow_radius, pow_angle);
    }

public:
    constexpr size_t pixels() const { return m_real_resolution * m_imag_resolution; }

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

    plot(plot<Num> &&other) :
        m_real(std::move(other.m_real)),
        m_imag(std::move(other.m_imag)),
        m_real_min(other.m_real_min),
        m_real_max(other.m_real_max),
        m_imag_min(other.m_imag_min),
        m_imag_max(other.m_imag_max),
        m_real_resolution(other.m_real_resolution),
        m_imag_resolution(other.m_imag_resolution) {
        }

    plot(plot<Num> const &other) :
        m_real(other.m_real),
        m_imag(other.m_imag),
        m_real_min(other.m_real_min),
        m_real_max(other.m_real_max),
        m_imag_min(other.m_imag_min),
        m_imag_max(other.m_imag_max),
        m_real_resolution(other.m_real_resolution),
        m_imag_resolution(other.m_imag_resolution) {
        }

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
        for (size_t j = 0; j < m_imag_resolution; j++) {
            for (size_t r = 0; r < m_real_resolution; r++) {
                m_real.push_back(m_real_min + r * r_step);
                m_imag.push_back(m_imag_max - j * j_step);
            }
        }
    }

    plot<Num>& operator=(plot<Num> &&rhs) {
        m_real = std::move(rhs.m_real);
        m_imag = std::move(rhs.m_imag);

        m_real_min = rhs.m_real_min;
        m_real_max = rhs.m_real_max;
        m_imag_min = rhs.m_imag_min;
        m_imag_max = rhs.m_imag_max;
        m_real_resolution = rhs.m_real_resolution;
        m_imag_resolution = rhs.m_imag_resolution;

        return *this;
    }

    plot<Num> operator+(plot<Num> const &rhs) const {
        auto [reals, imags] = add_internal(rhs);
        return plot<Num>(*this, std::move(reals), std::move(imags));
    }

    plot<Num> &operator+=(plot<Num> const &rhs) {
        auto [reals, imags] = add_internal(rhs);
        m_real = std::move(reals);
        m_imag = std::move(imags);

        return *this;
    }

    plot<Num> operator-(plot<Num> const &rhs) const {
        std::vector<Num> reals;
        std::vector<Num> imags;
        reals.reserve(pixels());
        imags.reserve(pixels());

        for (auto &&[left, right] : std::views::zip(m_real, rhs.m_real)) {
            reals.push_back(left - right);
        }

        for (auto &&[left, right] : std::views::zip(m_imag, rhs.m_imag)) {
            imags.push_back(left - right);
        }

        return plot<Num>(rhs, std::move(reals), std::move(imags));
    }

    plot<Num> operator*(plot<double> const &rhs) const {
        auto &&lhr = m_real;
        auto &&lhi = m_imag;
        auto &&rhr = rhs.m_real;
        auto &&rhi = rhs.m_imag;
#if defined (__riscv)
        std::vector<Num> mlr(pixels(), (Num)0.0);
        std::vector<Num> mli(pixels(), (Num)0.0);

        std::vector<Num> mrr(pixels(), (Num)0.0);
        std::vector<Num> mri(pixels(), (Num)0.0);

        riscv_vec_mul(mlr, lhr, rhr);
        riscv_vec_mul(mli, lhr, rhi);

        riscv_vec_mul(mrr, lhi, rhi);
        riscv_vec_mul(mri, lhi, rhr);

        std::vector<Num> res_r(pixels(), (Num)0.0);
        std::vector<Num> res_i(pixels(), (Num)0.0);
        riscv_vec_sub(res_r, mlr, mrr);
        riscv_vec_add(res_i, mli, mri);
#else // defined (__riscv)
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
#endif // defined (__riscv)

        return plot<Num>(*this, std::move(res_r), std::move(res_i));
    }

    friend plot<Num> operator*(std::complex<Num> const &lhs, plot<Num> const &rhs) {
        // This is just a simpler version of the plot * plot case.
        auto a = lhs.real();
        auto b = lhs.imag();

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

        for (auto &&[l, r] : std::views::zip(mlr, mrr)) {
            res_r.push_back(l - r);
        }

        for (auto &&[ l, r] : std::views::zip(mli, mri)) {
            res_i.push_back(l + r);
        }

        return plot<Num>(rhs, std::move(res_r), std::move(res_i));
    }

    plot<Num> operator/(plot<Num> const &rhs) const {
        // Division, for z_1 = a + bi, z_2 = c + di, resolves to:
        // (ac + bd)/(c^2 + d^2) + (bc - ad)i/(c^2 + d^2)
        // so we need three terms: (ac + bd), (bc - ad), and (c^2 + d^2)
        std::vector<Num> denoms;
        std::vector<Num> mlr;
        std::vector<Num> mli;
        std::vector<Num> mrr;
        std::vector<Num> mri;

        denoms.reserve(pixels());
        mlr.reserve(pixels());
        mli.reserve(pixels());
        mrr.reserve(pixels());
        mri.reserve(pixels());

        for (auto &&[real, imag] : std::views::zip(rhs.m_real, rhs.m_imag)) {
            denoms.push_back(real * real + imag * imag);
        }

        for (auto &&[lr, rr] : std::views::zip(m_real, rhs.m_real)) {
            mlr.push_back(lr * rr);
        }

        for (auto &&[li, rr] : std::views::zip(m_imag, rhs.m_real)) {
            mli.push_back(li * rr);
        }

        for (auto &&[li, ri] : std::views::zip(m_imag, rhs.m_imag)) {
            mrr.push_back(li * ri);
        }

        for (auto &&[lr, ri] : std::views::zip(m_real, rhs.m_imag)) {
            mri.push_back(lr * ri);
        }

        std::vector<Num> real_sum;
        std::vector<Num> imag_sum;
        real_sum.reserve(pixels());
        imag_sum.reserve(pixels());

        for (auto && [left, right] : std::views::zip(mlr, mrr)) {
            real_sum.push_back(left + right);
        }

        for (auto &&[left, right] : std::views::zip(mli, mri)) {
            imag_sum.push_back(left - right);
        }

        std::vector<Num> real;
        std::vector<Num> imag;
        real.reserve(pixels());
        imag.reserve(pixels());

        for (auto &&[num, denom] : std::views::zip(real_sum, denoms)) {
            real.push_back(num / denom);
        }

        for (auto &&[num, denom] : std::views::zip(imag_sum, denoms)) {
            imag.push_back(num / denom);
        }

        return plot<Num>(rhs, std::move(real), std::move(imag));
    }



    plot<Num> pow(uint power) const {
        switch (power) {
            case 0:
                return plot<Num>(*this, std::vector<Num>(pixels(), (Num)1), std::vector<Num>(pixels(), (Num)0));

            case 1:
                return *this;

            case 2:
                return *this * *this;

            default: // 3 or more... just go to polar.
            {
                return pow_exp(power);
            }
        }
    }

    std::vector<std::complex<Num>> find_roots(size_t order) {
        std::vector<std::pair<std::pair<Num, Num>, size_t>> possible_roots;
        for (auto &&[real, imag] : std::views::zip(m_real, m_imag)) {
            auto r = std::find_if(possible_roots.begin(), possible_roots.end(), [real, imag](auto &&r){
                return std::abs(r.first.first - real) < 1e-10 && std::abs(r.first.second - imag) < 1e-10;
            });
            if (r != possible_roots.end()) {
                r->second = r->second + 1;
            } else {
                possible_roots.emplace_back(std::pair{real, imag}, 1);
            }
        }

        std::sort(possible_roots.begin(), possible_roots.end(), [](auto &lhs, auto &rhs){
            return lhs.second > rhs.second;
        });

        // std::cout << std::format("{}", possible_roots) << std::endl;

        return std::vector<std::complex<Num>>(std::from_range, std::ranges::views::take(possible_roots, order) | std::views::transform([](auto const p) -> std::complex<Num> { return std::complex(p.first.first, p.first.second); }));
    }

    std::string to_string() const {
        auto rows = std::views::zip(m_real, m_imag) | std::views::chunk(m_real_resolution);
        return std::format("{}", rows);
    }
};

template<typename Num, typename Char>
struct std::formatter<plot<Num>, Char> {
    std::formatter<Num, Char> num_format;
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext &ctx) {
        return num_format.parse(ctx);
    }

    template<class FmtContext>
    FmtContext::iterator format(plot<Num> &plot, FmtContext &ctx) {
        std::ostringstream out;
        out << plot.to_string();
        return std::ranges::copy(std::move(out).str(), ctx.out()).out;
    }
};
