#pragma once

#include "atomic.h"
#include "queue.h"
#include "heap.h"
#include "debug.h"
#include "smp.h"
#include "vme.h"
#include "vmm.h"
#include "processes.h"

constexpr size_t STACK_BYTES = 8 * 1024;
constexpr size_t STACK_WORDS = STACK_BYTES / sizeof(uintptr_t);  /* sizeof(word) == size(void*) */

namespace impl::threads {

    [[noreturn]]
    extern void thread_entry();

    // The bulk of the thread's state is saved on the stack, the SaveArea
    // is the part that gets saved outside of the stack
    struct SaveArea {
        uintptr_t sp = 0;            // the stack pointer
    };

    extern "C" void context_switch(SaveArea* from, SaveArea* to);
    extern void reap();

    // Abstract base class for thread control blocks
    struct TCB {
        TCB* next = nullptr;        // for adding to queues, invariant: a TCB can belong to at most one queue
        uint32_t at_jiffies = 0;    // when to wakeup, only used by sleeping threads
        SaveArea save_area{};       // the save area
        StrongPtr<VME<NoLock>> vme;
        uint32_t pd;
        StrongPtr<PCB> pcb;

        // subclasses can override to add cleanup code
        virtual ~TCB();

        // subclass can override to customize thread entry logic
        virtual void doit() = 0;

	// TSS esp0 for this thread
        virtual uint32_t interruptEsp() = 0;
	
    };

    // A TCB that wraps a callable object (function pointer, lambda, etc)
    template <typename Work>
    struct TCBWithWork: public TCB {
        uintptr_t stack[STACK_WORDS];   // The stack
        Work work;                      // captured entry logic

        TCBWithWork(Work const& work, uint32_t pd, StrongPtr<PCB> pcb, StrongPtr<VME<NoLock>> vme): work(work) {
            this->pd = pd;
            this->pcb = pcb;
            this->vme = vme;

            auto stack_index = STACK_WORDS-1;
            auto push = [&stack_index, this](uintptr_t v) {
                stack[stack_index --] = v;
            };

            // initialize the stack to mimic a thread that performed context_switch
            // this allows us to add new threads to the ready queue and switch to them
            // without treating them as a special case
            push((uintptr_t) thread_entry);     // return addr
            push(0);            // ebp
            push(0);            // edi
            push(0);            // esi
            push(0);            // ebx
            push((uint32_t)pd); // %cr3
            push(0);            // %cr2
            push(0x200);        // flags

            save_area.sp = (uintptr_t) &stack[stack_index+1];
        }

        // called when the thread is dispatached for the first time, performs the work
        void doit() override {
            work();
        }

        uint32_t interruptEsp() override {
            return uint32_t(&stack[STACK_WORDS - 1]);
        }

    };

    // Parts of the kernel have restrictions on their behavior:
    //     - critical sections protected by spinning (e.g. using a SpinLock) need to run in O(1)
    //     - critical sections protected by disabled interrupts need to run in O(1)
    //     - interrupts need to be disabled when an interrupt frame is active
    //
    // This has many implications. For example:
    //     - acquiring a spin lock has to be combined with disabling interrupts
    //     - interrupt handlers can't contend for spin locks
    //     - interrupt handlers can't enable interrupts but can switch (in O(1)) to threads
    //       that run with interrupts enabled
    //
    // The choice I made is to create a dedicated (per-core), non-preemptable helper thread
    // that does off-level interrupt processing for that core.
    //    - being dedicated implies that interrupt handlers can find it without contending for locks
    //    - having a dedicated stack means that it can run with interrupts enabled
    //    - being non-preemptable protects against re-entry issues during blocking
    //
    // The model is to have interrupt handlers do quick O(1) work then delegate the rest
    // to the helper thread by sending them a request (sub-class of the Request class), usualy
    // wrapping a lambda.
    //

