#pragma once
#include "doctest.h"
#include "engine/render/PersistentBuffer.hpp"
#include <vector>

    TEST_CASE("[Tech] PersistentBuffer - Creation and Lifecycle") {
        NoMoreDay::render::PersistentBuffer buffer;
        const size_t slotSize = 1024;
        buffer.Create(slotSize);
        
        CHECK(buffer.GetSize() == slotSize);
        CHECK(buffer.GetId() != 0);
        
        buffer.Destroy();
        CHECK(buffer.GetId() == 0);
    }
    
    TEST_CASE("[Tech] PersistentBuffer - Write and Flush Cycle") {
        NoMoreDay::render::PersistentBuffer buffer;
        const size_t slotSize = sizeof(int);
        buffer.Create(slotSize);
        
        // Run a simulation of 3 frames to fill the ring buffer
        
        // Frame 0: Write 100
        int* p0 = (int*)buffer.BeginWrite();
        *p0 = 100;
        buffer.Flush();
        buffer.Lock(); 
        
        // Frame 1: Write 200
        int* p1 = (int*)buffer.BeginWrite();
        *p1 = 200;
        buffer.Flush();
        buffer.Lock();
        
        // Frame 2: Write 300
        int* p2 = (int*)buffer.BeginWrite();
        *p2 = 300;
        buffer.Flush();
        buffer.Lock();
        
        // Frame 3: BeginWrite returns to Slot 0
        int* p3 = (int*)buffer.BeginWrite();
        
        // Verify Pointer Arithmetic (if persistent)
        // Note: PersistentBuffer aligns each slot to 256 bytes.
        if (buffer.GetMode() == NoMoreDay::render::PersistentBuffer::Mode::Persistent) {
            CHECK(p3 == p0);
            
            // p1 should be p0 + alignedSlotSize / sizeof(int)
            const size_t alignedSize = 256; // Defined in PersistentBuffer.cpp
            size_t step = alignedSize / sizeof(int);
            CHECK(p1 == p0 + step);
            CHECK(p2 == p1 + step);
            
            // Read Back Verification
            // Read() copies from the most recently Locked slot.
            // After Frame 2's Lock, targetSlot is 2, which holds 300.
            int readVal = 0;
            buffer.Read(&readVal, sizeof(int));
            CHECK(readVal == 300);
        } else {
            // Compat mode
            // p0, p1, p2 point to staging buffer (same address)
            CHECK(p0 == p1);
            CHECK(p1 == p2);
            CHECK(p3 == p0);
            
            // Read should get what was last uploaded to GPU (300 from Frame 2)
            int readVal = 0;
            buffer.Read(&readVal, sizeof(int));
            CHECK(readVal == 300); 
        }
        
        buffer.Destroy();
    }

