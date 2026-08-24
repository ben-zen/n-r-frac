// SPDX-FileCopyrightText: 2026 Ben Lewis
//
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <format>
#include <fstream>
#include <vector>

#include "graphics.hh"

void write_ppm(std::string const &filename, size_t h_px, size_t v_px, uint8_t max_value, std::vector<rgb> pixels) {
    std::fstream out{filename, out.binary | out.trunc | out.out };
    out << std::format("P6\n{} {}\n{}\n", h_px, v_px, max_value);
    std::ranges::for_each(pixels, [&out](auto &&px) {
        out << px.red << px.green << px.blue;
    });
    out.flush();
    out.close();
}
