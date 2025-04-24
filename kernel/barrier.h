#pragma once

#include <stdint.h>
#include "debug.h"
#include "semaphore.h"

class Barrier {
    Atomic<int32_t> n;
    Semaphore ready{0};
public:
    Barrier(int32_t const n): n(n) {
    }
    Barrier(Barrier const&) = delete;

    void sync() {
        if (n.add_fetch(-1) == 0) {
            ready.up();
        } else {
            ready.down();
            ready.up();
        }
        
    }
        
};

