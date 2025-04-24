#include "processes.h"
#include "ext2.h"
#include "atomic.h"
#include "promise.h"

OpenFile::OpenFile(StrongPtr<Node> node): node(node) {};
OpenFile::~OpenFile(){};

StrongPtr<Node> stdin = StrongPtr<Node>((Node*) new STDIN());
StrongPtr<Node> stdout = StrongPtr<Node>((Node*) new STDOUT());
StrongPtr<Node> stderr = StrongPtr<Node>((Node*) new STDERR());

PCB::PCB(uint32_t pcb_id): pcb_id(pcb_id){
    fdt[0] = StrongPtr<OpenFile>::make(stdin);
    fdt[1] = StrongPtr<OpenFile>::make(stdout);
    fdt[2] = StrongPtr<OpenFile>::make(stderr);
};
PCB::~PCB(){};

StrongPtr<PCB> PCB::duplicate(StrongPtr<PCB> parent_pcb) {
    StrongPtr<PCB> child_pcb;
    
    uint32_t child_idx = 0;
    while(child_idx < PCB_ARR_SIZE && parent_pcb->children[PCB_ARR_SIZE] == nullptr){
        child_idx++;
    }

    child_pcb = StrongPtr<PCB>::make(child_idx);
    parent_pcb->children[child_idx] = child_pcb;

    ASSERT(child_pcb->pcb_id < PCB_ARR_SIZE);

    child_pcb->cwd = parent_pcb->cwd;
    child_pcb->parent = parent_pcb; // set child's parent
    
    for(uint32_t i = 0; i < PCB_ARR_SIZE; i++) {
        child_pcb->fdt[i] = parent_pcb->fdt[i];
    }

    for(uint32_t i = 0; i < PCB_ARR_SIZE; i++) {
        child_pcb->sdt[i] = parent_pcb->sdt[i];
    }

    return child_pcb;
}