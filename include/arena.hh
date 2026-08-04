// SPDX-FileCopyrightText: 2026 Ben Lewis
//
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <span>

namespace zen {
/* I'm taking the tactic of stashing `shared_ptr`s of arrays,
 * then re-using them when the initial computation's done. There
 * should be an upper limit on how many I need at once, really.
 *
 * This is currently functions, but will eventually be its own
 * wrapper class. I'm calling it `allocator` at the moment but will
 * probably change it to `cache_ptr` or something.
 */

template<typename Num>
class cache_ptr {
    struct cache {
        std::map<std::shared_ptr<Num>, size_t> loose_resources;
        std::map<std::shared_ptr<Num>, size_t> assigned_resources;
        std::mutex resource_mutex;
    } static impl;

    std::shared_ptr<Num> m_ptr;
    size_t m_extent;

    cache_ptr(std::shared_ptr<Num> &&ptr, size_t extent) : m_ptr(std::move(ptr)), m_extent(extent) {}

public:
    static
    cache_ptr<Num>
    allocate_array(size_t item_count) {
        std::shared_ptr<Num> resource;
        std::unique_lock lock(impl.resource_mutex);
        // if loose_resources has anything, find the first open buffer of the right size.
        auto available = std::find_if(impl.loose_resources.begin(), impl.loose_resources.end(), [&item_count](auto &r) -> bool {
            return r.second == item_count;
        });

        if (available != impl.loose_resources.end()) {
            resource = available->first;
            impl.loose_resources.erase(resource);
        } else {
            resource.reset(new Num[item_count]);
        }

        impl.assigned_resources.emplace(resource, item_count);
        return cache_ptr{std::move(resource), item_count};
    }

    // Get the size of each block of resources, allocated and loose.
    static
    std::pair<size_t, size_t>
    contents() {
        std::unique_lock lock(impl.resource_mutex);
        return {impl.assigned_resources.size(), impl.loose_resources.size()};
    }

    // Releasing takes ownership of it. The caller should use `std::move()` to
    // relocate the resource.

    ~cache_ptr() {
        std::unique_lock lock(impl.resource_mutex);
        if (impl.assigned_resources.contains(m_ptr)) {
            size_t item_count = impl.assigned_resources[m_ptr];
            impl.assigned_resources.erase(m_ptr);

            impl.loose_resources.emplace(m_ptr, item_count);
        }
    }

    // This is ... sketchy at the moment. For my purposes it should be fine, though.
    operator std::span<Num>() noexcept {
        return std::span<Num>(m_ptr.get(), m_extent);
    }
};

};
