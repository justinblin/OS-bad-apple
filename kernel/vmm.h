#pragma once

#include "stdint.h"
#include "shared.h"
#include "physmem.h"
#include "vme.h"
// #include "blocking_lock.h"

class Node;
class BlockingLock;

namespace VMM {

    extern BlockingLock* shared_vme_lock;
    extern VME<BlockingLock>* shared_vme;

    extern void remove_PT_mapping(uint32_t* PD, uint32_t PDI);
    extern void remove_mapping(uint32_t* PD, uint32_t VPN);

    extern uint32_t new_page_directory();
    extern void copy_page_directory(uint32_t* pd, uint32_t* new_pd);

    // Called (on the initial core) to initialize data structures, etc
    extern void global_init();

    // Called on each core to do per-core initialization
    extern void per_core_init();

    // naive mmap
    extern void* naive_mmap(uint32_t size, bool shared, StrongPtr<Node> file, uint32_t file_offset, bool user);

    // naive munmap
    void naive_munmap(void* p, bool user);

}

