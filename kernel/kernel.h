#ifndef _KERNEL_H_
#define _KERNEL_H_

#include "ext2.h"

extern StrongPtr<Ext2> fs;

uint32_t* load_args(uint32_t* userEsp, const char** argv);
void kernelMain(void);

#endif
