#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>

using std::FILE;
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::ifstream;
using std::stringstream;
using std::vector;

// a class that saves the LRU count/tag/dirty/valid
// and if the data exists in  another cache it saves its placement
class CacheEntry
{
public:
    unsigned long int tag;
    bool dirty;
    bool valid;
    unsigned long int LRU;
    bool is_shared;
    unsigned long int shared_cache_set_index;
    unsigned long int shared_cache_way_index;

    // Constructor to initialize CacheEntry object
    CacheEntry() : tag(0), dirty(false), valid(false), LRU(0), is_shared(false), shared_cache_set_index(0), shared_cache_way_index(0) {}

    // Constructor with parameters to initialize CacheEntry object
    CacheEntry(unsigned long int initTag, bool initDirty, bool initValid, unsigned long int initLRU,
               bool initExistsInOtherCache, unsigned long int initSetInOtherCache, unsigned long int initWayInOtherCache)
            : tag(initTag), dirty(initDirty), valid(initValid), LRU(initLRU),
              is_shared(initExistsInOtherCache), shared_cache_set_index(initSetInOtherCache), shared_cache_way_index(initWayInOtherCache) {}
};

// a class that saves the sizes of the cache parameters and number of misses/hits
// and saves the different datas inside the cache
class Cache {
public:
    unsigned long int block_size;
    unsigned long int num_of_sets;
    unsigned long int num_of_ways;
    bool write_alloc;
    std::vector<std::vector<CacheEntry> > memory;


    // Constructor for initializing the cache
    Cache(unsigned long int BSize, unsigned long int LAssoc, unsigned long int LSize, bool WrAlloc)
            : block_size(pow(2,BSize)),
              num_of_ways(pow(2,LAssoc)),
              num_of_sets(pow(2,LSize - BSize - LAssoc)),
              write_alloc(WrAlloc)
              {
        initializeMemory();
    }

private:

    // Private function to initialize memory with default empty blocks
    void initializeMemory() {
        CacheEntry empty;
        empty.valid = false;
        empty.dirty = false;
        empty.LRU = 0;
        empty.is_shared = false;

        memory = std::vector<std::vector<CacheEntry> >(num_of_sets, std::vector<CacheEntry>(num_of_ways, empty));
    }
};
void updateOtherCacheState(Cache& L1, Cache& L2, unsigned long int L1_set, unsigned long int temp, unsigned long int L2_set) {
    if (L1.memory[L1_set][temp].valid) {
        unsigned long int other_set = L1.memory[L1_set][temp].shared_cache_set_index;
        unsigned long int other_way = L1.memory[L1_set][temp].shared_cache_way_index;

        L2.memory[other_set][other_way].is_shared = false;

        if (L1.memory[L1_set][temp].dirty) {
            unsigned long int old_LRU = L2.memory[other_set][other_way].LRU;
            L2.memory[other_set][other_way].LRU = L2.num_of_ways;

            for (unsigned long int j = 0; j < L2.num_of_ways; ++j) {
                if (L2.memory[L2_set][j].LRU > old_LRU && j != other_way)
                    L2.memory[L2_set][j].LRU--;
            }
        }
    }
}
void updateLRU(Cache& cache, unsigned long int set_index, unsigned long int excluded_way) {
    for (unsigned long int j = 0; j < cache.num_of_ways; ++j) {
        if (cache.memory[set_index][j].LRU > 0 && j != excluded_way) {
            (cache.memory[set_index][j].LRU)--;
        }
    }
}

void updateLocalCacheSet(Cache& localCache, unsigned long int set, unsigned long int way, const CacheEntry& data) {
    localCache.memory[set][way] = data;
    localCache.memory[set][way].LRU = localCache.num_of_ways;
    updateLRU(localCache, set, way);
}



