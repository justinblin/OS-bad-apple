#pragma once

#include "machine.h"
#include "init.h"
#include "loop.h"

template <typename T>
class AtomicPtr {
    volatile T *ptr;
public:
    AtomicPtr() : ptr(nullptr) {}
    AtomicPtr(T *x) : ptr(x) {}
    AtomicPtr<T>& operator= (T v) {
        __atomic_store_n(ptr,v,__ATOMIC_SEQ_CST);
        return *this;
    }
    operator T () const {
        return __atomic_load_n(ptr,__ATOMIC_SEQ_CST);
    }
    T fetch_add(T inc) {
        return __atomic_fetch_add(ptr,inc,__ATOMIC_SEQ_CST);
    }
    T add_fetch(T inc) {
        return __atomic_add_fetch(ptr,inc,__ATOMIC_SEQ_CST);
    }
    void set(T inc) {
        return __atomic_store_n(ptr,inc,__ATOMIC_SEQ_CST);
    }
    T get(void) {
        return __atomic_load_n(ptr,__ATOMIC_SEQ_CST);
    }
    T exchange(T v) {
        T ret;
        __atomic_exchange(ptr,&v,&ret,__ATOMIC_SEQ_CST);
        return ret;
    }
};

template <typename T>
class Atomic {
    volatile T value;
public:
    Atomic(T x) : value(x) {}
    Atomic<T>& operator= (T v) {
        __atomic_store_n(&value,v,__ATOMIC_SEQ_CST);
        return *this;
    }
    operator T () const {
        return __atomic_load_n(&value,__ATOMIC_SEQ_CST);
    }
    T fetch_add(T inc) {
        return __atomic_fetch_add(&value,inc,__ATOMIC_SEQ_CST);
    }
    T add_fetch(T inc) {
        return __atomic_add_fetch(&value,inc,__ATOMIC_SEQ_CST);
    }
    void set(T inc) {
        return __atomic_store_n(&value,inc,__ATOMIC_SEQ_CST);
    }
    T get(void) {
        return __atomic_load_n(&value,__ATOMIC_SEQ_CST);
    }
    T exchange(T v) {
        T ret;
        __atomic_exchange(&value,&v,&ret,__ATOMIC_SEQ_CST);
        return ret;
    }
    void monitor_value() {
        monitor((uintptr_t)&value);
    }
};

template <>
class Atomic<uint64_t> {
public:
    Atomic() = delete;
    Atomic(uint64_t) = delete;
};

template <>
class Atomic<int64_t> {
public:
    Atomic() = delete;
    Atomic(int64_t) = delete;
};

template <typename T>
class LockGuard {
    T& it;
public:
    inline LockGuard(T& it): it(it) {
        it.lock();
    }
    inline ~LockGuard() {
        it.unlock();
    }
};

template <typename T>
class LockGuardP {
    T* it;
public:
    inline LockGuardP(T* it): it(it) {
        if (it) it->lock();
    }
    inline ~LockGuardP() {
        if (it) it->unlock();
    }
};

class NoLock {
public:
    inline void lock() {}
    inline void unlock() {}
};

extern void pause();

class Interrupts {
public:
    static bool isDisabled() {
        uint32_t oldFlags = getFlags();
        return (oldFlags & 0x200) == 0;
    }
        
    static bool disable() {
        bool wasDisabled = isDisabled();
        if (!wasDisabled)
            cli();
        return wasDisabled;
    }   
    
    static void restore(bool wasDisabled) {
        if (!wasDisabled) {
            sti();
        }
    }   
    
    template <typename Work>
    static inline void protect(Work work) {
        auto was = disable();
        work();
        restore(was);
    }
    
};


class SpinLock  {
    Atomic<bool> taken{false};
    Atomic<bool> was{false};
public:
    SpinLock() {}
    SpinLock(const SpinLock&) = delete;

    // for debugging, etc. Allows false positives
    bool isMine() {
        return taken.get();
    }

    void lock() {
        while (true) {
            taken.monitor_value();
            bool wasDisabled = Interrupts::disable();
            if (!taken.exchange(true)) {
                was.set(wasDisabled);
                return;
            }
            Interrupts::restore(wasDisabled);
            iAmStuckInALoop(true);
        }
    }

    void unlock() {
        auto wasDisabled = was.get();
        taken.set(false);
        Interrupts::restore(wasDisabled);
    }

    friend class Semaphore;
};

namespace impl::atomic {
    class ISL {
        Atomic<bool> taken{false};
    public:
        ISL() {}
        ISL(const ISL&) = delete;

        // for debugging, etc. Allows false positives
        bool isMine() {
            return taken.get();
        }

        bool lock() {
            while (true) {
                taken.monitor_value();
                bool wasDisabled = Interrupts::disable();
                if (!taken.exchange(true)) {
                    return wasDisabled;
                }
                Interrupts::restore(wasDisabled);
                iAmStuckInALoop(true);
            }
        }

        void unlock(bool was) {
            taken.set(false);
            Interrupts::restore(was);
        }
    };
}

