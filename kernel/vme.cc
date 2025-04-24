//! TERRIBLE IMPLEMENTATION - IT WORKS SO DON'T WORRY ABOUT IT

#include "vme.h"
#include "physmem.h"
#include "ext2.h"

template class VME<NoLock>;
template class VME<BlockingLock>;

template void VME<NoLock>::insert_entry_sorted(uint32_t, uint32_t, uint32_t, StrongPtr<Node>, uint32_t, bool);
template void VME<NoLock>::coallescing();
template void VME<NoLock>::insert_free_space(uint32_t, uint32_t);
template StrongPtr<VMEEntry> VME<NoLock>::get(uint32_t);
template uint32_t VME<NoLock>::add_entry(uint32_t, StrongPtr<Node>, uint32_t, bool);
template void VME<NoLock>::remove_entry(uint32_t, bool);
// template StrongPtr<VME<NoLock>> VME<NoLock>::duplicate(StrongPtr<VME<NoLock>>);

template void VME<BlockingLock>::insert_entry_sorted(uint32_t, uint32_t, uint32_t, StrongPtr<Node>, uint32_t, bool);
template void VME<BlockingLock>::coallescing();
template void VME<BlockingLock>::insert_free_space(uint32_t, uint32_t);
template StrongPtr<VMEEntry> VME<BlockingLock>::get(uint32_t);
template uint32_t VME<BlockingLock>::add_entry(uint32_t, StrongPtr<Node>, uint32_t, bool);
template void VME<BlockingLock>::remove_entry(uint32_t, bool);
// template StrongPtr<VME<BlockingLock>> VME<BlockingLock>::duplicate(StrongPtr<VME<BlockingLock>>);

VMEEntry::VMEEntry(uint32_t start, uint32_t num_pages, uint32_t size, StrongPtr<Node> file, uint32_t file_offset, bool user): start(start), num_pages(num_pages), size(size), file(file), file_offset(file_offset), user(user) {};
VMEEntry::~VMEEntry(){
    for(uint32_t i = 0; i < num_pages; i++) {
        VMM::remove_mapping((uint32_t*)getCR3(), ((uint32_t)start + i * PhysMem::FRAME_SIZE) >> 12);
    }
};

template <typename Lock>
VME<Lock>::VME(uint32_t start, uint32_t end) : avail(start), limit(end), lock() {};

template <typename Lock>
VME<Lock>::~VME() {
}


template <typename Lock>
void VME<Lock>::insert_entry_sorted(uint32_t start, uint32_t num_pages, uint32_t size,
                         StrongPtr<Node> file, uint32_t file_offset, bool user) {
    StrongPtr<VMEEntry> prev{nullptr};
    StrongPtr<VMEEntry> curr = entries;

    while (!(curr == nullptr) && curr->start < start) {
        //! LOOP HERE -  Debug::printf("HERE\n");
        prev = curr;
        curr = curr->next;
    }


    StrongPtr<VMEEntry> new_node =
        StrongPtr<VMEEntry>::make(start, num_pages, size, file, file_offset, user);
    new_node->next = curr;
    if (prev == nullptr) {
        entries = new_node;
    } else {
        prev->next = new_node;
    }
}

