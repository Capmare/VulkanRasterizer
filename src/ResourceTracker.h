//
// Created by capma on 7/21/2025.
//

#ifndef ALLOCATOR_H
#define ALLOCATOR_H
#include <iostream>
#include <unordered_map>

#include "vk_mem_alloc.h"


class ResourceTracker {

public:

    void TrackAllocation(VmaAllocation alloc, const std::string& name) {
        if (reinterpret_cast<uintptr_t>(alloc) < 0x1000) {
            printf("TrackAllocation: alloc appears invalid (%p)\n", alloc);
            return;
        }

        g_TrackedAllocs[alloc] = name;
    }

    void UntrackAllocation(VmaAllocation alloc) {
        g_TrackedAllocs.erase(alloc);
    }

    void PrintAllocations() {
        std::cout << "Current allocations: " <<  g_TrackedAllocs.size() << std::endl;
        for (auto& [alloc,name] : g_TrackedAllocs) {
            std::cout << name.c_str() << std::endl;
        }
    }

    void TrackImageView(VkImageView view, const std::string& name) {
        g_ImageViewTracker[view] = name;
    }

    void UntrackImageView(VkImageView view) {
        g_ImageViewTracker.erase(view);
    }

    void PrintImageViews() {
        std::cout << "Current ImageViews: " <<  g_ImageViewTracker.size() << std::endl;
        for (auto& [alloc,name] : g_ImageViewTracker) {
            std::cout << name.c_str() << std::endl;
        }
    }

private:
    std::unordered_map<void*, std::string> g_TrackedAllocs;
    std::unordered_map<VkImageView, std::string> g_ImageViewTracker;




};



#endif //ALLOCATOR_H
