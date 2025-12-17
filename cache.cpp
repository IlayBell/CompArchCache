#include "cache.h"
#include <iostream>

int pow2(int exp) {
    return 1 << exp;
}

int log2(int x) {
    int res = 0;
    while (x >>= 1) {
        res++;
    }

    return res;
}


CacheManager::CacheManager(int mem_cycle, 
                           int block_size_log,
                           bool is_write_allocate,
                           int l1_size_log, 
                           int l1_ways_log, 
                           int l1_access_time,
                           int l2_size_log, 
                           int l2_ways_log, 
                           int l2_access_time) : 

                           l1(pow2(l1_size_log),
                           pow2(block_size_log),
                           pow2(l1_ways_log),
                           l1_access_time), 

                           l2(pow2(l2_size_log),
                           pow2(block_size_log),
                           pow2(l2_ways_log),
                           l2_access_time) {

    this->mem_cycle = mem_cycle;
    this->is_write_allocate = is_write_allocate;
    this->block_size = pow2(block_size_log);
    
}

CacheLevel::CacheLevel(int cache_size, int block_size, int ways_num, int access_time) :
        sets((cache_size / block_size) / ways_num, Set(ways_num, 0)) {

    this->block_num = cache_size / block_size;
    this->ways_num = ways_num;
    this->sets_num = this->block_num / this->ways_num;

    this->miss = 0;
    this->access = 0;
    this->access_time = access_time;

    for(int i = 0; i < this->sets_num; i++) {
        Set& curr_set = this->sets.at(i);
        curr_set.set_set_num(i);
    } 
}



Set::Set(int ways_num, int set_num) : ways(ways_num) {
    this->ways_num = ways_num;
    this->set_num = set_num;
}

Block::Block() {
    this->valid = false;
    this->dirty = false;
    this->tag = 0;
    this->LRU_order = 0;
    this->addr_aligned = 0;
}

bool Block::get_valid() {
    return this->valid;
}

bool Block::get_dirty() {
    return this->dirty;
}

void Block::set_valid(bool valid) {
    this->valid = valid;
}

void Block::set_dirty(bool dirty) {
    this->dirty = dirty;
}

bool Block::compare_tag(uint64_t other) {
    return this->tag == other;
}

uint64_t Block::get_addr_aligned() {
    return this->addr_aligned;
}

void Block::set_addr_aligned(uint64_t addr_aligned) {
    this->addr_aligned = addr_aligned;
}

void Block::fill(uint64_t tag, uint64_t addr_aligned) {
    this->tag = tag;
    this->valid = true;
    this->dirty = false;
    this->LRU_order = 0; 
    this->addr_aligned = addr_aligned;
}

void CacheManager::read(uint64_t addr) {
    addr >>= log2(this->block_size);
    uint64_t set1 = 0;
    uint64_t tag1 = 0;
    uint64_t set2 = 0;
    uint64_t tag2 = 0;

    extract_bits(addr, this->l1.set_num_bits(), set1, tag1);
    extract_bits(addr, this->l2.set_num_bits(), set2, tag2);

    this->l1.add_access();
    if (this->l1.is_exist_in_set(set1, tag1)) {
        this->l1.update_LRU(set1, tag1);
    } else {
        this->l1.add_miss();

        this->l2.add_access();
        if (this->l2.is_exist_in_set(set2, tag2)) {
            this->l2.update_LRU(set2, tag2);
        } else {
            this->l2.add_miss();

            this->l2.propogate_block(set2, tag2, addr, this);
        }

        this->l1.propogate_block(set1, tag1, addr, this);
        
        Block* new_l1_block = this->l1.get_block_in_set(set1, tag1);
        Block* new_l2_block = this->l2.get_block_in_set(set2, tag2);
        if (new_l1_block && new_l2_block) {
            new_l1_block->set_dirty(new_l2_block->get_dirty());
        } else {
            std::cout << "hamatzav bakantim!!" << std::endl;
        }
    }

}

void CacheLevel::propogate_block(uint64_t set,
                             uint64_t tag,
                             uint64_t addr_aligned,
                             CacheManager* cache_manager) {
    Set& curr_set = this->sets.at(set);
    Block* empty_block = curr_set.get_available_block();

    if (!empty_block) {
        empty_block = curr_set.release(this, cache_manager);
    }

    empty_block->fill(tag, addr_aligned);
    this->update_LRU(set, tag);
}

Block* Set::release(CacheLevel* cache, CacheManager* cache_manager) {
    Block& lru_block = this->get_LRU_block();
    cache->evac_block(lru_block, cache_manager);

    return &lru_block;
}

Block& Set::get_LRU_block() {
    Block* max_block = &this->ways.at(0);

    for (int i = 1; i < this->ways_num; i++) {
        Block* curr_block = &this->ways.at(i);

        if(max_block->get_LRU_order() < curr_block->get_LRU_order()) {
            max_block = curr_block;
        }
    }

    return *max_block;
}

void L1::evac_block(Block& block, CacheManager* cache_manager) {
    uint64_t set2 = 0;
    uint64_t tag2 = 0;

    extract_bits(block.get_addr_aligned(), cache_manager->l2.set_num_bits(), set2, tag2);

    Block* l2_block = cache_manager->l2.get_block_in_set(set2, tag2);

    if (l2_block) {
        l2_block->set_dirty(block.get_dirty());
    } 

    block.set_valid(false);
}

