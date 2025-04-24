//! TERRIBLE IMPLEMENTATION - IT WORKS SO DON'T WORRY ABOUT IT

#pragma once

#include "debug.h"
#include "shared.h"

class Node;

struct VMEEntry {
    uint32_t start;
    uint32_t num_pages;
    uint32_t size;
    StrongPtr<Node> file;
    uint32_t file_offset;
    StrongPtr<VMEEntry> next{nullptr};
    bool user;
    
    VMEEntry(uint32_t start, uint32_t num_pages, uint32_t size, StrongPtr<Node> file, uint32_t file_offset, bool user);
    ~VMEEntry();

    static StrongPtr<VMEEntry> duplicate(StrongPtr<VMEEntry> vme_entry);
};

struct FreeEntry {
    uint32_t start;
    uint32_t num_pages;
    StrongPtr<FreeEntry> next;

    FreeEntry(uint32_t start, uint32_t num_pages): start(start), num_pages(num_pages) {};

    static StrongPtr<FreeEntry> duplicate(StrongPtr<FreeEntry> free_entry);
};


template <typename LockType>
class VME {
    
    public:
    uint32_t avail;
    uint32_t limit;
    LockType lock;

    StrongPtr<VMEEntry> entries;
    StrongPtr<FreeEntry> free_entries;

    void coallescing();
    void insert_entry_sorted(uint32_t start, uint32_t num_pages, uint32_t size, StrongPtr<Node> file, uint32_t file_offset, bool user);
    void insert_free_space(uint32_t start, uint32_t num_pages);

    VME(uint32_t start, uint32_t end);
    ~VME();

    StrongPtr<VMEEntry> get(uint32_t va);
    uint32_t add_entry(uint32_t size, StrongPtr<Node> file, uint32_t file_offset, bool user);
    void remove_entry(uint32_t va, bool user);

    static StrongPtr<VME> duplicate(StrongPtr<VME> vme);
};