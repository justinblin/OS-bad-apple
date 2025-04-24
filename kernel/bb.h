#pragma once

#include "semaphore.h"

template <typename T>
class BB {
    T* data;
    uint32_t n;
    Semaphore n_empty;
    Semaphore n_full;
    SpinLock spin{};
    uint32_t front = 0;
    uint32_t back = 0;
public:
    BB(uint32_t n) : data{new T[n]()}, n{n}, n_empty{n}, n_full{0} {}
    ~BB() {
        delete[] data;
    }
    void put(T& v) {
        n_empty.down();
        spin.lock();
        data[back] = v;
        back = (back+1)%n;
        spin.unlock();
        n_full.up();
    }

    T get() {
        n_full.down();
        spin.lock();
        auto v = data[front];
        front = (front + 1) % n;
        spin.unlock();
        n_empty.up();
        return v;
    }
};