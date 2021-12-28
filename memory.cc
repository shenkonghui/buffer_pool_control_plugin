#include <fstream>
#include <iostream>
#include "memory.h"
#include <stdlib.h>
using namespace std;

int main()
{
    Memory mem = getMemoryInfo();
    cout << "memTotal: " << mem.MemTotal << endl;
    cout << "memAvailable: " << mem.MemAvailable << endl;
}


Memory getMemoryInfo()
{
    string data;
    string memTotal;
    string memAvailable;
    ifstream infile;
    // infile.open("/proc/meminfo");
    infile.open("/root/meminfo");
    while (infile >> data)
    {
        if (data== "MemTotal:") {
            infile >> memTotal;
        }
        if (data == "MemAvailable:")
        {
            infile >> memAvailable;
        }
    }
   infile.close();

    Memory mem;
    mem.MemAvailable = atoi(memAvailable.c_str());
    mem.MemTotal = atoi(memTotal.c_str()) ;
    return mem;
}

Memory getMemoryInfoInDocker()
{
    string data;
    string memTotal;
    string memAvailable;
    string memUsage;
    ifstream infile;
    infile.open("cat /sys/fs/cgroup/memory/memory.limit_in_bytes");
    while (infile >> data)
    {
        if (data == "hierarchical_memory_limit")
        {
            infile >> memTotal;
        }
        if (data == "rss")
        {
            infile >> memUsage;
        }

    }
    infile.close();

    Memory mem;
    mem.MemTotal = atoi(memTotal.c_str());
    mem.MemUsage = atoi(memTotal.c_str());
    mem.MemAvailable = mem.MemTotal - mem.MemUsage;
    return mem;
}