int updateCache(Cache& cache, unsigned long int set, unsigned long int tag, char operation, bool& hit) {
    for (unsigned long int i = 0; i < cache.num_of_ways; ++i) {
        if (cache.memory[set][i].valid && cache.memory[set][i].tag == tag) {
            // Upon hit in the cache, update LRU and highlight as dirty if needed
            hit = true;
            if (operation == 'w')
                cache.memory[set][i].dirty = true;

            unsigned long int old_LRU = cache.memory[set][i].LRU;
            cache.memory[set][i].LRU = cache.num_of_ways;

            for (unsigned long int j = 0; j < cache.num_of_ways; ++j) {
                if (cache.memory[set][j].LRU > old_LRU && j != i)
                    cache.memory[set][j].LRU--;
            }
            return i;
        }
    }
    return -1;  // Return -1 if no hit is found
}
unsigned long int findAvailableWay(const Cache& cache, unsigned long int set) {
    for (unsigned long int j = 0; j < cache.num_of_ways; ++j) {
        if (!cache.memory[set][j].valid) {
            return j;
        }
    }

    for (unsigned long int j = 0; j < cache.num_of_ways; ++j) {
        if (cache.memory[set][j].LRU == 1) {
            return j;
        }
    }

    return 0;  // Default return value in case no available way is found
}


