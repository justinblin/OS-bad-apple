#pragma once

#include "shared.h"

const uint32_t PCB_ARR_SIZE = 100;

class Node;

template <typename T>
class Promise;

class Semaphore;


extern StrongPtr<Node> stdio;
extern StrongPtr<Node> stdout;

struct OpenFile {
    StrongPtr<Node> node;
    uint32_t offset = 0;
    // permissions
    OpenFile(StrongPtr<Node> node);
    ~OpenFile();
};

enum PCB_State {
    RUNNING,
    WAITING,
    ZOMBIE,
    TERMINATED
};

struct PCB {
    uint32_t pcb_id;
    StrongPtr<Node> cwd;
    StrongPtr<PCB> parent;
    StrongPtr<PCB> children[PCB_ARR_SIZE];
    StrongPtr<OpenFile> fdt[PCB_ARR_SIZE];
    StrongPtr<Semaphore> sdt[PCB_ARR_SIZE];
    
    StrongPtr<Promise<int32_t>> parent_promise;
    StrongPtr<Promise<int32_t>> children_promises[PCB_ARR_SIZE];
    PCB_State state = RUNNING;
    uint32_t exit_status;

    PCB(uint32_t pcb_id);
    ~PCB();
    static StrongPtr<PCB> duplicate(StrongPtr<PCB> pcb);
};