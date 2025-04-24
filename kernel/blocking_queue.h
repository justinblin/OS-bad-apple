#pragma once

#include "debug.h"
#include "semaphore.h"
#include "queue.h"

namespace impl::threads {

template <typename T>
class BlockingQueue {
    Queue<T, SpinLock> data{};
    Semaphore n{0};
public:
    // spins
    // doesn't block
    void add(T* p) {
        ASSERT(p != nullptr);
        data.add(p);       // spins
        n.up();            // spins
    }


    // spins
    // blocks
    // never returns nullptr
    T* remove() {
        T* p = nullptr;
        while (p == nullptr) {
            n.down();          // spins, blocks
            p = data.remove(); // spins
        }
        return p;
    }

    T* remove_all() {
        return data.remove_all();
    }

};

}