int main(int argc, char **argv) {
    if (argc < 19) {
        cerr << "Not enough arguments" << endl;
        return 0;
    }
     double misses1=0;
    double visits1=0;
    double misses2=0;
    double visits2=0;
    // Get input arguments

    // File
    // Assuming it is the first argument
    char* fileString = argv[1];
    ifstream file(fileString); //input file stream
    string line;
    if (!file || !file.good()) {
        // File doesn't exist or some other error
        cerr << "File not found" << endl;
        return 0;
    }

    unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
            L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

    for (int i = 2; i < 19; i += 2) {
        string s(argv[i]);
        if (s == "--mem-cyc") {
            MemCyc = atoi(argv[i + 1]);
        } else if (s == "--bsize") {
            BSize = atoi(argv[i + 1]);
        } else if (s == "--l1-size") {
            L1Size = atoi(argv[i + 1]);
        } else if (s == "--l2-size") {
            L2Size = atoi(argv[i + 1]);
        } else if (s == "--l1-cyc") {
            L1Cyc = atoi(argv[i + 1]);
        } else if (s == "--l2-cyc") {
            L2Cyc = atoi(argv[i + 1]);
        } else if (s == "--l1-assoc") {
            L1Assoc = atoi(argv[i + 1]);
        } else if (s == "--l2-assoc") {
            L2Assoc = atoi(argv[i + 1]);
        } else if (s == "--wr-alloc") {
            WrAlloc = atoi(argv[i + 1]);
        } else {
            cerr << "Error in arguments" << endl;
            return 0;
        }
    }
    if(L1Size < BSize + L1Assoc || L2Size < BSize + L2Assoc || WrAlloc > 1)
    {
        cerr << "Error in arguments" << endl;
        return 0;
    }

    // initializes both caches with the proper sizes settings (write alloc or no write alloc)
    Cache L1(BSize, L1Assoc, L1Size, WrAlloc);
    Cache L2(BSize, L2Assoc, L2Size, WrAlloc);

    while (getline(file, line)) {

        stringstream ss(line);
        string address;
        char operation = 0; // read (R) or write (W)
        if (!(ss >> operation >> address)) {
            // Operation appears in an Invalid format
            cout << "Command Format error" << endl;
            return 0;
        }
        string cutAddress = address.substr(2); // Removing the "0x" part of the address

        unsigned long int num ;
        num = strtoul(cutAddress.c_str(), NULL, 16);
        // calculate the set and
        // tag for each cache for the current address
        unsigned long int L1_set = (num / L1.block_size) % L1.num_of_sets;
        unsigned long int L2_set = (num / L2.block_size) % L2.num_of_sets;
        unsigned long int L1_tag = num / (L1.block_size * L1.num_of_sets);
        unsigned long int L2_tag = num / (L2.block_size * L2.num_of_sets);
        bool L1_hit = false;
        bool L2_hit = false;

        (visits1)++;

        updateCache(L1, L1_set, L1_tag, operation, L1_hit);
        if(L1_hit)
            continue;
        //if L1 hit move on to next address otherwise update misses in L1, visit L2 and check if hits
        (misses1)++;
        (visits2)++;

        int i= updateCache(L2, L2_set, L2_tag, operation, L2_hit);
        if(L2_hit)
        {

            if(operation == 'w' && !L2.write_alloc)
            {
                (L2.memory[L2_set][i]).dirty = true;
            }
            else {
                CacheEntry L1_data(L1_tag, (operation == 'w'), true, 0, true, L2_set, i);
                unsigned long int temp = findAvailableWay(L1, L1_set);
                //if the data in L1 replaced another data dont forget to update the data in L2 so it
                //knows that the data no longer exists in another cache and update the data in L2 and
                //the LRU if data in L1 is dirty
                updateOtherCacheState(L1, L2, L1_set, temp, L2_set);
                L1.memory[L1_set][temp] = L1_data;
                L1.memory[L1_set][temp].LRU = L1.num_of_ways;
                updateLRU(L1, L1_set, temp);

                //make L2 save the placement of R1
                L2.memory[L2_set][i].is_shared = true;
                L2.memory[L2_set][i].shared_cache_set_index = L1_set;
                L2.memory[L2_set][i].shared_cache_way_index = temp;
            }
        }


        if(L2_hit)
            continue;

        //if L2 hit move on to next address otherwise update misses in L2 and if operation is write with
        //no write alloc then move on to next address otherwise add data to both L2 and L1
        (misses2)++;
        if(operation == 'w' && !L2.write_alloc)
            continue;
        CacheEntry L2_data(L2_tag, false, true, 0, true, L1_set, 0);
        unsigned long int temp = 0;
        temp = findAvailableWay(L2, L2_set);
        //when adding data to L2 if the data replaced a data that existed also in L1 then because of exclusiveness
        //we have to remove it from L1 and add the new data in its place
        bool exist_in_L1 = false;
        unsigned long int way_in_L1 ;
        if(L2.memory[L2_set][temp].is_shared)
        {
            exist_in_L1 = true;
            way_in_L1 = L2.memory[L2_set][temp].shared_cache_way_index;
        }
        L2.memory[L2_set][temp] = L2_data;
        L2.memory[L2_set][temp].LRU = L2.num_of_ways;
        updateLRU(L2,L1_set,temp);
        if(operation == 'w' && !L2.write_alloc)
            break;
        // Create a new CacheEntry object for L1
        CacheEntry L1_data(L1_tag, (operation == 'w'), true, 0, true, L2_set, temp);
        if (exist_in_L1) {
            L1.memory[L1_set][way_in_L1] = L1_data;
            L2.memory[L2_set][temp].shared_cache_way_index = way_in_L1;
        } else {
            way_in_L1 = findAvailableWay(L1, L1_set);
            updateOtherCacheState(L1, L2, L1_set, way_in_L1, L2_set);
            updateLocalCacheSet(L1, L1_set, way_in_L1, L1_data);
            L2.memory[L2_set][temp].shared_cache_way_index = way_in_L1;
        }
    }
    //calculate miss rate for each cache and calculate the avg time using them and then print conclusions
    double L1MissRate = (double)misses1 / (double)visits1;
    double L2MissRate = (double)misses2 / (double)visits2;
    double avgAccTime = (double)L1Cyc + L1MissRate * (double)L2Cyc + L1MissRate * L2MissRate * (double)MemCyc;
    printf("L1miss=%.03f ", L1MissRate);
    printf("L2miss=%.03f ", L2MissRate);
    printf("AccTimeAvg=%.03f\n", avgAccTime);

    return 0;
}
