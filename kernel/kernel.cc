#include "debug.h"
#include "ide.h"
#include "ext2.h"
#include "elf.h"
#include "machine.h"
#include "libk.h"
#include "config.h"
#include "shared.h"

StrongPtr<Ext2> fs{};

uint32_t* load_args(uint32_t* userEsp, const char** argv) {
    uint32_t argc = 0;
    while(argv[argc] != nullptr) {
        argc++;
    }
    
    uint32_t length[argc];
    uint32_t sum = 0;
    for(uint32_t i = 0; i < argc; i++) {
        length[i] = K::strlen(argv[i]) + 1;
        sum += length[i];
    }

    uint32_t next = sizeof(char*) * (argc + 1 + 1); // account for argc, argv
    userEsp -= ((sum + next + 15) / 16) * 16;

    ((uint32_t*)userEsp)[0] = argc; // adds argc to stack
    ((uint32_t*)userEsp)[1] = (uint32_t)userEsp + 8; // makes argv point to the next slot
    for(uint32_t i = 0; i < argc; i++) {
        ((uint32_t*)userEsp)[1 + 1 + i] = (uint32_t)userEsp + next;
        uint32_t len = length[i];
        memcpy((void*)((char*)userEsp + next), argv[i], len);
        next += len;
    }

    return userEsp;
}

void kernelMain(void) {
    auto d = StrongPtr<Ide>::make(1,0);
    fs = StrongPtr<Ext2>::make(d);
    auto init = fs->find(fs->root, "/sbin/init");

    // Debug::printf("loading init\n");
    uint32_t e = ELF::load(init);
    // Debug::printf("entry %x\n",e);
    const uint32_t stack_size = 134217728;
    void* stack_start = VMM::naive_mmap(stack_size, false, {}, 0, false);
    uint32_t userEsp = ((uint32_t)stack_start + stack_size - 16);
    
    const char* argv[] = { "init", nullptr };
    userEsp = (uint32_t)load_args((uint32_t*)userEsp, argv);
    
    // userEsp-=16; // Account for argc and argv
    
    // ((uint32_t*)userEsp)[0] = 1;
    // ((uint32_t*)userEsp)[1] = (uint32_t)userEsp + 8;
    // ((uint32_t*)userEsp)[2] = (uint32_t)userEsp + 12;

    // ((char*)userEsp)[12] = 'i';
    // ((char*)userEsp)[13] = 'n';
    // ((char*)userEsp)[14] = 'i';
    // ((char*)userEsp)[15] = 't';
    // ((char*)userEsp)[16] = '\0';

    // char** argv = (char**)((uint32_t*)userEsp)[1];

    // Debug::printf("HERE %s\n", argv[0]);



    ASSERT(userEsp % 16 == 0);

    // Debug::printf("user esp %x\n",userEsp);
    // Current state:
    //     - %eip points somewhere in the middle of kernelMain
    //     - %cs contains kernelCS (CPL = 0)
    //     - %esp points in the middle of the thread's kernel stack
    //     - %ss contains kernelSS
    //     - %eflags has IF=1
    // Desired state:
    //     - %eip points at e
    //     - %cs contains userCS (CPL = 3)
    //     - %eflags continues to have IF=1
    //     - %esp points to the bottom of the user stack
    //     - %ss contain userSS
    // User mode will never "return" from switchToUser. It will
    // enter the kernel through interrupts, exceptions, and system
    // calls.
    switchToUser(e,userEsp,0);
    Debug::panic("*** implement switchToUser in machine.S\n");
}

