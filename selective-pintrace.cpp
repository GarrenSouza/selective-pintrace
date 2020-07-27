/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include <set>

#include "pin.H"


FILE * trace;

typedef std::pair<int, int> Range;

struct RangeCompare
{
    //overlapping ranges are considered equivalent
    bool operator()(const Range& lhv, const Range& rhv) const
    {   
        return lhv.second < rhv.first;
    } 
};

std::set<Range, RangeCompare> ranges;
bool enableTrace;


bool in_range(const std::set<Range, RangeCompare>& ranges, unsigned long long value)
{
    return ranges.find(Range(value, value)) != ranges.end();
}

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr)
{
    bool is_recording = in_range(ranges, (unsigned long long)addr);

    if(is_recording) {
        fprintf(trace,"%p: R %p\n", ip, addr);
    }
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr)
{
    bool is_recording = in_range(ranges, (unsigned long long)addr);

    if(is_recording) {
        fprintf(trace,"%p: W %p\n", ip, addr);
    }
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.

    if(enableTrace) {
        UINT32 memOperands = INS_MemoryOperandCount(ins);

        // Iterate over each memory operand of the instruction.
        for (UINT32 memOp = 0; memOp < memOperands; memOp++)
        {
            if (INS_MemoryOperandIsRead(ins, memOp))
            {
                INS_InsertPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                    IARG_INST_PTR,
                    IARG_MEMORYOP_EA, memOp,
                    IARG_END);
            }
            // Note that in some architectures a single memory operand can be 
            // both read and written (for instance incl (%eax) on IA-32)
            // In that case we instrument it once for read and once for write.
            if (INS_MemoryOperandIsWritten(ins, memOp))
            {
                INS_InsertPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                    IARG_INST_PTR,
                    IARG_MEMORYOP_EA, memOp,
                    IARG_END);
            }
        }
    }
}

VOID Fini(INT32 code, VOID *v)
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

VOID add_mem(unsigned long long start_address, unsigned long long end_address)
{
    ranges.insert(Range(start_address, end_address));
    fprintf(trace, "%s %llu %llu\n", "add_mem", start_address, end_address);
}

VOID remove_mem(unsigned long long start_address, unsigned long long end_address)
{
    ranges.erase(Range(start_address, end_address));
    fprintf(trace, "%s %llu %llu\n", "remove_mem", start_address, end_address);
}

VOID set_enable_trace(bool enable_trace) {
    enableTrace = enable_trace;
    fprintf(trace, "%s %d\n", "set_enable_trace", enableTrace);

}


VOID Routine(RTN rtn, VOID *v)
{
    std::string rtnName = RTN_Name(rtn);

    // Insert a call at the entry point of routines
    if (rtnName.find("add_mem") != std::string::npos) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)add_mem, 
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, 
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_END);
        RTN_Close(rtn);
    }

    if (rtnName.find("remove_mem") != std::string::npos) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)remove_mem,  
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, 
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_END);
        RTN_Close(rtn);
    }

    if (rtnName.find("set_enable_trace") != std::string::npos) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)set_enable_trace, 
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
        RTN_Close(rtn);
    }

    // printf("%s\n", RTN_Name(rtn).c_str());
}


/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize symbol table code, needed for rtn instrumentation
    PIN_InitSymbols();
    
    if (PIN_Init(argc, argv)) return Usage();
    
    trace = fopen("sel-pintrace.out", "w");
    enableTrace = false;

    // Register Routine to be called to instrument rtn
    RTN_AddInstrumentFunction(Routine, 0);

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
