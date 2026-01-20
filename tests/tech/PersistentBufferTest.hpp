#pragma once
#include "doctest.h"
#include "engine/render/PersistentBuffer.hpp"
#include <vector>

TEST_SUITE("PersistentBuffer") {
    TEST_CASE("Creation and Lifecycle") {
        NoMoreDay::render::PersistentBuffer buffer;
        const size_t slotSize = 1024;
        buffer.Create(slotSize);
        
        CHECK(buffer.GetSize() == slotSize);
        CHECK(buffer.GetId() != 0);
        
        buffer.Destroy();
        CHECK(buffer.GetId() == 0);
    }
    
    TEST_CASE("Write and Flush Cycle") {
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
        // p0, p1, p2 should be distinct and spaced by slotSize?
        // Actually BeginWrite returns mappedPtr + slotIndex * slotSize.
        // So p3 should equal p0.
        if (buffer.GetMode() == NoMoreDay::render::PersistentBuffer::Mode::Persistent) {
            CHECK(p3 == p0);
            CHECK(p1 == p0 + 1); // int pointer arithmetic, +1 int = +4 bytes. slotSize=4. CORRECT.
            CHECK(p2 == p1 + 1);
            
            // Read Back Verification
            // Read() copies from Current Slot (Slot 0).
            // Slot 0 holds what we wrote in Frame 0 (100).
            int readVal = 0;
            buffer.Read(&readVal, sizeof(int));
            CHECK(readVal == 100);
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
}
