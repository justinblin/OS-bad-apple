#pragma once

#include "debug.h"
#include "semaphore.h"

class BlockingLock {
    Semaphore sem{1};
public:
    BlockingLock() {
    }
    BlockingLock(BlockingLock const&) = delete;
    inline void lock() {
        sem.down();
    }
    inline void unlock() {
        sem.up();
    }
};