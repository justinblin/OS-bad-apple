#pragma once

#include "debug.h"
#include "semaphore.h"

template <typename T>
class Promise {
    Semaphore sem{0};
    T value{};
public:
    void set(T const& v) {
        value = v;
        sem.up();
    }
    T get() {
        sem.down();
        sem.up();
        return value;
    }
};



