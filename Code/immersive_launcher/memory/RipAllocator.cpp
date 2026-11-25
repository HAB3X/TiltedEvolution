
// Deliberately in no namespace...

#include <Windows.h>

#include <cstdint>
#include <cstring>

extern uint8_t highrip[];
static uint8_t* globalPointer = &highrip[0];

void* RipAllocateN(size_t blockLength)
{
    // Typical alignment size for function calls on x64 systems
    const size_t alignment = 16;

    // Calculate the next aligned address for the start of the block
    uintptr_t currentPtr = reinterpret_cast<uintptr_t>(globalPointer);
    uintptr_t alignedPtr = (currentPtr + (alignment - 1)) & ~(alignment - 1);

    // Pad the unused space with 0xCC
    size_t paddingSize = alignedPtr - currentPtr;
    memset(reinterpret_cast<void*>(currentPtr), 0xCC, paddingSize);

    // Align the block length as well
    size_t alignedBlockLength = (blockLength + (alignment - 1)) & ~(alignment - 1);

    // Advance the global pointer by the aligned block length plus the padding
    globalPointer += alignedBlockLength + paddingSize;

    // Pad the end of the block if there is any space remaining due to alignment
    if (alignedBlockLength > blockLength)
    {
        size_t endPaddingSize = alignedBlockLength - blockLength;
        memset(reinterpret_cast<void*>(alignedPtr + blockLength), 0xCC, endPaddingSize);
    }

    // Blocks hold generated code (JIT hook stubs), but highrip lives in the .game section, which is read-write only:
    // without execute permission the first stub the game runs (the spell projectile hook) faults under Wine's DEP
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(alignedPtr), alignedBlockLength, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Return the aligned pointer
    return reinterpret_cast<void*>(alignedPtr);
}

// TBD
void RipFree(void* apBlock)
{
}
