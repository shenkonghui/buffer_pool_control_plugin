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
    infile.open("/proc/meminfo");
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
