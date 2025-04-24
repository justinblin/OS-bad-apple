#ifndef _SYS_H_
#define _SYS_H_

extern void exit(uint32_t rc); //  0
extern uint32_t write(uint32_t fd, void* buf, int32_t nbyte); // 1
extern uint32_t fork(); //  2
extern uint32_t sem(uint32_t initial); //  3
extern uint32_t up(uint32_t id); //  4
extern uint32_t down(uint32_t id); //  5
extern uint32_t close(uint32_t id); //  6
// shutdown // 7
extern uint32_t wait(uint32_t id, uint32_t *status); // 8
extern uint32_t execl(const char* path, const char* argv[]); // 9;
extern uint32_t open(const char* fn, uint32_t flags); // 10
extern uint32_t len(uint32_t fd); // 11
extern uint32_t read(uint32_t fd, void* buf, uint32_t nbyte); // 12
extern uint32_t seek(uint32_t fd, uint32_t offset); // 13;


extern uint32_t chdir(const char* path); // 100; 
void* naive_mmap(uint32_t size, bool is_shared, uint32_t file, uint32_t offset); // 101 
void naive_unmap(void* p); // 102
void iamateapot(); // 418


class SYS {
public:
    static void init(void);
    static int exec(const char* path, int argc, const char* argv[]);
};

#endif
