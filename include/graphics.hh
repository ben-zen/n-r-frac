// SPDX-FileCopyrightText: 2026 Ben Lewis
//
// SPDX-License-Identifier: MIT

#pragma once

#include <complex>
#include <format>
#include <vector>

template <typename S>
struct hsv {
    // Range: [0, 360)
    S hue;
    // Range: [0, 1]
    S saturation;
    // Range: [0, 1]
    S value;
};


inline
hsv<double> point_to_hsv(std::complex<double> const &point) {
    auto angle = std::arg(point);
    if (angle < 0) {
        angle += 2.0 * M_PI;
    }

    return hsv { (angle * (360.0 / (2.0 * M_PI))), 1.0, 1.0 };
}

struct rgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

template<typename Char>
struct std::formatter<rgb, Char> {
    std::formatter<Char> num_format;
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext &ctx) {
        return num_format.parse(ctx);
    }

    template<class FmtContext>
    FmtContext::iterator format(rgb const &pixel, FmtContext &ctx) const {
        return std::format_to(ctx.out(), "({}, {}, {})", pixel.red, pixel.green, pixel.blue);
    }
};

template<typename Num>
rgb hsv_to_rgb(hsv<Num> in) {
    // I'm pulling this from 'https://en.wikipedia.org/wiki/HSL_and_HSV#HSV_to_RGB
    auto chroma = in.value * in.saturation;
    auto hue_comp = in.hue / 60.0; // H' on Wikipedia
    auto hue_rem = (hue_comp / 2.0 - std::floor(hue_comp / 2.0)) * 2.0; // H' mod 2
    auto secondary = chroma * (1.0 - std::abs(hue_rem - 1.0));

    Num red_base = (Num) 0, green_base = (Num) 0, blue_base = (Num) 0;
    if (hue_comp >= 0 && hue_comp < 1) {
        red_base = chroma;
        green_base = secondary;
    } else if (hue_comp >= 1 && hue_comp < 2) {
        red_base = secondary;
        green_base = chroma;
    } else if (hue_comp >= 2 && hue_comp < 3) {
        green_base = chroma;
        blue_base = secondary;
    } else if (hue_comp >= 3 && hue_comp < 4) {
        green_base = secondary;
        blue_base = chroma;
    } else if (hue_comp >= 4 && hue_comp < 5) {
        red_base = secondary;
        blue_base = chroma;
    } else if (hue_comp >= 5 && hue_comp < 6) {
        red_base = chroma;
        blue_base = secondary;
    }

    auto baseline = in.value - chroma;
    auto red = (red_base + baseline) * 255.0;
    auto green = (green_base + baseline) * 255.0;
    auto blue = (blue_base + baseline) * 255.0;
    return rgb {
        static_cast<uint8_t>(red),
        static_cast<uint8_t>(green),
        static_cast<uint8_t>(blue)
    };
}

void write_ppm(std::string const &filename, size_t h_px, size_t v_px, uint8_t max_value, std::vector<rgb> pixels);
