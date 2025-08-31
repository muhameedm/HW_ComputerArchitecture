#include "core_api.h"
#include "sim_api.h"
#include <stdio.h>
#include <vector>
using std::vector;

//struct to sava data
typedef struct _MTInfo {
    vector<vector<int>> registers;
    int instructions_num;
    int cycles_num;
} MTInfo;

MTInfo blocked_mt_info;
MTInfo fine_grained_mt_info;
void update_threadInfo(MTInfo &info,int thread_id,int curr_line[]){
    info.instructions_num++;
    info.cycles_num++;
    curr_line[thread_id] += 1;
    return;
}
void ProcessInstruction(cmd_opcode curr_opcode, int curr_dst,int curr_src1,int curr_src2,bool _is_curr_src2_imm,MTInfo &info,int curr_thread,int curr_line[],int cycles_to_wait[], int load_latency,
int store_latency){
    switch (curr_opcode) {
        case CMD_ADD:
            info.registers[curr_thread][curr_dst] = info.registers[curr_thread][curr_src1] +
                                                               info.registers[curr_thread][curr_src2];
            update_threadInfo(info,curr_thread,curr_line);
            break;
        case CMD_ADDI:
            info.registers[curr_thread][curr_dst] = info.registers[curr_thread][curr_src1] +
                                                               curr_src2;
            update_threadInfo(info,curr_thread,curr_line);

            break;
        case CMD_SUB:
            info.registers[curr_thread][curr_dst] = info.registers[curr_thread][curr_src1] -
                                                               info.registers[curr_thread][curr_src2];
            update_threadInfo(info,curr_thread,curr_line);

            break;
        case CMD_SUBI:
            info.registers[curr_thread][curr_dst] = info.registers[curr_thread][curr_src1] -
                                                               curr_src2;
            update_threadInfo(info,curr_thread,curr_line);

            break;
        case CMD_HALT:
            cycles_to_wait[curr_thread] = -1;
            info.instructions_num++;
            info.cycles_num++;
            break;
        case CMD_LOAD:
            if(_is_curr_src2_imm)
            {
                SIM_MemDataRead(info.registers[curr_thread][curr_src1] + curr_src2,
                                &info.registers[curr_thread][curr_dst]);
            }
            else
            {
                SIM_MemDataRead(info.registers[curr_thread][curr_src1] +
                                info.registers[curr_thread][curr_src2],
                                &info.registers[curr_thread][curr_dst]);
            }
            cycles_to_wait[curr_thread] = load_latency + 1;
            update_threadInfo(info,curr_thread,curr_line);
            break;
        case CMD_STORE:
            if(_is_curr_src2_imm)
            {
                SIM_MemDataWrite(info.registers[curr_thread][curr_dst] + curr_src2,
                                 info.registers[curr_thread][curr_src1]);
            }
            else
            {
                SIM_MemDataWrite(info.registers[curr_thread][curr_dst] +
                                 info.registers[curr_thread][curr_src2],
                                 info.registers[curr_thread][curr_src1]);
            }
            cycles_to_wait[curr_thread] = store_latency + 1;
            update_threadInfo(info,curr_thread,curr_line);
            break;
        default:
            info.cycles_num++;
    }
}
void init_func(int threads_num ,int cycles_to_wait[],int curr_line[],MTInfo &info){
    info.cycles_num = 0;
    info.instructions_num = 0;
    info.registers.resize(threads_num, vector<int>(REGS_COUNT, 0));
    for (int i = 0; i < threads_num; ++i) {
        cycles_to_wait[i] = 0;
    }
    for (int i = 0; i < threads_num; ++i) {
        curr_line[i] = 0;
    }

}
bool check_thread_finish(int threads_num,int cycles_to_wait[]){
    for (int i = 0; i < threads_num; ++i) {
        if(cycles_to_wait[i] != -1)
        {
            if(i == threads_num){
                return true;
            }
        return false;
        }
    }
}
void update_cycle_thread(int threads_num,int cycles_to_wait[]){
    for (int i = 0; i < threads_num; ++i) {
        if(cycles_to_wait[i] > 0)
            cycles_to_wait[i]--;
    }
}
void CORE_BlockedMT() {
    //reading image data (latencies/num of threads/ cycles needed for switch)
    int load_latency = SIM_GetLoadLat();
    int store_latency = SIM_GetStoreLat();
    int switch_cycles = SIM_GetSwitchCycles();
    int threads_num = SIM_GetThreadsNum();
    int cycles_to_wait[threads_num];
    int curr_thread = 0;
    int curr_line[threads_num];
    init_func(threads_num,cycles_to_wait,curr_line,blocked_mt_info);
    Instruction curr_instruction;
    SIM_MemInstRead(0, &curr_instruction, 0);
    while (true)
    {
        //getting instruction arguments
        cmd_opcode curr_opcode = curr_instruction.opcode;
        int curr_dst = curr_instruction.dst_index;
        int curr_src1 = curr_instruction.src1_index;
        int curr_src2 = curr_instruction.src2_index_imm;
        bool _is_curr_src2_imm = curr_instruction.isSrc2Imm;
        //changing data/registers if needed depending on the opcode in addition to changing num of instruction
        //and cycles and if needed changing cycles waiting
        ProcessInstruction(curr_opcode,curr_dst,curr_src1,curr_src2,_is_curr_src2_imm,blocked_mt_info,curr_thread,curr_line,cycles_to_wait,load_latency,store_latency);
        //check if all threads finished and then end the function
        if(check_thread_finish(threads_num,cycles_to_wait)){
            break;
        }
        int i;
        update_cycle_thread(threads_num,cycles_to_wait);
        //look for ready instruction using RR (current thread have priority)
        for (i = 0; i < threads_num; ++i) {
            //update thread and instruction
            if(cycles_to_wait[(curr_thread + i) % threads_num] == 0)
            {
                SIM_MemInstRead(curr_line[(curr_thread + i) % threads_num], &curr_instruction,
                                (curr_thread + i) % threads_num);
                curr_thread = (curr_thread + i) % threads_num;
                //pay for the needed cycles to switch thread and reduce the waiting thread's waiting cycles
                if(i != 0)
                {
                    blocked_mt_info.cycles_num += switch_cycles;
                    for (int j = 0; j < threads_num; ++j) {
                        if(cycles_to_wait[j] > 0)
                            cycles_to_wait[j] = (cycles_to_wait[j] > switch_cycles) ?
                                                cycles_to_wait[j] - switch_cycles : 0;
                    }
                }
                break;
            }
        }
        //if no thread is ready then update opcode to NOP
        if(i == threads_num)
        {
            curr_instruction.opcode = CMD_NOP;
        }
    }
}

