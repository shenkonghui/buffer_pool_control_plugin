#include <stdint.h>

struct Memory
{
    int64_t MemTotal;
    int64_t MemUsage;
    int64_t MemAvailable;
};

Memory getMemoryInfo();
Memory getMemoryInfoInDocker();
