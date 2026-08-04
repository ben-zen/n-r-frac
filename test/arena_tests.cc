// SPDX-FileCopyrightText: 2026 Ben Lewis
//
// SPDX-License-Identifier: MIT

#include "gtest/gtest.h"
#include "arena.hh"

template<>
std::map<std::shared_ptr<double>, size_t> zen::allocator<double>::loose_resources{};

template<>
std::map<std::shared_ptr<double>, size_t> zen::allocator<double>::assigned_resources{};

template<>
std::mutex zen::allocator<double>::resource_mutex{};

TEST(ArenaTest, AllocatesSeveral) {
    EXPECT_EQ(std::pair(0, 0), zen::allocator<double>::contents()) << "There should be 0 and 0.";

    // allocate one
    size_t length = 40;
    std::shared_ptr<double> first = zen::allocator<double>::allocate_array(length);
    // check that ::count() is { 1 , 0 }
    EXPECT_EQ(std::pair(1, 0), zen::allocator<double>::contents()) << "There should be 1 and 0.";

    // in a new scope, allocate another one
    {
        std::shared_ptr<double> second = zen::allocator<double>::allocate_array(length);
        // check that ::count() is { 2 , 0 }
        EXPECT_EQ(std::pair(2, 0), zen::allocator<double>::contents()) << "There should be 2 and 0.";

        zen::allocator<double>::release_array(std::move(second)); // this becomes the destructor
    }

    EXPECT_EQ(std::pair(1, 1), zen::allocator<double>::contents()) << "There should be 1 and 1.";
}
