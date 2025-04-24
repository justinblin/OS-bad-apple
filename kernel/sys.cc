#include <stdint.h>

#include "sys.h"
#include "stdint.h"
#include "debug.h"
#include "idt.h"
#include "machine.h"
#include "ext2.h"
#include "kernel.h"
#include "threads.h"
#include "vme.h"
#include "libk.h"
#include "elf.h"
#include "promise.h"
#include "vga.h"

bool address_valid(uint32_t addr, uint32_t size) {
    return addr >= 0x80000000 && addr + size <= 0xFFFFFFFF;
}

uint32_t read(uint32_t fd, void* buf, uint32_t nbyte) {
    if(!address_valid((uint32_t)buf, nbyte) || fd >= PCB_ARR_SIZE) {
        return -1;
    }

    auto me = impl::threads::state.current();
    auto open_file = me->pcb->fdt[fd];
    
    if(open_file == nullptr || open_file->node == nullptr) return -1; // File doesn't exist
    uint32_t bytes_read = open_file->node->read_all(open_file->offset, nbyte, (char*)buf);
    // Debug::printf("RET VAL: %d\n", bytes_read);
    open_file->offset += bytes_read;
    return bytes_read;
}

uint32_t write(uint32_t fd, void* buf, int32_t nbyte) {
    if(nbyte < 0 || !address_valid((uint32_t)buf, nbyte) || fd >= PCB_ARR_SIZE) {
        return -1;
    }

    auto me = impl::threads::state.current();
    auto open_file = me->pcb->fdt[fd];

    if(open_file == nullptr || open_file->node == nullptr) return -1; // File doesn't exist
    uint32_t bytes_written = open_file->node->write(open_file->offset, nbyte, (char*)buf);
    open_file->offset += bytes_written;
    return bytes_written;
}

uint32_t open(const char* fd) {
    auto me = impl::threads::state.current();
    if(me->pcb->cwd == nullptr) me->pcb->cwd = fs->root;

    StrongPtr<Node> new_node = fs->find(me->pcb->cwd, fd);
    StrongPtr<OpenFile> new_open_file = StrongPtr<OpenFile>::make(new_node);

    for(uint32_t i = 0; i < PCB_ARR_SIZE; i++) {
        if(me->pcb->fdt[i] == nullptr) { // Found empty file
            me->pcb->fdt[i] = new_open_file;
            return i;
        }
    }
    Debug::printf("FDT IS FULL!\n");
    return -1;
}

uint32_t len(uint32_t fd) {
    if(fd >= PCB_ARR_SIZE) {
        return -1;
    }
    auto me = impl::threads::state.current();
    StrongPtr<OpenFile> open_file = me->pcb->fdt[fd];
    if(open_file == nullptr) return -1;
    return open_file->node->size_in_bytes();
}

uint32_t close(uint32_t fd) {
    auto me = impl::threads::state.current();
    if (fd < PCB_ARR_SIZE) { // file
        me->pcb->fdt[fd] = nullptr;
    } else if (fd < PCB_ARR_SIZE * 2) { // semaphore
        fd -= PCB_ARR_SIZE;
        me->pcb->sdt[fd] = nullptr;
    } else if (fd < PCB_ARR_SIZE * 3) { // children
        fd -= PCB_ARR_SIZE * 2;
        me->pcb->children[fd] = nullptr;
        me->pcb->children_promises[fd] = nullptr;
    } else { // should never happen
        return -1;
    }

    return 0;
}

uint32_t seek(uint32_t fd, uint32_t offset) {
    if(fd >= PCB_ARR_SIZE) {
        return -1;
    }
    auto me = impl::threads::state.current();
    StrongPtr<OpenFile> open_file = me->pcb->fdt[fd];
    if(open_file == nullptr) return -1;
    open_file->offset = offset;
    return open_file->offset;
}

