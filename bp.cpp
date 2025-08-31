#include "bp_api.h"
#include <vector>
#include <cmath>
/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */
using namespace std;
enum Share_way {not_using_share, using_share_lsb, using_share_mid};
enum State {SNT, WNT, WT, ST};
class Branch_Predictor{
public:
    unsigned btbSize;
    unsigned historySize;
    unsigned tagSize;
    State default_state;
    bool isGlobalHist;
    bool isGlobalTable;
    Share_way Shared;
    SIM_stats stats;
    vector<bool> validBits;
    vector<uint32_t> tags;
    vector<uint32_t> addresses;
    void setParameters(unsigned btbSize_, unsigned historySize_, unsigned tagSize_,
                       unsigned default_state_, bool isGlobalHist_, bool isGlobalTable_,
                       int Shared_) {
        btbSize = btbSize_;
        historySize = historySize_;
        tagSize = tagSize_;
        default_state =static_cast<State>(default_state_);
        isGlobalHist = isGlobalHist_;
        isGlobalTable = isGlobalTable_;
        Shared = static_cast<Share_way>(Shared_);
        tags=vector<uint32_t>(btbSize, 0);
        validBits=vector<bool>(btbSize, false);
        addresses=vector<uint32_t>(btbSize, 0);
        stats.size=btbSize_*(tagSize_+31);
        stats.br_num=0;
        stats.flush_num=0;
    }
};
class Tables:public Branch_Predictor{
public:
    vector<vector<State> > local_table;
    vector<State> global_table;
    vector<unsigned> local_history;
    unsigned global_history;
    void setTablesbytype(bool isGlobalHist, bool isGlobalTable,unsigned fsmState){
        if(isGlobalHist)
        {
            global_history = 0;
            local_history = vector<unsigned>(0);
            stats.size += historySize;
        }
        else
        {
            global_history = 0;
            local_history = vector<unsigned>(btbSize, 0);
            stats.size += btbSize * historySize;
        }
        if(isGlobalTable)
        {
            local_table = vector<vector<State> >(0);
            global_table = vector<State>(pow(2,historySize), State(fsmState));
            stats.size +=  pow(2,historySize+1);
        }
        else
        {
            global_table = vector<State>(0);
            local_table = vector<vector<State> >(btbSize, vector<State>(pow(2,historySize), State(fsmState)));
            stats.size += btbSize  * pow(2,historySize+1);
        }
    }
};
Tables bp;
bool input_validity(unsigned btbSize, unsigned historySize,unsigned tagSize,unsigned fsmState,int Shared){
    if(btbSize != 1 && btbSize != 2 && btbSize != 4 && btbSize != 8 && btbSize != 16 && btbSize != 32)
        return false;
    if(historySize > 8 || historySize < 1)
        return false;
    if(fsmState > 3)
        return false;
    if(Shared < 0 || Shared > 2)
        return false;
    if(tagSize > 30 - log2(btbSize))
        return false;
    return true;
}
void change_bpstate(uint32_t pc_tag,uint32_t pc_position,uint32_t targetPc){
	 bp.validBits[pc_position] = true;

        bp.tags[pc_position] = pc_tag;
        bp.addresses[pc_position] = targetPc;
        if(!bp.isGlobalTable)
            bp.local_table[pc_position] = vector<State>(pow(2,bp.historySize), State(bp.default_state));
        if(!bp.isGlobalHist)
            bp.local_history[pc_position] = 0;



}
void updateTable(vector<State>& table,size_t index,bool taken){
	State& entry=table[index];
	if(entry != ST && taken){	
	entry=static_cast<State>(static_cast<int>(entry)+1);
	} 
	else if(entry != SNT && !taken){
	entry=static_cast<State>(static_cast<int>(entry)-1);
	}
}

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
            bool isGlobalHist, bool isGlobalTable, int Shared){
    if(!input_validity( btbSize,  historySize, tagSize, fsmState, Shared))
        return -1;
    bp.setParameters(btbSize,historySize,tagSize,
            fsmState,isGlobalHist,isGlobalTable,
            Shared);
    bp.setTablesbytype(isGlobalHist,isGlobalTable,fsmState);
    return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
    uint32_t pc_position = (pc >> 2) % bp.btbSize;
    uint32_t pc_tag = ((pc >> 2) / bp.btbSize) % (1<<(bp.tagSize));
    if(!bp.validBits[pc_position] || bp.tags[pc_position] != pc_tag)
    {
        *dst = pc + 4;
        return false;
    }
 
    uint32_t doxor = 0;
    if (bp.isGlobalTable)
    {
        if(bp.Shared == using_share_lsb)
            doxor = (pc >> 2) % (1<<(bp.historySize));
        if(bp.Shared == using_share_mid)
            doxor = (pc >> 16) %(1<<(bp.historySize));
	
	if(bp.isGlobalHist)
	{
		if(bp.global_table[bp.global_history ^ doxor] == WT ||
                bp.global_table[bp.global_history ^ doxor] == ST)
       		 {
           		 *dst = bp.addresses[(pc>>2)%bp.btbSize];
          		  return true;
        	}
   
	}
	else{
	if(bp.global_table[bp.local_history[pc_position] ^ doxor] == WT ||
          bp.global_table[bp.local_history[pc_position] ^ doxor] == ST)
        {
            *dst = bp.addresses[(pc>>2)%bp.btbSize];
            return true;
        }
    
	}
 	

    }


    if(bp.isGlobalHist && !bp.isGlobalTable)
    {
        if(bp.local_table[pc_position][bp.global_history] == WT ||
                bp.local_table[pc_position][bp.global_history] == ST)
        {
            *dst = bp.addresses[(pc>>2)%bp.btbSize];
            return true;
        }        
    }
   if(!(bp.isGlobalHist) && !(bp.isGlobalTable)){
  	if(bp.local_table[pc_position][bp.local_history[pc_position]] == WT ||
                bp.local_table[pc_position][bp.local_history[pc_position]] == ST)
        {
            *dst = bp.addresses[(pc>>2)%bp.btbSize];
            return true;
        }     
	}
    *dst = pc + 4;
    return false;
       

}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
    
    bp.stats.br_num++;
    
    uint32_t pc_position = (pc >> 2) % bp.btbSize;
    uint32_t pc_tag = ((pc >> 2) / bp.btbSize) % static_cast<unsigned>(pow(2,bp.tagSize));


	bp.addresses[pc_position] = targetPc;
	
     if(((taken && pred_dst != targetPc)
|| (!taken && pred_dst != pc + 4)) )
        bp.stats.flush_num++;
    if(!bp.validBits[pc_position] || bp.tags[pc_position] != pc_tag)
    {
	change_bpstate(pc_tag,pc_position,targetPc);
       
    }
   
    uint32_t doxor = 0;
    if (bp.isGlobalTable)
    {
        if(bp.Shared == using_share_lsb)
            doxor = (pc >> 2) % (1<<(bp.historySize));
        if(bp.Shared == using_share_mid)
            doxor = (pc >> 16) % (1<<(bp.historySize));
        
        if(bp.isGlobalHist)
        {
         
		updateTable(bp.global_table,bp.global_history ^ doxor,taken);
              
            bp.global_history = 2 * (bp.global_history % static_cast<unsigned>(pow(2,bp.historySize - 1) ) ) + taken;
            return;
        }
    }


    if(bp.isGlobalHist)
    {
	updateTable(bp.local_table[pc_position],bp.global_history,taken);
     
        bp.global_history = 2 * (bp.global_history % static_cast<unsigned>(pow(2,bp.historySize - 1))) + taken;
        return;
    }
    if(bp.isGlobalTable)
    {
	updateTable(bp.global_table,bp.local_history[pc_position] ^ doxor,taken);
      
        bp.local_history[pc_position] %= static_cast<unsigned>(pow(2,(bp.historySize - 1)));
        bp.local_history[pc_position] = 2 * bp.local_history[pc_position] + taken;
        return;
    }
    updateTable(bp.local_table[pc_position],bp.local_history[pc_position],taken);
    bp.local_history[pc_position] %= static_cast<unsigned>(pow(2,(bp.historySize - 1)));
    bp.local_history[pc_position] = 2 * bp.local_history[pc_position] + taken;
    return;
}

void BP_GetStats(SIM_stats *curStats){
    curStats->flush_num=bp.stats.flush_num;
    curStats->size=bp.stats.size;
    curStats->br_num=bp.stats.br_num;
    return;
}