template <typename Lock>
void VME<Lock>::coallescing() {
    StrongPtr<FreeEntry> curr = free_entries;
    while (!(curr == nullptr) && !(curr->next == nullptr)) {
        if (curr->start + curr->num_pages * PhysMem::FRAME_SIZE ==
            curr->next->start) {
            curr->num_pages += curr->next->num_pages;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

template <typename Lock>
void VME<Lock>::insert_free_space(uint32_t start, uint32_t num_pages) {
    StrongPtr<FreeEntry> prev{nullptr};
    StrongPtr<FreeEntry> curr = free_entries;

    while (!(curr == nullptr) && curr->start < start) {
        prev = curr;
        curr = curr->next;
    }

    StrongPtr<FreeEntry> new_node =
        StrongPtr<FreeEntry>::make(start, num_pages);
    new_node->next = curr;
    if (prev == nullptr) {
        free_entries = new_node;
    } else {
        prev->next = new_node;
    }

    coallescing();
}


template <typename Lock>
StrongPtr<VMEEntry> VME<Lock>::get(uint32_t va) {
    LockGuard g{lock};
    StrongPtr<VMEEntry> curr = entries;
    while (!(curr == nullptr)) {
        if (va >= curr->start && va < curr->start + curr->num_pages * PhysMem::FRAME_SIZE) {
            return curr;
        }
        curr = curr->next;
    }
    return StrongPtr<VMEEntry>();
}

template <typename Lock>
uint32_t VME<Lock>::add_entry(uint32_t size, StrongPtr<Node> file, uint32_t file_offset, bool user) {
    LockGuard g{lock};
    StrongPtr<FreeEntry> prev{nullptr};
    StrongPtr<FreeEntry> curr = free_entries;

    uint32_t va = 0;

    
    uint32_t required_pages =
    (size + PhysMem::FRAME_SIZE - 1) / PhysMem::FRAME_SIZE;
    
    while (!(curr == nullptr)) {
        if (curr->num_pages >= required_pages) {
            va = curr->start;
            curr->start += required_pages * PhysMem::FRAME_SIZE;
            curr->num_pages -= required_pages;

            if (curr->num_pages == 0) {
                if (prev == nullptr) {
                    free_entries = curr->next;
                } else {
                    prev->next = curr->next;
                }
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    
    if (va == 0) {
        if (avail + size > limit) return 0;  //! NO MORE SPACE
        va = avail;
        avail += PhysMem::FRAME_SIZE * required_pages;
    }
    
    insert_entry_sorted(va, required_pages, size, file, file_offset, user);
    return va;
}


template <typename Lock>
void VME<Lock>::remove_entry(uint32_t va, bool user) {
    LockGuard g{lock};
    StrongPtr<VMEEntry> prev{nullptr};
    StrongPtr<VMEEntry> curr = entries;


    while (!(curr == nullptr)) {
        if (va >= curr->start && va < curr->start + curr->num_pages * PhysMem::FRAME_SIZE && (!user || curr->user == user)) {
            uint32_t start = curr->start;
            uint32_t size = curr->num_pages;

            if (prev == nullptr) {
                entries = curr->next;
            } else {
                prev->next = curr->next;
            }

            insert_free_space(start, size);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    return;
}

template <typename Lock>
StrongPtr<VME<Lock>> VME<Lock>::duplicate(StrongPtr<VME<Lock>> vme) {
    StrongPtr<VME<Lock>> new_vme = StrongPtr<VME<Lock>>::make(vme->avail, vme->limit);
    new_vme->entries = VMEEntry::duplicate(vme->entries);
    new_vme->free_entries = FreeEntry::duplicate(vme->free_entries);
    return new_vme;
}

StrongPtr<FreeEntry> FreeEntry::duplicate(StrongPtr<FreeEntry> free_entry) {
    if(free_entry == nullptr) return StrongPtr<FreeEntry>();
    StrongPtr<FreeEntry> new_free_entry = StrongPtr<FreeEntry>::make(free_entry->start, free_entry->num_pages);
    new_free_entry->next = FreeEntry::duplicate(free_entry->next);
    return new_free_entry;
}

StrongPtr<VMEEntry> VMEEntry::duplicate(StrongPtr<VMEEntry> vme_entry) {
    if(vme_entry == nullptr) return StrongPtr<VMEEntry>();
    // Debug::printf("HERE\n");
    // Debug::printf("START: %x | NUM_PAGES: %d | SIZE: %d | FILE: %d | OFFSET: %d\n", vme_entry->start, vme_entry->num_pages, vme_entry->size, vme_entry->file, vme_entry->file_offset);
    StrongPtr<VMEEntry> new_vme_entry = StrongPtr<VMEEntry>::make(vme_entry->start, vme_entry->num_pages, vme_entry->size, vme_entry->file, vme_entry->file_offset, vme_entry->user);
    // Debug::printf("AFTER CREATION\n");
    new_vme_entry->next = VMEEntry::duplicate(vme_entry->next);
    return new_vme_entry;
}