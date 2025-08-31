#include "dflow_calc.h"

 struct progctx{
     unsigned int* i_latency;
    int r_d[32];
    int* s1_dependency;
    int* s2_dependency;
     unsigned int num_of_insts;
     bool check;
     int* bigger;


};

ProgCtx analyzeProg(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts) {
    unsigned int s1_t;
    unsigned int s2_t;
    progctx* p_re = new progctx;
    for (int i = 0; i < 32; ++i) {
        p_re->r_d[i] = -1;
    }
    p_re->s1_dependency = new int[numOfInsts];
    p_re->s2_dependency = new int[numOfInsts];
    p_re->i_latency = new unsigned int[numOfInsts + 1];

    p_re->num_of_insts = numOfInsts;

    //running over Insts and updating dependencies and latencies
    unsigned int i = 0;
    while (i < numOfInsts) {
        int s1_dependency = p_re->r_d[progTrace[i].src1Idx];
        int s2_dependency = p_re->r_d[progTrace[i].src2Idx];

        // Update dependencies
        p_re->s1_dependency[i] = s1_dependency;
        p_re->s2_dependency[i] = s2_dependency;

        // Compute latencies
        unsigned int s1_t = (s1_dependency != -1) ? (p_re->i_latency[s1_dependency] + opsLatency[progTrace[s1_dependency].opcode]) : 0;
        unsigned int s2_t = (s2_dependency != -1) ? (p_re->i_latency[s2_dependency] + opsLatency[progTrace[s2_dependency].opcode]) : 0;

        // Determine the maximum latency
        unsigned int max_latency = (s1_t > s2_t) ? s1_t : s2_t;

        // Update latency and dependency records
        p_re->i_latency[i] = max_latency;
        p_re->r_d[progTrace[i].dstIdx] = i;

        ++i;
    }

    // Updating Exit latency as the max latency
    p_re->i_latency[numOfInsts] = 0;
    i = 0;
    while (i < numOfInsts) {
        unsigned int current_latency = p_re->i_latency[i] + opsLatency[progTrace[i].opcode];
        if (current_latency > p_re->i_latency[numOfInsts]) {
            p_re->i_latency[numOfInsts] = current_latency;
        }
        ++i;
    }

    return (void*)p_re;



void freeProgCtx(ProgCtx ctx) {
    delete [] ((progctx*)ctx)->s1_dependency;
    delete [] ((progctx*)ctx)->s2_dependency;
    delete [] ((progctx*)ctx)->i_latency;
    delete ((progctx*)ctx);
}

int getInstDepth(ProgCtx ctx, unsigned int theInst) {
    return ((progctx*)ctx)->i_latency[theInst];
}

int getInstDeps(ProgCtx ctx, unsigned int theInst, int *src1DepInst, int *src2DepInst) {
    *src1DepInst = ((progctx*)ctx)->s1_dependency[theInst];
    *src2DepInst = ((progctx*)ctx)->s2_dependency[theInst];
    return 0;
}

int getProgDepth(ProgCtx ctx) {
    return ((progctx*)ctx)->i_latency[((progctx*)ctx)->num_of_insts];
}