    // base class for helper requests, a typical request is a subclass that lives on the caller's
    // stack 
    struct Request {
        virtual void doit() = 0;
    };

    // A request that wraps a callable object (function pointer, lambda, ...)
    template <typename Work>
    struct RequestWithWork: public Request {
        Work work;

        RequestWithWork(Work work): work(work) {}
        void doit() override {
            work();
        }
    };

    // SleepQueue, a linked list of TCBs representing sleeping threads
    // ordered by wakeup time.
    // Having O(n) add implies that a sleep queue can't be protected by
    // a spin lock.
    // Needing to access it from helper threads means that it can't be
    // protected by a blocking lock.
    // The only choice we're left with is to make it per-core.
    class SleepQueue {
        impl::threads::TCB* first = nullptr;
    public:
        // Add a TCB to the list, O(n)
        void add(impl::threads::TCB* tcb, uint32_t after_seconds);
        // remove the next eligible TCB (the earliest one whose time has come, if any)
        // from the list.
        // O(1)
        impl::threads::TCB* remove();
    };

    // Forward declaration in order to avoid circular references
    template <typename T> class BlockingQueue;

    // The threading state of the kernel. Meant to be a singleton
    struct State {
        Request **help_requests = new Request*[kConfig.totalProcs]();    // An array of off-level requests, one per core
        SaveArea *helpers = new SaveArea[kConfig.totalProcs]();          // The saved state of the helpers, one per core
        TCB** active_thread = new TCB*[kConfig.totalProcs]();            // active threads, one per core
        SleepQueue *sleep_queues = new SleepQueue[kConfig.totalProcs](); // sleep queues (sorted by wakeup time), one per core
        Queue<TCB, SpinLock> ready_queue{};   // the ready queue, one per system
        BlockingQueue<TCB>* reaper_queue;  // the reaper queue, one per system

        State();

        bool in_helper_thread() {
            return current() == nullptr;
        }

        bool preemption_is_disabled() {
            return Interrupts::isDisabled() || in_helper_thread();
        }

        // O(1)
        TCB* current() {
            // Need to disable interrupts briefly in order to prevent preemption
            auto was = Interrupts::disable();
            auto tcb = active_thread[SMP::me()];  // O(1)
            Interrupts::restore(was);
            return tcb;
        }

        // O(1)
        template <typename Work>
        void block(const char* from, Work w) {
            static_assert(sizeof(w) < 200);      // will be wrapped in a lambda that lives on the stack
                                                 // size will be checked at compile time

            auto was = Interrupts::disable();       // O(1)
            auto id = SMP::me();                    // O(1)
            auto tcb = active_thread[id];           // O(1)
            active_thread[id] = nullptr;            // O(1) prevents preemption
            Interrupts::restore(was);               // O(1)

            ASSERT(id < MAX_PROCS);
            ASSERT(tcb != nullptr);

            RequestWithWork request(w);             // O(1)
            help_requests[id] = &request;           // O(1)
                                                    // Safe iff the helper stops looking at it
                                                    // once it gives up control over the tcb
            context_switch(&tcb->save_area, &helpers[id]); // O(1)
        }
    };

    // The state singleton
    extern State state;

};

[[noreturn]]
extern void stop();
extern void yield();
extern void sleep(uint32_t seconds);

template <typename T>
void thread(T const& f) {
    using namespace impl::threads;

    reap();
    auto tcb = new TCBWithWork(f, VMM::new_page_directory(), StrongPtr<PCB>::make(1), StrongPtr<VME<NoLock>>::make(0x80000000, 0xF0000000));
    state.ready_queue.add(tcb);
}

template <typename T>
void thread(T const& f, uint32_t pd, StrongPtr<PCB> pcb, StrongPtr<VME<NoLock>> vme) {
    using namespace impl::threads;

    reap();
    auto tcb = new TCBWithWork(f, pd, pcb, vme);
    state.ready_queue.add(tcb);
}

