#pragma once

#include "atomic.h"
#include "queue.h"
#include "threads.h"

class Semaphore {
    SpinLock spin{};
    uint64_t n;
    Queue<impl::threads::TCB, NoLock> waiting{};
public:

    Semaphore(uint32_t n): n(n) {

    }

    void down() {
        using namespace impl::threads;

        spin.lock();
        if (n > 0) {            // O(1)
            n = n - 1;          // O(1)
            spin.unlock();    // O(1)
            return;
        }

        // sempahore is 0, release the lock then check again in the helper
        // this way we:
        //    - spend less time holding the lock
        //    - don't have to mess with interrupt state
        spin.unlock();
        
        auto tcb = state.current();       // O(1)
        ASSERT(tcb != nullptr);           // O(1)

        state.block("down", [this, tcb] { // O(1)
            // running in the helper thread with:
            //     - interrupts enabled
            //     - preemption disabled

            spin.lock();
            if (n > 0) {             // O(1)
                n = n - 1;           // O(1)
                spin.unlock();
                state.ready_queue.add(tcb);
            } else {
                waiting.add(tcb);
                spin.unlock();
            }
        });
    }

    void up() {
        using namespace impl::threads;
        spin.lock();
        auto wakeup = waiting.remove();     // O(1)
        if (wakeup == nullptr) {            // O(1)
            n = n + 1;                      // O(1)
        }
        spin.unlock();
        if (wakeup != nullptr) {
            state.ready_queue.add(wakeup);
        }
    }
};