uint32_t fork(uint32_t *frame) {
    auto me = impl::threads::state.current();
    
    using namespace impl::threads;
    
    auto parent_pcb = me->pcb;
    auto parent_pd = me->pd;
    auto parent_vme = me->vme;
    auto parent_jiffies = me->at_jiffies;

    uint32_t pc = frame[0];
    uint32_t sp = frame[3];
    
    StrongPtr<PCB> child_pcb = PCB::duplicate(parent_pcb);
    uint32_t child_pd = VMM::new_page_directory();
    VMM::copy_page_directory((uint32_t*)parent_pd, (uint32_t*)child_pd); // PD
    StrongPtr<VME<NoLock>> child_vme = VME<NoLock>::duplicate(me->vme); // VME


    me->pcb->children_promises[child_pcb->pcb_id] = StrongPtr<Promise<int32_t>>::make();
    child_pcb->parent_promise = me->pcb->children_promises[child_pcb->pcb_id];

    // Debug::printf("HERE\n");
    
    thread([parent_jiffies, pc, sp] {
        auto me = impl::threads::state.current();
        me->at_jiffies = parent_jiffies; // Jiffies  
        // Debug::printf("GO TO PC: %x\n", pc);
        switchToUser(pc, sp, 0);

    }, child_pd, child_pcb, child_vme);
    // Debug::printf("%s\n", new_tcb->pcb->pcb_id);

    return child_pcb->pcb_id + PCB_ARR_SIZE * 2;
}

uint32_t execl(const char* path, const char* argv[]) {
    auto me = impl::threads::state.current();
    
    StrongPtr<Node> node = me->pcb->cwd;
    if(node == nullptr) {
        me->pcb->cwd = node;
        node = fs->root;
    }
    StrongPtr<Node> new_program = fs->find(node, path);
    if(new_program == nullptr) {
        Debug::printf("INVALID PROGRAM PATH\n");
        return -1;
    }
    
    // COPY ARGS TO KERNEL
    uint32_t argc = 0;
    while(argv[argc] != 0) {
        if((uint32_t)argv[argc] < 0x80000000 || (uint32_t)argv[argc] + K::strlen(argv[argc]) >= 0xF0000000) {
            Debug::printf("INVALID ARGUMENT\n");
            return -1;
        }
        argc++;
    }

    const char* k_argv[argc + 1];
    for(uint32_t i = 0; i < argc; i++) {
        uint32_t length = K::strlen(argv[i]) + 1;
        k_argv[i] = new char[length];
        memcpy((void*)k_argv[i], argv[i], length);
    }
    k_argv[argc] = nullptr;
    
    // CLEAR ADDR SPACE
    me->vme = nullptr;
    for(uint32_t i = (0x80000000 >> 22); i < (0xF0000000 >> 22); i++) {
        VMM::remove_PT_mapping((uint32_t*)me->pd, i);
    }
    
    me->vme = StrongPtr<VME<NoLock>>::make(0x80000000, 0xF0000000);
    
    uint32_t e = ELF::load(new_program);

    const uint32_t stack_size = 134217728;
    void* stack_start = VMM::naive_mmap(stack_size, false, {}, 0, false);
    uint32_t userEsp = ((uint32_t)stack_start + stack_size - 16);
    
    userEsp = (uint32_t)load_args((uint32_t*)userEsp, k_argv);
    
    switchToUser(e,userEsp,0);
    return -1;
}

void exit(uint32_t rc) {
    auto me = impl::threads::state.current();
    if(me->pcb->parent_promise != nullptr) {
        me->pcb->parent_promise->set(rc);
    }
    stop();
}

uint32_t wait(uint32_t id, uint32_t* status) {
    //! Need to implement new code for wait for any child
    auto me = impl::threads::state.current();
    if(id >= PCB_ARR_SIZE || me->pcb->children_promises[id] == nullptr) {
        return -1;
    }
    *status = me->pcb->children_promises[id]->get();
    me->pcb->children_promises[id] = nullptr;
    me->pcb->children[id] = nullptr;

    return 0;
}

