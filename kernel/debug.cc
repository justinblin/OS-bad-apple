#include "debug.h"
#include "libk.h"
#include "machine.h"
#include "smp.h"
#include "config.h"
#include "kernel.h"
#include "atomic.h"

OutputStream<char> *Debug::sink = 0;
bool Debug::debugAll = false;
bool Debug::shutdown_called = false;
Atomic<uint32_t> Debug::checks{0};

void Debug::init(OutputStream<char> *sink) {
    Debug::sink = sink;
}

static SpinLock lock{};

class NopStream: public OutputStream<char> {
public:
    void put(char v) {

    }
};

void Debug::vprintf(const char* fmt, va_list ap) {
    if (sink) {
        NopStream s{};
        K::vsnprintf(s, 1000, fmt,ap);
        lock.lock();
        K::vsnprintf(*sink,1000,fmt,ap);
        lock.unlock();
    }
}

void Debug::printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt,ap);
    va_end(ap);
}

static Atomic<bool> showed_checks { false };

void Debug::shutdown() {
    if (!showed_checks.exchange(true)) {
        if (checks.get() > 0) {
            printf("*** passed %d checks\n",checks.get());
        }
    }
    printf("shutdown\n",SMP::me());
    shutdown_called = true;
    while (true) {
        outb(0xf4,0x00);
	    asm volatile("pause");
    }
    //printf("| system shutdown\n");
    //while (true) asm volatile ("hlt");
}

[[noreturn]]
void Debug::vpanic(const char* fmt, va_list ap) {
    lock.unlock(); // things are going bad, force unlock
    vprintf(fmt,ap);
    printf("| processor %d halting\n",SMP::me());
    shutdown_called = true;
    while (true) {
        outb(0xf4,0x00);
 	    asm volatile("pause");
    }
    //printf("| system shutdown\n");
    //while (true) asm volatile ("hlt");
}

[[noreturn]]
void Debug::panic(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vpanic(fmt,ap);
    va_end(ap);
}

void Debug::vdebug(const char* fmt, va_list ap) {
    if (debugAll || flag) {
       printf("[%s] ",what);
       vprintf(fmt,ap);
       printf("\n");
    }
}

void Debug::debug(const char* fmt, ...) {
    if (debugAll || flag) {
        va_list ap;
        va_start(ap, fmt);
        vdebug(fmt,ap);
        va_end(ap);
    }
}

[[noreturn]]
void Debug::missing(const char* file, int line) {
    panic("*** Missing code at %s:%d\n",file,line);
}


