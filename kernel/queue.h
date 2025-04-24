#pragma once

#include "atomic.h"
#include "debug.h"

template <typename T, typename LockType>
class Queue {
    T * volatile first = nullptr;
    T * volatile last = nullptr;
    LockType lock;
public:
    Queue() : first(nullptr), last(nullptr), lock() {}
    Queue(const Queue&) = delete;

    void monitor_add() {
        monitor((uintptr_t)&last);
    }

    void monitor_remove() {
        monitor((uintptr_t)&first);
    }

    // not reliable
    bool isEmpty() {
        return first == nullptr;
    }

    void add_front(T* t) { 
        ASSERT(t != nullptr);
        LockGuard g{lock};
        t->next = first;
        first = t;
        if (last == nullptr) {
            last = t;
        }
    }

    void add(T* t) {
        ASSERT(t != nullptr);
        LockGuard g{lock};
        t->next = nullptr;
        if (first == nullptr) {
            first = t;
        } else {
            last->next = t;
        }
        last = t;
    }

    T* remove() {
        LockGuard g{lock};
        if (first == nullptr) {
            return nullptr;
        }
        auto it = first;
        first = it->next;
        if (first == nullptr) {
            last = nullptr;
        }
        return it;
    }

    T* remove_all() {
        LockGuard g{lock};
        auto it = first;
        first = nullptr;
        last = nullptr;
        return it;
    }
};

