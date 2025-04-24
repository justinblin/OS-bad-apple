#pragma once

#include "blocking_lock.h"
#include "ide.h"
#include "block_io.h"
#include "shared.h"
#include "ide.h"
#include "atomic.h"
#include "debug.h"


namespace impl::ext2 {

    struct GlobalIndex {
        uint32_t index;
        StrongPtr<GlobalIndex> next{nullptr};
        WeakPtr<GlobalIndex> prev{StrongPtr<GlobalIndex>()};
    
        GlobalIndex(uint32_t index): index(index){}; 
    };
    
    struct Block {
        uint32_t block_id;
        char* buffer;
        StrongPtr<Block> next{nullptr};
        WeakPtr<Block> prev{StrongPtr<Block>()};
    
        Block(uint32_t block_id, uint32_t block_size) : block_id(block_id) {
            buffer = new char[block_size];
        };
    
        ~Block() {
            delete[] buffer;
        }
    };
    
    class BlockList {
        StrongPtr<Block> first{nullptr};
        StrongPtr<Block> last{nullptr};
        BlockingLock lock{};
    
    public:
        StrongPtr<GlobalIndex> global_index;
    
        BlockList(uint32_t index) {
            global_index = StrongPtr<GlobalIndex>::make(index);
        };
    
        void print() {
            StrongPtr<Block> curr = first;
            while(!(curr == nullptr)) {
                // Debug::printf("CURR %d | ", curr->block_id);
                curr = curr->next;
            }
            // Debug::printf("\n");
        }
    
        bool remove() {
            LockGuard<BlockingLock> g{lock};
            if(last == nullptr) return false;
            StrongPtr<Block> last_prev = last->prev.promote();
            last = last_prev;
            if(last == nullptr) {
                first = nullptr;
            } else {
                last->next = nullptr;
            }
            return true;
        }
    
        StrongPtr<Block> extract(uint32_t block_id) {
            LockGuard<BlockingLock> g{lock};
            StrongPtr<Block> prev = StrongPtr<Block>();
            StrongPtr<Block> curr = first;
            while(!(curr == nullptr)) {
                if(curr->block_id == block_id) {
                    if(first == curr) {
                        first = curr->next;
                    }
                    if(last == curr) {
                        last = nullptr;
                    }
    
    
                    if(!(prev == nullptr)) prev->next = curr->next;
                    if(!(curr->next == nullptr)) {
                        curr->next->prev = prev;
                    } else {
                        last = prev;
                    }
                    
                    curr->next = nullptr;
                    curr->prev = StrongPtr<Block>();
                    return curr;
                }
                prev = curr;
                curr = curr->next;
            }
            return StrongPtr<Block>();
        }
    
        void add(StrongPtr<Block> block) {
            LockGuard<BlockingLock> g{lock};
            block->next = first;
            if(first == nullptr) {
                last = block;
            } else {
                first->prev = block;
            }
            first = block;
        }
    };
    
    class BufferCache: public BlockIO {
    private:
        StrongPtr<Ide> ide;
        BlockList** indices;
        
        StrongPtr<GlobalIndex> global_index_first{nullptr};
        StrongPtr<GlobalIndex> global_index_last{nullptr};
        BlockingLock global_index_lock{};
    
        uint32_t size = 997;
        Atomic<uint32_t> count{0};
    
        
    public:
        explicit BufferCache(StrongPtr<Ide> ide): BlockIO(ide->block_size) {
            this->ide = ide;
            indices = new BlockList*[size];
            for(uint32_t i = 0; i < size; i++) {
                indices[i] = new BlockList(i);
            }
        };
    
        virtual ~BufferCache() {
            for(uint32_t i = 0; i < size; i++) {
                delete indices[i];
            }
            delete[] indices;
        }
    
        uint32_t size_in_bytes() override {
            return ide->size_in_bytes();
        }
    
        void read_block(uint32_t block_number, char* buffer) override {
            // ide->read_block(block_number, buffer);
            // return;
            // 1. Find Block
            // Debug::printf("Reading Block %d\n", block_number);
            uint32_t index = block_number % size;
            // Debug::printf("Index %d\n", index);
            BlockList* list = indices[index];
    
            // Debug::printf("LIST INDEX %d: ", index);
            // list->print();
            StrongPtr<Block> block = list->extract(block_number);
            // list->print();
            // 2. If Block not Found:
            if(block == nullptr) {
                // Debug::printf("Not in Memory\n");
                // - Read from Memory (PROMISE)
                block = StrongPtr<Block>::make(block_number, block_size);
                ide->read_block(block_number, block->buffer);
                // - Increment count
                if(count.add_fetch(1) >= size) { // - If count bigger than size:
                        ASSERT(!(global_index_last == nullptr));
                        global_index_lock.lock();
                        StrongPtr<GlobalIndex> oldest = global_index_last;
                        ASSERT(!(global_index_last->prev.promote() == nullptr));
                        StrongPtr<GlobalIndex> global_index_last_prev = global_index_last->prev.promote();
                        global_index_last = global_index_last_prev;
                        global_index_last->next = nullptr;
                        ASSERT(!(global_index_first == nullptr));
                        BlockList* oldest_list = indices[oldest->index];
                        oldest_list->remove();
                        count.set(size);
                        global_index_lock.unlock();
                }
            } else {
                // Debug::printf("In Memory\n");
            }
            
            // 3. If Block Found
            memcpy(buffer, block->buffer, block_size);
            // Debug::printf("ADDING TO LIST %d\n", index);
            list->add(block);
            
            global_index_lock.lock();
            StrongPtr<GlobalIndex> global_index = list->global_index;
            if(!(global_index->prev.promote() == nullptr)) {
                StrongPtr<GlobalIndex> global_index_next = global_index->next;
                global_index->prev.promote()->next = global_index_next;
                if(!(global_index->next == nullptr)) {
                    global_index->next->prev = global_index->prev;
                }
            }
            global_index->next = global_index_first;
            global_index->prev = StrongPtr<GlobalIndex>();
            if(!(global_index_first == nullptr)) global_index_first->prev = global_index;
            global_index_first = global_index;
            if(global_index_last == nullptr) global_index_last = global_index;
            global_index_lock.unlock();
    
        }
    
    };
};