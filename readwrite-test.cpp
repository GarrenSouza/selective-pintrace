#include <iostream>
#include "pintrace.h"

#define VEC_SIZE 100

using namespace std;

int main() {
    int *beginMemory = (int*) malloc(sizeof(int) * VEC_SIZE);
    
    int *endMemory = beginMemory + VEC_SIZE;
    set_enable_trace(true);

    add_mem((unsigned long long) beginMemory, (unsigned long long) endMemory);

    for(int i=0; i<VEC_SIZE; i++) {
        beginMemory[i] = i*2;
    }


    for (int i = 0; i < VEC_SIZE; i++)
    {
        cout << beginMemory[i] << endl;
    }    

    remove_mem((unsigned long long) beginMemory, (unsigned long long) endMemory);
    set_enable_trace(false);
}