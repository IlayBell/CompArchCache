#ifndef __CACHE__
#define __CACHE__

#include <vector>
#include <string>
#include <cstdint>

#define ALIGNMENT_OFFSET 2
#define ADDRESS_SIZE 64

class Block;
class Set;
class CacheLevel;
class L1;
class L2;
struct CacheManager;

class Block {
    bool valid;
    bool dirty;
    uint64_t tag;
    int LRU_order;
    uint64_t addr_aligned;

    public:
        Block();
        void set_block(uint64_t tag);
        bool get_valid();
        bool get_dirty();
        void set_valid(bool valid);
        void set_dirty(bool dirty);
        bool compare_tag(uint64_t other);
        void set_LRU_order(int order);
        int get_LRU_order();
        void fill(uint64_t tag, uint64_t addr_aligned);
        uint64_t get_tag();
        uint64_t get_addr_aligned();
        void set_addr_aligned(uint64_t addr_aligned);
};

class Set {
    int set_num;
    std::vector<Block> ways;
    int ways_num;

    public:
        Set(int ways_num, int set_num);
        void set_set_num(int set_num);
        void insert(uint64_t tag);
        bool is_exist(uint64_t tag);
        void extract();
        Block& get_block_at_idx(int idx);
        Block* get_block_by_tag(uint64_t tag);
        Block* get_available_block();
        Block* release(CacheLevel* cache, CacheManager* cache_manager);
        Block& get_LRU_block();
};

class CacheLevel {
    // Used for stats
    protected:
        int miss;
        int access;
        int access_time;

        int block_num;
        int ways_num;
        int sets_num;

        std::vector<Set> sets;

    public:
        CacheLevel(int cache_size, int block_size, int ways_num, int access_time);
        int set_num_bits();
        bool is_exist_in_set(uint64_t set, uint64_t tag);
        void add_access();
        void add_miss();
        void update_LRU(uint64_t set, uint64_t tag);
        void write_data(uint64_t set, uint64_t tag);
        void propogate_block(uint64_t set, uint64_t tag, uint64_t addr_aligned, CacheManager* cache_manager);
        Block* get_block_in_set(int set_num, uint64_t tag);

        int get_miss();
        int get_access();
        double calc_miss_rate();
        int get_access_time();

        virtual void evac_block(Block& block, CacheManager* cache_manager) = 0;

        virtual ~CacheLevel() = default;
};

class L1 : public CacheLevel {
    public:
        L1(int cache_size, int block_size, int ways_num, int access_time);
        void evac_block(Block& block, CacheManager* cache_manager);
};

class L2 : public CacheLevel {
    public:
        L2(int cache_size, int block_size, int ways_num, int access_time);
        void evac_block(Block& block, CacheManager* cache_manager);
};

struct CacheManager {
    L1 l1;
    L2 l2;

    int mem_cycle;
    bool is_write_allocate;
    int block_size;
    
    CacheManager(int mem_cycle, int block_size_log, bool is_write_allocate,
                 int l1_size_log, int l1_ways_log, int l1_access_time,
                 int l2_size_log, int l2_ways_log, int l2_access_time);
    
    void read(uint64_t addr);
    void write(uint64_t addr);

    void calc_stats(double& L1_miss_rate, double& L2_miss_rate, double& avg_acc_time);
};

void extract_bits(uint64_t num, int lsb_size, uint64_t& lsb, uint64_t& msb);

#endif