// SPDX-FileCopyrightText: 2026 Ben Lewis
//
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>

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
class allocator {
    static std::map<std::shared_ptr<Num>, size_t> loose_resources;
    static std::map<std::shared_ptr<Num>, size_t> assigned_resources;
    // allocator lock, best to put this in _before_ we go multi-threaded.
    static std::mutex resource_mutex;

public:
    static
    std::shared_ptr<Num>
    allocate_array(size_t item_count) {
        std::shared_ptr<Num> resource;
        std::unique_lock lock(resource_mutex);
        // if loose_resources has anything, find the first open buffer of the right size.
        auto available = std::find_if(loose_resources.begin(), loose_resources.end(), [&item_count](auto &r) -> bool {
            return r.second == item_count;
        });

        if (available != loose_resources.end()) {
            resource = available->first;
            loose_resources.erase(resource);
        } else {
            resource.reset(new Num[item_count]);
        }

        assigned_resources.emplace(resource, item_count);
        return resource;
    }

    // Releasing takes ownership of it. The caller should use `std::move()` to
    // relocate the resource.
    static
    void
    release_array(std::shared_ptr<Num> &&resource) {
        std::unique_lock lock(resource_mutex);
        if (assigned_resources.contains(resource)) {
            size_t item_count = assigned_resources[resource];
            assigned_resources.erase(resource);

            loose_resources.emplace(resource, item_count);
        }
    }

    // Get the size of each block of resources, allocated and loose.
    static
    std::pair<size_t, size_t>
    contents() {
        std::unique_lock lock(resource_mutex);
        return {assigned_resources.size(), loose_resources.size()};
    }
};

};