void CORE_FinegrainedMT() {
    //reading image data (latencies/num of threads/ cycles needed for switch)
    int load_latency = SIM_GetLoadLat();
    int store_latency = SIM_GetStoreLat();
    int threads_num = SIM_GetThreadsNum();
    int cycles_to_wait[threads_num];
    int curr_thread = 0;
    int curr_line[threads_num];
    init_func(threads_num,cycles_to_wait,curr_line,fine_grained_mt_info);

    Instruction curr_instruction;
    SIM_MemInstRead(0, &curr_instruction, 0);
    while (true)
    {
        //getting instruction arguments
        cmd_opcode curr_opcode = curr_instruction.opcode;
        int curr_dst = curr_instruction.dst_index;
        int curr_src1 = curr_instruction.src1_index;
        int curr_src2 = curr_instruction.src2_index_imm;
        bool _is_curr_src2_imm = curr_instruction.isSrc2Imm;
        //changing data/registers if needed depending on the opcode in addition to changing num of instruction
        //and cycles and if needed changing cycles waiting
        ProcessInstruction(curr_opcode,curr_dst,curr_src1,curr_src2,_is_curr_src2_imm,fine_grained_mt_info,curr_thread,curr_line,cycles_to_wait,load_latency,store_latency);
        //check if all threads finished and then end the function
        int i;
        if(check_thread_finish(threads_num,cycles_to_wait)){
            break;
        }
        update_cycle_thread(threads_num,cycles_to_wait);
        //look for ready instruction using RR (next thread have priority)
        for (i = 1; i <= threads_num; ++i) {
            if(cycles_to_wait[(curr_thread + i) % threads_num] == 0)
            {
                SIM_MemInstRead(curr_line[(curr_thread + i) % threads_num], &curr_instruction,
                                (curr_thread + i) % threads_num);
                curr_thread = (curr_thread + i) % threads_num;
                break;
            }
        }
        //if no thread is ready then update opcode to NOP
        if(i == threads_num + 1)
            curr_instruction.opcode = CMD_NOP;
    }
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
    for (int i = 0; i < REGS_COUNT; ++i) {
        context[threadid].reg[i] = fine_grained_mt_info.registers[threadid][i];
    }
}

double CORE_BlockedMT_CPI(){
    return (double)blocked_mt_info.cycles_num / (double)blocked_mt_info.instructions_num;
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
    for (int i = 0; i < REGS_COUNT; ++i) {
        context[threadid].reg[i] = blocked_mt_info.registers[threadid][i];
    }
}

double CORE_FinegrainedMT_CPI(){
    return (double)fine_grained_mt_info.cycles_num / (double)fine_grained_mt_info.instructions_num;
}
