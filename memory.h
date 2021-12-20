#include <stdint.h>

struct Memory
{
    uint64_t MemTotal;
    uint64_t MemAvailable;
};

Memory getMemoryInfo();
