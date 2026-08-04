// SPDX-FileCopyrightText: 2026 Ben Lewis
//
// SPDX-License-Identifier: MIT

#include "gtest/gtest.h"
#include "arena.hh"

// Initiazlize the cache before using it.
template<>
zen::cache_ptr<double>::cache zen::cache_ptr<double>::impl{};


TEST(ArenaTest, AllocatesSeveral) {
    EXPECT_EQ(std::pair(0, 0), zen::cache_ptr<double>::contents()) << "There should be 0 and 0.";

    // allocate one
    size_t length = 40;
    zen::cache_ptr<double> first = zen::cache_ptr<double>::allocate_array(length);
    // check that ::count() is { 1 , 0 }
    EXPECT_EQ(std::pair(1, 0), zen::cache_ptr<double>::contents()) << "There should be 1 and 0.";

    // in a new scope, allocate another one
    {
        zen::cache_ptr<double> second = zen::cache_ptr<double>::allocate_array(length);
        // check that ::count() is { 2 , 0 }
        EXPECT_EQ(std::pair(2, 0), zen::cache_ptr<double>::contents()) << "There should be 2 and 0.";
    }

    EXPECT_EQ(std::pair(1, 1), zen::cache_ptr<double>::contents()) << "There should be 1 and 1.";

    {
        zen::cache_ptr<double> third = zen::cache_ptr<double>::allocate_array(length);

        EXPECT_EQ(std::pair(2, 0), zen::cache_ptr<double>::contents()) << "There should be 2 and 0, no new allocation.";
    }

    EXPECT_EQ(std::pair(1, 1), zen::cache_ptr<double>::contents()) << "There should be 1 and 1.";
}
