#include "elf.h"
#include "debug.h"
#include "shared.h"
#include "vmm.h"

uint32_t ELF::load(StrongPtr<Node> file) {
    ElfHeader hdr;
    
    file->read(0,hdr);

    
    if(hdr.maigc0 != 0x7f || hdr.magic1 != 'E' || hdr.magic2 != 'L' || hdr.magic3 != 'F') {
        Debug::panic("ELF NOT VALID\n");
        return 0;
    }
    
    uint32_t hoff = hdr.phoff;

    for (uint32_t i=0; i<hdr.phnum; i++) {
        ProgramHeader phdr;
        file->read(hoff,phdr);
        hoff += hdr.phentsize;

        if (phdr.type == 1) {
            char *p = (char*) phdr.vaddr;
            uint32_t memsz = phdr.memsz;
            uint32_t filesz = phdr.filesz;

            if((uint32_t)p < 0x80000000 || (uint32_t)p + memsz >= 0xf0000000) {
                Debug::panic("ELF P IN UNVALID SECTION\n");
                return 0;
            }

            if((uint32_t)p > 0x80000000) {
                Debug::printf("P NOT AT 0x80000000\n");
                VMM::naive_mmap((uint32_t)p - 0x80000000, false, {}, 0, false);
            }

            VMM::naive_mmap(memsz, false, {}, 0, false);

            Debug::printf("vaddr:%x memsz:0x%x filesz:0x%x fileoff:%x\n",
                p,memsz,filesz,phdr.offset);
            file->read_all(phdr.offset,filesz,p);
            bzero(p + filesz, memsz - filesz);
        }
    }

    return hdr.entry;
}
