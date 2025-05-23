#pragma once
#include <stdint.h>
#include "block_io.h"
#include "blocking_lock.h"
#include "shared.h"

// Simple (way too simple) device driver for IDE devices (mostly disks)
//
// IDE (integrated device electronics) is the original standard for
// PC disk controllers. We use it because it is simple
//
// The original IDE controller had 2 channels with up to 2 devices
// attached to each channel. Those are the A:, B:, C:, and D: devices
// in an old PC. Internally, those devices are assigned number (0..3)
//
class Ide : public BlockIO {  // We are a block device

    constexpr static uint32_t sector_size = 512;  // older disks had a sector size of 512B
    const uint32_t drive; /* 0 -> A, 1 -> B, 2 -> C, 3 -> D */
    const uint32_t latency_ms;
    uint32_t last = uint32_t(-1);
    BlockingLock lock{};
public:
    Ide(uint32_t drive, uint32_t latency_ms) : BlockIO(sector_size), drive(drive), latency_ms(latency_ms) {}

    virtual ~Ide() {}
    
    // Read the given block into the given buffer. We assume the
    // buffer is big enough
    void read_block(uint32_t block_number, char* buffer) override;

    // We lie because I'm too lazy to get the actual drive size
    // This means that we'll get QEMU errors if we try to access
    // non existent blocks.
    uint32_t size_in_bytes() {
        // This is not correct
        return ~(uint32_t(0));
    }

};