void L2::evac_block(Block& block, CacheManager* cache_manager) {
    block.set_valid(false);

    uint64_t set1 = 0;
    uint64_t tag1 = 0;

    extract_bits(block.get_addr_aligned(), cache_manager->l1.set_num_bits(), set1, tag1);

    Block* l1_block = cache_manager->l1.get_block_in_set(set1, tag1);

    if (l1_block) {
        l1_block->set_valid(false);
    }

}

void Set::set_set_num(int i) {
    this->set_num = i;
}

Block* CacheLevel::get_block_in_set(int set_num, uint64_t tag) {
    Set& curr_set = this->sets.at(set_num);

    return curr_set.get_block_by_tag(tag);
}

Block* Set::get_block_by_tag(uint64_t tag) {
    for (int i = 0; i < this->ways_num; i++) {
        Block& curr_block = this->ways.at(i);
        if (curr_block.get_valid() && curr_block.compare_tag(tag)) {
            return &curr_block;
        }
    }

    return nullptr;
}

uint64_t Block::get_tag() {
    return this->tag;
}

Block* Set::get_available_block() {
    for (int i = 0; i < this->ways_num; i++) {
        Block* curr_block = &this->ways.at(i);
        if (!curr_block->get_valid()) {
            return curr_block;
        }
    }

    return nullptr;
}

void CacheLevel::add_access() {
    this->access++;
}

void CacheLevel::add_miss() {
    this->miss++;
}

void CacheLevel::update_LRU(uint64_t set, uint64_t tag) {
    Set& curr_set = this->sets.at(set);
    for (int i = 0; i < this->ways_num; i++) {
        Block& curr_block = curr_set.get_block_at_idx(i);
        if (curr_block.get_valid()) {
            if (curr_block.compare_tag(tag)) {
                curr_block.set_LRU_order(0);
            } else {
                curr_block.set_LRU_order(curr_block.get_LRU_order() + 1);
            }
        }
    }
}

Block& Set::get_block_at_idx(int idx) {
    return this->ways.at(idx);
}

void Block::set_LRU_order(int order) {
    this->LRU_order = order;
}

int Block::get_LRU_order() {
    return this->LRU_order;
}

bool CacheLevel::is_exist_in_set(uint64_t set, uint64_t tag) {
    Set& curr_set = this->sets.at(set);
    for (int i = 0; i < this->ways_num; i++) {
        Block& curr_block = curr_set.get_block_at_idx(i);
        if (curr_block.get_valid() && curr_block.compare_tag(tag)) {
            return true;
        }
    }

    return false;
}

void CacheManager::write(uint64_t addr) {
    addr >>= log2(this->block_size);
    uint64_t set1 = 0;
    uint64_t tag1 = 0;
    uint64_t set2 = 0;
    uint64_t tag2 = 0;

    extract_bits(addr, this->l1.set_num_bits(), set1, tag1);
    extract_bits(addr, this->l2.set_num_bits(), set2, tag2);

    this->l1.add_access();
    if (this->l1.is_exist_in_set(set1, tag1)) {
        this->l1.write_data(set1, tag1);
        this->l1.update_LRU(set1, tag1);
    } else {
        this->l1.add_miss();

        this->l2.add_access();
        if (this->l2.is_exist_in_set(set2, tag2)) {
            if(this->is_write_allocate) {
                this->l1.propogate_block(set1, tag1, addr, this);
                this->l1.write_data(set1, tag1);
                this->l1.update_LRU(set1, tag1);
            } else {
                this->l2.write_data(set2, tag2);
            }

            this->l2.update_LRU(set2, tag2);

        } else {
            this->l2.add_miss();

            if(this->is_write_allocate) {
                this->l2.propogate_block(set2, tag2, addr, this);
                this->l2.update_LRU(set2, tag2);

                this->l1.propogate_block(set1, tag1, addr, this);
                this->l1.write_data(set1, tag1);
                this->l1.update_LRU(set1, tag1);
            }
        }
    }
}

void CacheLevel::write_data(uint64_t set,
                             uint64_t tag) {
    Set& curr_set = this->sets.at(set);
    Block* curr_block = curr_set.get_block_by_tag(tag);

    if (curr_block) {
        curr_block->set_dirty(true);
        return;
    }
}

int CacheLevel::set_num_bits() {
    return log2(this->sets_num);
}

void extract_bits(uint64_t num, int lsb_size, uint64_t& lsb, uint64_t& msb) {
    uint64_t msb_mask = -1 << lsb_size;
    lsb = num & (~msb_mask);
    msb = (num & msb_mask) >> lsb_size;
}

L1::L1(int cache_size, int block_size, int ways_num, int access_time)
    : CacheLevel(cache_size, block_size, ways_num, access_time) {};

L2::L2(int cache_size, int block_size, int ways_num, int access_time)
    : CacheLevel(cache_size, block_size, ways_num, access_time) {};

int CacheLevel::get_miss() {
    return this->miss;
}

int CacheLevel::get_access() {
    return this->access;
}

double CacheLevel::calc_miss_rate() {
    return ((double)this->miss)/this->access;
}

int CacheLevel::get_access_time() {
    return this->access_time;
}

void CacheManager::calc_stats(double& L1_miss_rate, double& L2_miss_rate, double& avg_acc_time) {
    L1_miss_rate = this->l1.calc_miss_rate();
    L2_miss_rate = this->l2.calc_miss_rate();

    avg_acc_time = l1.get_access_time() + L1_miss_rate * l2.get_access_time() + 
                    L1_miss_rate * L2_miss_rate * this->mem_cycle;
}