uint32_t chdir(const char* path) {
    auto me = impl::threads::state.current();
    ASSERT(!(fs == nullptr));
    ASSERT(!(me == nullptr));
    ASSERT(!(me->pcb == nullptr));

    StrongPtr<Node> node = me->pcb->cwd;
    if(node == nullptr) node = fs->root;

    auto new_node = fs->find(node, path);
    if(new_node == nullptr) return -1;
    me->pcb->cwd = new_node;
    return 0;
}

uint32_t sem(uint32_t initial) {
    auto me = impl::threads::state.current();
    uint32_t id = 0;
    while(!(me->pcb->sdt[id] == nullptr) && id < PCB_ARR_SIZE) {
        id++;
    }
    if(id == PCB_ARR_SIZE) return -1;

    me->pcb->sdt[id] = StrongPtr<Semaphore>::make(initial);
    return id + PCB_ARR_SIZE;
}

uint32_t up(uint32_t id) {
    auto me = impl::threads::state.current();
    if(id < 0 || id >= PCB_ARR_SIZE || me->pcb->sdt[id] == nullptr ) {
        Debug::printf("UP IS INVALID\n");
        return -1;
    }
    me->pcb->sdt[id]->up();

    return 0;
}
uint32_t down(uint32_t id) {
    auto me = impl::threads::state.current();
    if(id < 0 || id >= PCB_ARR_SIZE || me->pcb->sdt[id] == nullptr ) {
        Debug::printf("DOWN IS INVALID\n");
        return -1;
    }
    me->pcb->sdt[id]->down();
    
    return 0;
}

void* naive_mmap(uint32_t size, bool is_shared, uint32_t file, uint32_t offset) {
    auto me = impl::threads::state.current();
    StrongPtr<OpenFile> open_file = me->pcb->fdt[file];
    StrongPtr<Node> node = (open_file == nullptr) ? StrongPtr<Node>() : open_file->node;

    return VMM::naive_mmap(size, is_shared, node, offset, true);
}

void naive_unmap(void* p) {
    VMM::naive_munmap(p, true);
}

void vga_syscall() {
    vga_test();
}

extern "C" int sysHandler(uint32_t eax, uint32_t *frame) {
    //Debug::printf("*** syscall #%d\n",eax);
    uint32_t *user_sp = (uint32_t*)frame[3];
    if((uint32_t)user_sp < 0x80000000 || (uint32_t)user_sp >= 0xF0000000) {
        Debug::printf("USER ESP IS INVALID\n");
        exit(-1);
    }
    switch (eax) {
    case 0:
        exit(user_sp[1]);    
        return -1;
    case 1:
        return write(user_sp[1], (void*)user_sp[2], user_sp[3]);
    case 2:
        return fork(frame);
    case 3:
        return sem(user_sp[1]);
    case 4:
        return up(user_sp[1] - PCB_ARR_SIZE);
    case 5:
        return down(user_sp[1] - PCB_ARR_SIZE);
    case 6:
        return close(user_sp[1]);
    case 7: /* shutdown */
        Debug::shutdown();
        return -1;
    case 8:
        return wait(user_sp[1] - PCB_ARR_SIZE * 2, (uint32_t*)user_sp[2]);
    case 9:
        return execl((const char*)user_sp[1], (const char**)((char*)user_sp + 8));
    case 10:
        return open((const char*)user_sp[1]);
    case 11:
        return len(user_sp[1]);
    case 12:
        return read(user_sp[1], (void*)user_sp[2], user_sp[3]);
    case 13:
        return seek(user_sp[1], user_sp[2]);
    case 100:
        return chdir((const char*)user_sp[1]);
    case 101:
        return (uint32_t)naive_mmap(user_sp[1], user_sp[2], user_sp[3], user_sp[4]);
    case 102:
        naive_unmap((void*)user_sp[1]);
        return 0;
    case 103:
        Debug::printf("*** heya");
        vga_test();
        return 0;
    case 418:
        Debug::printf("*** I'm a teapot\n");
        return -1;
    default:
        Debug::printf("*** unknown system call %d\n",eax);
        return -1;
    }
    
}   

void SYS::init(void) {
    IDT::trap(48,(uint32_t)sysHandler_,3);
}
