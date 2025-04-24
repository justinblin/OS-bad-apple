#pragma once

#include "stdint.h"

extern "C" void kernelInit(void);
extern "C" uint32_t kernelPickStack(void);

extern bool onHypervisor;

