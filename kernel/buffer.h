#pragma once

#include <stdint.h>
#include "heap.h"
#include "debug.h"

// A buffer of a given size, intended to be wrapped in smart pointers

namespace impl::ext2 {

template <typename T>
struct Buffer {
    const size_t size;
    T* const data;

    Buffer(size_t size) : size(size), data((size == 0) ? nullptr : new T[size]()) {
        ASSERT(data != nullptr);
    }

    ~Buffer() {
        if (data != nullptr) {
            delete[] data;
        }
    }

};

}