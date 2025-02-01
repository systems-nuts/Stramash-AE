/*
 * Copyright (C) 2021, Mahmoud Mandour <ma.mandourr@gmail.com>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */

#include <inttypes.h>
#include <stdio.h>
#include <glib.h>

#include <qemu-plugin.h>

#define STRTOLL(x) g_ascii_strtoll(x, NULL, 10)

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static enum qemu_plugin_mem_rw rw = QEMU_PLUGIN_MEM_RW;

static GHashTable *miss_ht;

static GMutex hashtable_lock;
static GRand *rng;

static int limit;
static bool sys;

enum EvictionPolicy
{
    LRU,
    FIFO,
    RAND,
};

enum EvictionPolicy policy;

/*
 * A CacheSet is a set of cache blocks. A memory block that maps to a set can be
 * put in any of the blocks inside the set. The number of block per set is
 * called the associativity (assoc).
 *
 * Each block contains the stored tag and a valid bit. Since this is not
 * a functional simulator, the data itself is not stored. We only identify
 * whether a block is in the cache or not by searching for its tag.
 *
 * In order to search for memory data in the cache, the set identifier and tag
 * are extracted from the address and the set is probed to see whether a tag
 * match occur.
 *
 * An address is logically divided into three portions: The block offset,
 * the set number, and the tag.
 *
 * The set number is used to identify the set in which the block may exist.
 * The tag is compared against all the tags of a set to search for a match. If a
 * match is found, then the access is a hit.
 *
 * The CacheSet also contains bookkeaping information about eviction details.
 */

typedef enum
{
    MODIFIED,
    EXCLUSIVE,
    SHARED,
    INVALID
} CacheState;

typedef struct
{
    uint64_t tag;
    bool valid;
    bool dirty;
    CacheState state;
} CacheBlock;

typedef struct
{
    CacheBlock *blocks;
    uint64_t *lru_priorities;
    uint64_t lru_gen_counter;
    GQueue *fifo_queue;
} CacheSet;

typedef struct
{
    CacheSet *sets;
    int num_sets;
    int cachesize;
    int assoc;
    int blksize_shift;
    uint64_t set_mask;
    uint64_t tag_mask;
    uint64_t accesses;
    uint64_t misses;
} Cache;

typedef struct
{
    char *disas_str;
    const char *symbol;
    uint64_t addr;
    uint64_t l1_dmisses;
    uint64_t l1_imisses;
    uint64_t l2_misses;
} InsnData;

void (*update_hit)(Cache *cache, int set, int blk);
void (*update_miss)(Cache *cache, int set, int blk);

void (*metadata_init)(Cache *cache);
void (*metadata_destroy)(Cache *cache);

static int cores;
static Cache **l1_dcaches, **l1_icaches;

static bool use_l2;
static Cache **l2_ucaches;

static GMutex *l1_dcache_locks;
static GMutex *l1_icache_locks;
static GMutex *l2_ucache_locks;

static uint64_t l1_dmem_accesses;
static uint64_t l1_imem_accesses;
static uint64_t l1_imisses;
static uint64_t l1_dmisses;

static uint64_t l2_mem_accesses;
static uint64_t l2_misses;
//////////////////////////////////////////////////////////////////////////////
//// stramash
//////////////////////////////////////////////////////////////////////////////
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#define SHM_NAME "/icount_sync"
#define SHM_SIZE 0x1000

static void *new_shared(unsigned long size);
static void *new_shared_L3(unsigned long size);
#define sem_x86 *(uint64_t *)(shm_ptr + sizeof(uint64_t))
#define sem_arm *(uint64_t *)(shm_ptr)

#define DEBUG 0

#if DEBUG
#define DEBUG_PRINTF(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINTF(...) \
    do                    \
    {                     \
    } while (0)
#endif

typedef enum
{
    OPENPITON_MEM = 1,
    SHARED_MEM = 2,
    SAPEARATE_MEM = 3,
} TestMode;

static int64_t insn_count;
static void *shm_ptr;
static int fd;
static TestMode test;
static int Stramash_id;

typedef enum
{
    PROF_ARCH_AARCH64,
    PROF_ARCH_X86_64,
    PROF_ARCH_INCOMPATIBLE
} ProfileArch;


static ProfileArch prof_arch;
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//// cache sim
//////////////////////////////////////////////////////////////////////////////
#define SHM_CACHE_NAME "/Stramash_cache"
#define SHM_L3_NAME "/Stramash_L3"

/* mem access overhead*/
#define L1_x86_overhead 4
#define L2_x86_overhead 17

#define L1_arm_overhead 4
#define L2_arm_overhead 13

#define L3_overhead 40
#define Local_mem_overhead 360
#define Remote_mem_overhead 660

/* Flag sent by kernel*/
#define ARCH_TRIGGER_ADDR_OPEN (uint64_t)0x1200000500
#define ARCH_TRIGGER_ADDR_EXIT (uint64_t)0x1200000700
#define ARCH_TRIGGER_ADDR_STOP (uint64_t)0x1200000800

#define SWITCH (uint64_t)0x1200000000
// #define SWITCH_ARM (uint64_t)0xf4240000
#define SWITCH_ARM (uint64_t)0x1200000000

#define STOP_SWITCH (uint64_t)0x1200001000
#define STOP_SWITCH_ARM (uint64_t)0xf4241000

static uint64_t _switch; 
static uint64_t _stop_switch; 

static int is_stramash = 1;
static int fd_cache;
static int fd_L3;
int shm_cache_size;
int shm_L3_size;
static void *shm_L3_ptr;
static void *shm_L3_ptr_curr;
static void *shm_L3_ptr_curr_limit;

static void *shm_cache_ptr;
static void *shm_cache_own_ptr;
static void *shm_cache_other_ptr;
static void *shm_cache_own_ptr_curr;
static void *shm_cache_own_ptr_limit;

static int is_kernel = 0;

typedef struct
{
    u_int64_t l1d_misses;
    u_int64_t l1d_hits;

    u_int64_t l2_misses;
    u_int64_t l2_hits;

    u_int64_t l3_misses;
    u_int64_t l3_hits;

    u_int64_t local_mem_hits;
    u_int64_t remote_mem_hits;
    u_int64_t remote_shared_mem_hits;

    u_int64_t ipi;
    u_int64_t runtime;
    u_int64_t nr_of_inst;
    u_int64_t nr_of_mem_access;
} RunningInfo;

typedef struct
{
    Cache **l1_dcaches;
    Cache **l1_icaches;
    Cache **l2_ucaches;
    GMutex *l1_dcache_locks;
    GMutex *l1_icache_locks;
    GMutex *l2_ucache_locks;
    ProfileArch isa;
    int start_sim;
    int start_record;

    //kernel
    RunningInfo *running_info;
    //user
    RunningInfo *running_info2;
} ShmCatalog;

static ShmCatalog *shmcatalog;
static ShmCatalog *other_shmcatalog;

typedef enum
{
    L1 = 1,
    L2 = 2,
    L3 = 3,
    PEER_CACHE = 4,
    DRAM = 5,
    REMOTE = 6
} CacheLevel;

typedef struct
{
    CacheBlock *blocks;
} ReturnInfo;

// overload g_new to create in our own shared memory
#define _g_new(typ, nr)  (typ *)g_new(typ, nr)
#define new_L3(typ, nr)  (typ *)g_new(typ, nr)
#define _g_new0(typ, nr) (typ *)g_new0(typ, nr)
#define g_free(ptr) \
    do              \
    {               \
    } while (0)

/* cache extends */
// only one L3 cache which shared among all cores
static bool use_l3;
static bool shared_l3;

static Cache *l3_ucache;
static GMutex *l3_ucache_locks;
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static int pow_of_two(int num)
{
    g_assert((num & (num - 1)) == 0);
    int ret = 0;
    while (num /= 2)
    {
        ret++;
    }
    return ret;
}

/*
 * LRU evection policy: For each set, a generation counter is maintained
 * alongside a priority array.
 *
 * On each set access, the generation counter is incremented.
 *
 * On a cache hit: The hit-block is assigned the current generation counter,
 * indicating that it is the most recently used block.
 *
 * On a cache miss: The block with the least priority is searched and replaced
 * with the newly-cached block, of which the priority is set to the current
 * generation number.
 */

static void lru_priorities_init(Cache *cache)
{
    int i;

    for (i = 0; i < cache->num_sets; i++)
    {
        cache->sets[i].lru_priorities = _g_new0(uint64_t, cache->assoc);
        cache->sets[i].lru_gen_counter = 0;
    }
}

static void lru_update_blk(Cache *cache, int set_idx, int blk_idx)
{
    CacheSet *set = &cache->sets[set_idx];
    set->lru_priorities[blk_idx] = cache->sets[set_idx].lru_gen_counter;
    set->lru_gen_counter++;
}

static int lru_get_lru_block(Cache *cache, int set_idx)
{
    int i, min_idx, min_priority;

    min_priority = cache->sets[set_idx].lru_priorities[0];
    min_idx = 0;

    for (i = 1; i < cache->assoc; i++)
    {
        if (cache->sets[set_idx].lru_priorities[i] < min_priority)
        {
            min_priority = cache->sets[set_idx].lru_priorities[i];
            min_idx = i;
        }
    }
    return min_idx;
}

static void lru_priorities_destroy(Cache *cache)
{
    int i;

    for (i = 0; i < cache->num_sets; i++)
    {
        g_free(cache->sets[i].lru_priorities);
    }
}

/*
 * FIFO eviction policy: a FIFO queue is maintained for each CacheSet that
 * stores accesses to the cache.
 *
 * On a compulsory miss: The block index is enqueued to the fifo_queue to
 * indicate that it's the latest cached block.
 *
 * On a conflict miss: The first-in block is removed from the cache and the new
 * block is put in its place and enqueued to the FIFO queue.
 */

static void fifo_init(Cache *cache)
{
    int i;

    for (i = 0; i < cache->num_sets; i++)
    {
        cache->sets[i].fifo_queue = g_queue_new();
    }
}

static int fifo_get_first_block(Cache *cache, int set)
{
    GQueue *q = cache->sets[set].fifo_queue;
    return GPOINTER_TO_INT(g_queue_pop_tail(q));
}

static void fifo_update_on_miss(Cache *cache, int set, int blk_idx)
{
    GQueue *q = cache->sets[set].fifo_queue;
    g_queue_push_head(q, GINT_TO_POINTER(blk_idx));
}

static void fifo_destroy(Cache *cache)
{
    int i;

    for (i = 0; i < cache->num_sets; i++)
    {
        g_queue_free(cache->sets[i].fifo_queue);
    }
}

static inline uint64_t extract_tag(Cache *cache, uint64_t addr)
{
    return addr & cache->tag_mask;
}

static inline uint64_t extract_set(Cache *cache, uint64_t addr)
{
    return (addr & cache->set_mask) >> cache->blksize_shift;
}

static const char *cache_config_error(int blksize, int assoc, int cachesize)
{
    if (cachesize % blksize != 0)
    {
        return "cache size must be divisible by block size";
    }
    else if (cachesize % (blksize * assoc) != 0)
    {
        return "cache size must be divisible by set size (assoc * block size)";
    }
    else
    {
        return NULL;
    }
}

static bool bad_cache_params(int blksize, int assoc, int cachesize)
{
    return (cachesize % blksize) != 0 || (cachesize % (blksize * assoc) != 0);
}


static Cache *cache_init(int blksize, int assoc, int cachesize)
{
    Cache *cache;
    int i;
    uint64_t blk_mask;

    /*
     * This function shall not be called directly, and hence expects suitable
     * parameters.
     */
    g_assert(!bad_cache_params(blksize, assoc, cachesize));

    cache = _g_new(Cache, 1);
    cache->assoc = assoc;
    cache->cachesize = cachesize;
    cache->num_sets = cachesize / (blksize * assoc);
    cache->sets = _g_new(CacheSet, cache->num_sets);
    cache->blksize_shift = pow_of_two(blksize);
    cache->accesses = 0;
    cache->misses = 0;

    for (i = 0; i < cache->num_sets; i++)
    {
        cache->sets[i].blocks = _g_new0(CacheBlock, assoc);
    }

    blk_mask = blksize - 1;
    cache->set_mask = ((cache->num_sets - 1) << cache->blksize_shift);
    cache->tag_mask = ~(cache->set_mask | blk_mask);

    if (metadata_init)
    {
        metadata_init(cache);
    }

    return cache;
}

static Cache **caches_init(int blksize, int assoc, int cachesize)
{
    Cache **caches;
    int i;

    if (bad_cache_params(blksize, assoc, cachesize))
    {
        return NULL;
    }

    caches = _g_new(Cache *, cores);

    for (i = 0; i < cores; i++)
    {
        caches[i] = cache_init(blksize, assoc, cachesize);
    }

    return caches;
}

static int get_invalid_block(Cache *cache, uint64_t set)
{
    int i;

    for (i = 0; i < cache->assoc; i++)
    {
        if (!cache->sets[set].blocks[i].valid)
        {
            return i;
        }
    }

    return -1;
}

static int get_replaced_block(Cache *cache, int set)
{
    switch (policy)
    {
    case RAND:
        return g_rand_int_range(rng, 0, cache->assoc);
    case LRU:
        return lru_get_lru_block(cache, set);
    case FIFO:
        return fifo_get_first_block(cache, set);
    default:
        g_assert_not_reached();
    }
}

// Stramash
// Allocate in share memory
static void *new_shared(unsigned long size)
{
    // shared mem: /* --------x86--------- *//* --------arm--------- */
    //             /* -------------------- *//* -------------------- */
    // x86 use first half, arm use second half
    void *object = shm_cache_own_ptr_curr;
    shm_cache_own_ptr_curr += size;
    g_assert(shm_cache_own_ptr_curr < shm_cache_own_ptr_limit);
    return object;
}


static int in_cache(Cache *cache, uint64_t addr)
{
    int i;
    uint64_t tag, set;

    tag = extract_tag(cache, addr);
    set = extract_set(cache, addr);

    for (i = 0; i < cache->assoc; i++)
    {
        if (cache->sets[set].blocks[i].tag == tag &&
            cache->sets[set].blocks[i].valid)
        {
            return i;
        }
    }

    return -1;
}

static void add_runtime(uint64_t runtime)
{
    RunningInfo *info;
    if(is_kernel){
        shmcatalog->running_info->runtime += runtime;
    } else{
       shmcatalog->running_info2->runtime += runtime;
    }
    
    
    insn_count += runtime;
}

// Feedback overhead of memory access
static void mem_access_overhead(CacheLevel cache_level)
{   
    RunningInfo *info;
    if(is_kernel){
        info = shmcatalog->running_info;
    } else{
        info = shmcatalog->running_info2;
    }
    
    switch (cache_level)
    {
    case L1:
        info->l1d_hits++;

        add_runtime(prof_arch == PROF_ARCH_AARCH64 ? L1_arm_overhead : L1_x86_overhead);
        break;

    case L2:
        // need to support icache
        info->l1d_misses++;
        info->l2_hits++;
        
        add_runtime(prof_arch == PROF_ARCH_AARCH64 ? L2_arm_overhead : L2_x86_overhead);
        break;

    case L3:
        info->l1d_misses++;
        info->l2_misses++;

        info->l3_hits++;
        add_runtime(L3_overhead);
        break;

    case DRAM:
        info->l1d_misses++;
        info->l2_misses++;
        info->l3_misses++;

        info->local_mem_hits++;
        add_runtime(Local_mem_overhead);
        break;

    case REMOTE:
        info->l1d_misses++;
        info->l2_misses++;
        info->l3_misses++;

        info->remote_mem_hits++;
        add_runtime(Remote_mem_overhead);
        break;

    case PEER_CACHE:
        break;
    }
}

/**
 * access_cache(): Simulate a cache access
 * @cache: The cache under simulation
 * @addr: The address of the requested memory location
 *
 * Returns true if the requested data is hit in the cache and false when missed.
 * The cache is updated on miss for the next access.
 */
static bool access_cache(Cache *cache, uint64_t addr, ReturnInfo *ret_info)
{
    int hit_blk, replaced_blk;
    uint64_t tag, set;

    tag = extract_tag(cache, addr);
    set = extract_set(cache, addr);

    hit_blk = in_cache(cache, addr);

    // Cache Hit
    if (hit_blk != -1)
    {
        ret_info->blocks = &cache->sets[set].blocks[hit_blk];

        // todo: uncomment this
        // if (cache->sets[set].blocks[hit_blk].state == INVALID)
        // {
        //     return false;
        // }

        if (update_hit)
        {
            update_hit(cache, set, hit_blk);
        }

        return true;
    }

    // Cache miss
    replaced_blk = get_invalid_block(cache, set);

    if (replaced_blk == -1)
    {
        replaced_blk = get_replaced_block(cache, set);
    }

    if (update_miss)
    {
        update_miss(cache, set, replaced_blk);
    }

    cache->sets[set].blocks[replaced_blk].tag = tag;
    cache->sets[set].blocks[replaced_blk].valid = true;

    ret_info->blocks = &cache->sets[set].blocks[replaced_blk];
    return false;
}

static bool in_peer_cache(uint64_t effective_addr)
{
    // g_mutex_lock(&other_shmcatalog->l2_ucache_locks[0]);
    bool result = !(in_cache(other_shmcatalog->l2_ucaches[0], effective_addr) == -1);
    // g_mutex_unlock(&other_shmcatalog->l2_ucache_locks[0]);
    return result;
}

static void find_and_set(Cache *cache, uint64_t addr, CacheState state)
{
    int i;
    uint64_t tag, set;

    tag = extract_tag(cache, addr);
    set = extract_set(cache, addr);

    for (i = 0; i < cache->assoc; i++)
    {
        if (cache->sets[set].blocks[i].tag == tag &&
            cache->sets[set].blocks[i].valid)
        {
            cache->sets[set].blocks[i].state = state;
            return;
        }
    }

    return;
}

static void broadcast_state(uint64_t effective_addr, CacheState state)
{
    find_and_set(other_shmcatalog->l2_ucaches[0], effective_addr, state);
    find_and_set(other_shmcatalog->l1_dcaches[0], effective_addr, state);
    return;
}

static void print_info(RunningInfo *info)
{
    // Calculate cache hit rates
    double l1_hit_rate = (double)info->l1d_hits / (info->l1d_hits + info->l1d_misses) * 100.0;
    double l2_hit_rate = (double)info->l2_hits / (info->l2_hits + info->l2_misses) * 100.0;
    double l3_hit_rate = (double)info->l3_hits / (info->l3_hits + info->l3_misses) * 100.0;

    // Print the RunningInfo
    printf("L1 Cache Hit Rate: %.2f%%\n", l1_hit_rate);
    printf("L2 Cache Hit Rate: %.2f%%\n", l2_hit_rate);
    printf("L3 Cache Hit Rate: %.2f%%\n", l3_hit_rate);

    printf("L1 Cache Hits: %lu\n", info->l1d_hits);
    printf("L2 Cache Hits: %lu\n", info->l2_hits);
    printf("L3 Cache Hits: %lu\n", info->l3_hits);

    printf("L1 Cache Accesses: %lu\n", info->l1d_hits + info->l1d_misses);
    printf("L2 Cache Accesses: %lu\n", info->l2_hits + info->l2_misses);
    printf("L3 Cache Accesses: %lu\n", info->l3_hits + info->l3_misses);

    printf("Local Memory Hits: %lu\n", info->local_mem_hits);
    printf("Remote Memory Hits: %lu\n", info->remote_mem_hits);
    // printf("Remote Shared Memory Hits: %lu\n", info->remote_shared_mem_hits);
    // printf("Inter-Processor Interrupts: %lu\n", info->ipi);

    printf("Number of Instructions: %lu\n", info->nr_of_inst);
    printf("Number of mem_access: %lu\n", info->nr_of_mem_access);
    printf("Runtime: %lu\n", info->runtime);
}

static void print_result()
{
    RunningInfo *info = shmcatalog->running_info;

    printf("kernel:\n");
    print_info(info);
    
    printf("user:\n");
    info = shmcatalog->running_info2;
    print_info(info);
    
}

static void clean_both_cache()
{
    memset(shmcatalog->running_info, 0, sizeof(RunningInfo));

    memset(shmcatalog->running_info2, 0, sizeof(RunningInfo));
}

static void sync_both_cache()
{
    memcpy(shmcatalog->running_info, other_shmcatalog->running_info, sizeof(RunningInfo));
}

static void vcpu_mem_access(unsigned int vcpu_index, qemu_plugin_meminfo_t info,
                            uint64_t vaddr, void *userdata)
{
    uint64_t effective_addr;
    struct qemu_plugin_hwaddr *hwaddr;
    int cache_idx;
    InsnData *insn;
    bool hit_in_l1;
    bool hit_in_l2;
    bool hit_in_l3;
    ReturnInfo l1_ret;
    ReturnInfo l2_ret;
    ReturnInfo l3_ret;

    bool is_store;

    hwaddr = qemu_plugin_get_hwaddr(info, vaddr);
    
    if (hwaddr && qemu_plugin_hwaddr_is_io(hwaddr)) {
        return;
    }

    effective_addr = is_stramash ? qemu_plugin_hwaddr_phys_addr(hwaddr) : vaddr;    
    
    if (is_stramash && (effective_addr) == _switch)
    {
        if (shmcatalog->start_sim == 0)
        {
            printf("Cache sync Open\n");

            shmcatalog->start_sim = 1;
            return;
        }
        else
        {
            printf("Cache sync close\n");

            if(prof_arch == PROF_ARCH_AARCH64){
                sem_arm = (uint64_t)1;
            }
            else{
                sem_x86 = (uint64_t)1;
            }

            shmcatalog->start_sim = 0;
            print_result();
            clean_both_cache();
            return;
        }
    }

    // if (is_stramash && (effective_addr) == _stop_switch)
    // {
    //     if (shmcatalog->start_record == 0)
    //     {
    //         shmcatalog->start_record = 1;
    //         return;
    //     }
    //     else
    //     {
    //         shmcatalog->start_record = 0;
    //         return;
    //     }
    // }
    
    /////////////////////////////
    // Stramash single thread version
    /////////////////////////////

    // if ((effective_addr) == ARCH_TRIGGER_ADDR_OPEN)
    // {
    //     printf("Cache sim Open\n");
    //     shmcatalog->start_sim = 1;
    //     other_shmcatalog->start_sim = 0;
    //     return;
    // }

    // if ((effective_addr) == ARCH_TRIGGER_ADDR_STOP)
    // {
    //     printf("Cache sim Stop\n");
    //     shmcatalog->start_sim = 0;
    //     other_shmcatalog->start_sim = 1;
    //     return;
    // }

    // if ((effective_addr) == ARCH_TRIGGER_ADDR_EXIT)
    // {
    //     printf("Cache sim Exit\n");
    //     shmcatalog->start_sim = 0;
    //     other_shmcatalog->start_sim = 0;
    //     sync_both_cache();
    //     print_result();
    //     clean_both_cache();
    //     return;
    // }

    /////////////////////////////
    // ----------------------
    /////////////////////////////

    if (is_stramash && shmcatalog->start_sim == 0)
    {
        return;
    }
    
    if (is_stramash && shmcatalog->start_record == 0)
    {
        return;
    }

    if(is_kernel){
        shmcatalog->running_info->nr_of_mem_access++;
    } else{
        shmcatalog->running_info2->nr_of_mem_access++;
    }

    cache_idx = vcpu_index % cores;
    is_store = qemu_plugin_mem_is_store(info);

    g_mutex_lock(&l1_dcache_locks[cache_idx]);
    hit_in_l1 = access_cache(l1_dcaches[cache_idx], effective_addr, &l1_ret);
    g_mutex_unlock(&l1_dcache_locks[cache_idx]);
    
    if (hit_in_l1 || !use_l2)
    {
        // L1 hit
        mem_access_overhead(L1);
        // if (is_store && l1_ret.blocks->state == SHARED)
        // {
        //     l1_ret.blocks->state = MODIFIED;
        //     broadcast_state(effective_addr, INVALID);
        // }
        return;
    }
    g_mutex_lock(&l2_ucache_locks[cache_idx]);
    hit_in_l2 = access_cache(l2_ucaches[cache_idx], effective_addr, &l2_ret);
    g_mutex_unlock(&l2_ucache_locks[cache_idx]);

    if (hit_in_l2 || !use_l3)
    {
        // L2 hit
        mem_access_overhead(L2);
        // if (is_store && l2_ret.blocks->state == SHARED)
        // {
        //     l2_ret.blocks->state = MODIFIED;
        //     broadcast_state(effective_addr, INVALID);
        // }
        // // sync the state of L1 and L2
        // l1_ret.blocks->state = l2_ret.blocks->state;
        return;
    }

    if (shared_l3 == true)
    {
        while (!g_mutex_trylock(l3_ucache_locks))
        {
            // Busy-wait loop until the lock is acquired
        }

        hit_in_l3 = access_cache(l3_ucache, effective_addr, &l3_ret);
        g_mutex_unlock(l3_ucache_locks);
    }
    else{
        hit_in_l3 = access_cache(l3_ucache, effective_addr, &l3_ret);
    }
    

    if (!hit_in_l3)
    {
        // l3_ret.blocks->state = EXCLUSIVE;
        // l2_ret.blocks->state = EXCLUSIVE;
        // l1_ret.blocks->state = EXCLUSIVE;

        mem_access_overhead(DRAM);

        // if(prof_arch == PROF_ARCH_AARCH64)
        // {
        //     // arm
        //     // Ring buffer
        //     if(0xA0000000 >= effective_addr && effective_addr >=0x40000000)
        //     {
        //         mem_access_overhead(REMOTE);
        //     }
        //     else if (effective_addr >= 0x100000000 && effective_addr < 0x150000000)
        //     {
        //         mem_access_overhead(REMOTE);
        //     }
        //     else
        //     {
        //         mem_access_overhead(DRAM);
        //     }
        // }
        // else
        // {
        //     // x86
        //     // Ring buffer
        //     if(0xC0000000 >= effective_addr && effective_addr >=0x60000000)
        //     {
        //         mem_access_overhead(REMOTE);
        //     }
        //     else if (effective_addr >= 0x150000000)
        //     {
        //         mem_access_overhead(REMOTE);
        //     }
        //     else
        //     {
        //         mem_access_overhead(DRAM);
        //     }
        // }

        // if(test==OPENPITON_MEM)
        // {
        //         mem_access_overhead(DRAM);
        // }
        // if(test==SHARED_MEM)
        // {
        //     if(prof_arch == PROF_ARCH_AARCH64)
        //     {
        //         // arm
        //         // Ring buffer
        //         if(0xA0000000 >= effective_addr && effective_addr >=0x40000000)
        //         {
        //             mem_access_overhead(REMOTE);
        //         }
        //         else if (effective_addr >= 0x100000000 && effective_addr < 0x150000000)
        //         {
        //             mem_access_overhead(REMOTE);
        //         }
        //         else
        //         {
        //             mem_access_overhead(DRAM);
        //         }
        //     }
        //     else
        //     {
        //         // x86
        //         // Ring buffer
        //         if(0xC0000000 >= effective_addr && effective_addr >=0x60000000)
        //         {
        //             mem_access_overhead(REMOTE);
        //         }
        //         else if (effective_addr >= 0x150000000)
        //         {
        //             mem_access_overhead(REMOTE);
        //         }
        //         else
        //         {
        //             mem_access_overhead(DRAM);
        //         }
        //     }
        // }
        // if(test==SAPEARATE_MEM)
        // {   if(prof_arch == PROF_ARCH_AARCH64)
        //     {
        //         // arm
        //         // Ring buffer
        //         if(0xA0000000 >= effective_addr && effective_addr >=0x40000000)
        //         {
        //             mem_access_overhead(REMOTE);
        //         }
        //         else if (effective_addr >= 0x100000000)
        //         {
        //             mem_access_overhead(REMOTE);
        //         }
        //         else
        //         {
        //             mem_access_overhead(DRAM);
        //         }
        //     }
        //     else{
        //         // x86
        //         // Ring buffer
        //         if(0xC0000000 >= effective_addr && effective_addr >=0x60000000)
        //         {
        //             mem_access_overhead(REMOTE);
        //         }
        //         else if (effective_addr >= 0x100000000)
        //         {
        //             mem_access_overhead(REMOTE);
        //         }
        //         else
        //         {
        //             mem_access_overhead(DRAM);
        //         }
        //     }
        // }
    }
    else
    {
        // L3 hit
        // sync the state of L1, L2 and L3
        // if (in_peer_cache(effective_addr))
        // {
        //     // peer cache hit
        //     l2_ret.blocks->state = SHARED;
        //     l1_ret.blocks->state = SHARED;
        // }
        // else
        // {
        //     l2_ret.blocks->state = EXCLUSIVE;
        //     l1_ret.blocks->state = EXCLUSIVE;
        // }
        mem_access_overhead(L3);
    }
}

static void vcpu_insn_exec(unsigned int vcpu_index, void *userdata)
{
    if(is_stramash == 0){
        shmcatalog->running_info->runtime += 1;
        shmcatalog->running_info->nr_of_inst++;
    }

    else if (shmcatalog->start_sim == 1)
    {
        insn_count += 1;
        if(shmcatalog->start_record == 1)
        {
            if(is_kernel){
                shmcatalog->running_info->runtime += 1;
                shmcatalog->running_info->nr_of_inst++;
            } else{
                shmcatalog->running_info2->runtime += 1;
                shmcatalog->running_info2->nr_of_inst++;
            }
        }

        // if(qemu_plugin_get_ipi()){
        //     shmcatalog->running_info->ipi++;
        //     shmcatalog->running_info->runtime+=40000;
        // }
        
        // if (insn_count > 60000)
        // {
        //     insn_count = 0;

        //     if (prof_arch == PROF_ARCH_AARCH64)
        //     {
        //         sem_arm = (uint64_t)1;
        //         while (sem_x86 != (uint64_t)1 && shmcatalog->start_sim == 1)
        //         {

        //         }
        //         sem_x86 = (uint64_t)0;
        //     }
        //     else
        //     {
        //         sem_x86 = (uint64_t)1;
        //         while (sem_arm != (uint64_t)1 && shmcatalog->start_sim == 1)
        //         {

        //         }
        //         sem_arm = (uint64_t)0;
        //     }
        // }
    }
    return;

    // if(qemu_plugin_get_ipi()){
    //     shmcatalog->running_info->ipi++;
    //     shmcatalog->running_info->runtime+=40000;
    // }
    // uint64_t insn_addr;
    // InsnData *insn;
    // int cache_idx;
    // bool hit_in_l1;
    // bool hit_in_l2;

    // insn_addr = ((InsnData *)userdata)->addr;

    // cache_idx = vcpu_index % cores;
    // g_mutex_lock(&l1_icache_locks[cache_idx]);
    // hit_in_l1 = access_cache(l1_icaches[cache_idx], insn_addr, false, L1, cache_idx);
    // if (!hit_in_l1)
    // {
    //     insn = userdata;
    //     __atomic_fetch_add(&insn->l1_imisses, 1, __ATOMIC_SEQ_CST);
    //     l1_icaches[cache_idx]->misses++;
    // }
    // l1_icaches[cache_idx]->accesses++;
    // g_mutex_unlock(&l1_icache_locks[cache_idx]);

    // if (hit_in_l1 || !use_l2)
    // {
    //     /* No need to access L2 */
    //     return;
    // }

    // g_mutex_lock(&l2_ucache_locks[cache_idx]);
    // hit_in_l2 = access_cache(l2_ucaches[cache_idx], insn_addr, false, L2, cache_idx);
    // if (!hit_in_l2)
    // {
    //     insn = userdata;
    //     __atomic_fetch_add(&insn->l2_misses, 1, __ATOMIC_SEQ_CST);
    //     l2_ucaches[cache_idx]->misses++;
    // }
    // l2_ucaches[cache_idx]->accesses++;
    // g_mutex_unlock(&l2_ucache_locks[cache_idx]);

    // if (hit_in_l2 || !use_l3)
    // {
    //     return;
    // }
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n_insns;
    size_t i;
    InsnData *data;
    n_insns = qemu_plugin_tb_n_insns(tb);

    if(prof_arch!=PROF_ARCH_AARCH64){
        if (qemu_plugin_get_cpl() == 0){
            is_kernel = 1;
        }else{
            is_kernel = 0;
        }
    }

    for (i = 0; i < n_insns; i++)
    {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        // uint64_t effective_addr;
        // effective_addr = (uint64_t)qemu_plugin_insn_vaddr(insn);

        // g_mutex_lock(&hashtable_lock);
        // data = g_hash_table_lookup(miss_ht, GUINT_TO_POINTER(effective_addr));
        // if (data == NULL)
        // {
        //     // data = g_new0(InsnData, 1);
        //     // stramash,cong not needed in stramash
        //     // data->disas_str = qemu_plugin_insn_disas(insn);
        //     // stramash,cong not support in qemu 6.0
        //     // data->symbol = qemu_plugin_insn_symbol(insn);
        //     data->addr = effective_addr;
        //     // g_hash_table_insert(miss_ht, GUINT_TO_POINTER(effective_addr),
        //     //                     (gpointer)data);
        // }
        // g_mutex_unlock(&hashtable_lock);
        
        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem_access,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         rw, NULL);

        // todo: check icache
        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                               QEMU_PLUGIN_CB_NO_REGS, NULL);
    }
}

static void insn_free(gpointer data)
{
    InsnData *insn = (InsnData *)data;
    g_free(insn->disas_str);
    g_free(insn);
}

static void cache_free(Cache *cache)
{
    for (int i = 0; i < cache->num_sets; i++)
    {
        g_free(cache->sets[i].blocks);
    }

    if (metadata_destroy)
    {
        metadata_destroy(cache);
    }

    g_free(cache->sets);
    g_free(cache);
}

static void caches_free(Cache **caches)
{
    int i;

    for (i = 0; i < cores; i++)
    {
        cache_free(caches[i]);
    }
}

static void append_stats_line(GString *line,
                              uint64_t l1_daccess, uint64_t l1_dmisses,
                              uint64_t l1_iaccess, uint64_t l1_imisses,
                              uint64_t l2_access, uint64_t l2_misses)
{
    double l1_dmiss_rate = ((double)l1_dmisses) / (l1_daccess) * 100.0;
    double l1_imiss_rate = ((double)l1_imisses) / (l1_iaccess) * 100.0;

    g_string_append_printf(line, "%-14" PRIu64 " %-12" PRIu64 " %9.4lf%%"
                                 "  %-14" PRIu64 " %-12" PRIu64 " %9.4lf%%",
                           l1_daccess,
                           l1_dmisses,
                           l1_daccess ? l1_dmiss_rate : 0.0,
                           l1_iaccess,
                           l1_imisses,
                           l1_iaccess ? l1_imiss_rate : 0.0);

    if (l2_access && l2_misses)
    {
        double l2_miss_rate = ((double)l2_misses) / (l2_access) * 100.0;
        g_string_append_printf(line,
                               "  %-12" PRIu64 " %-11" PRIu64 " %10.4lf%%",
                               l2_access,
                               l2_misses,
                               l2_access ? l2_miss_rate : 0.0);
    }

    g_string_append(line, "\n");
}

static void sum_stats(void)
{
    int i;

    g_assert(cores > 1);
    for (i = 0; i < cores; i++)
    {
        l1_imisses += l1_icaches[i]->misses;
        l1_dmisses += l1_dcaches[i]->misses;
        l1_imem_accesses += l1_icaches[i]->accesses;
        l1_dmem_accesses += l1_dcaches[i]->accesses;

        if (use_l2)
        {
            l2_misses += l2_ucaches[i]->misses;
            l2_mem_accesses += l2_ucaches[i]->accesses;
        }
    }
}

static int dcmp(gconstpointer a, gconstpointer b)
{
    InsnData *insn_a = (InsnData *)a;
    InsnData *insn_b = (InsnData *)b;

    return insn_a->l1_dmisses < insn_b->l1_dmisses ? 1 : -1;
}

static int icmp(gconstpointer a, gconstpointer b)
{
    InsnData *insn_a = (InsnData *)a;
    InsnData *insn_b = (InsnData *)b;

    return insn_a->l1_imisses < insn_b->l1_imisses ? 1 : -1;
}

static int l2_cmp(gconstpointer a, gconstpointer b)
{
    InsnData *insn_a = (InsnData *)a;
    InsnData *insn_b = (InsnData *)b;

    return insn_a->l2_misses < insn_b->l2_misses ? 1 : -1;
}

static void log_stats(void)
{
    int i;
    Cache *icache, *dcache, *l2_cache;

    g_autoptr(GString) rep = g_string_new("core #, data accesses, data misses,"
                                          " dmiss rate, insn accesses,"
                                          " insn misses, imiss rate");

    if (use_l2)
    {
        g_string_append(rep, ", l2 accesses, l2 misses, l2 miss rate");
    }

    g_string_append(rep, "\n");

    for (i = 0; i < cores; i++)
    {
        g_string_append_printf(rep, "%-8d", i);
        dcache = l1_dcaches[i];
        icache = l1_icaches[i];
        l2_cache = use_l2 ? l2_ucaches[i] : NULL;
        append_stats_line(rep, dcache->accesses, dcache->misses,
                          icache->accesses, icache->misses,
                          l2_cache ? l2_cache->accesses : 0,
                          l2_cache ? l2_cache->misses : 0);
    }

    if (cores > 1)
    {
        sum_stats();
        g_string_append_printf(rep, "%-8s", "sum");
        append_stats_line(rep, l1_dmem_accesses, l1_dmisses,
                          l1_imem_accesses, l1_imisses,
                          l2_cache ? l2_mem_accesses : 0, l2_cache ? l2_misses : 0);
    }

    g_string_append(rep, "\n");
    qemu_plugin_outs(rep->str);
}

static void log_top_insns(void)
{
    int i;
    GList *curr, *miss_insns;
    InsnData *insn;

    miss_insns = g_hash_table_get_values(miss_ht);
    miss_insns = g_list_sort(miss_insns, dcmp);
    g_autoptr(GString) rep = g_string_new("");
    g_string_append_printf(rep, "%s", "address, data misses, instruction\n");

    for (curr = miss_insns, i = 0; curr && i < limit; i++, curr = curr->next)
    {
        insn = (InsnData *)curr->data;
        g_string_append_printf(rep, "0x%" PRIx64, insn->addr);
        if (insn->symbol)
        {
            g_string_append_printf(rep, " (%s)", insn->symbol);
        }
        g_string_append_printf(rep, ", %" PRId64 ", %s\n",
                               insn->l1_dmisses, insn->disas_str);
    }

    miss_insns = g_list_sort(miss_insns, icmp);
    g_string_append_printf(rep, "%s", "\naddress, fetch misses, instruction\n");

    for (curr = miss_insns, i = 0; curr && i < limit; i++, curr = curr->next)
    {
        insn = (InsnData *)curr->data;
        g_string_append_printf(rep, "0x%" PRIx64, insn->addr);
        if (insn->symbol)
        {
            g_string_append_printf(rep, " (%s)", insn->symbol);
        }
        g_string_append_printf(rep, ", %" PRId64 ", %s\n",
                               insn->l1_imisses, insn->disas_str);
    }

    if (!use_l2)
    {
        goto finish;
    }

    miss_insns = g_list_sort(miss_insns, l2_cmp);
    g_string_append_printf(rep, "%s", "\naddress, L2 misses, instruction\n");

    for (curr = miss_insns, i = 0; curr && i < limit; i++, curr = curr->next)
    {
        insn = (InsnData *)curr->data;
        g_string_append_printf(rep, "0x%" PRIx64, insn->addr);
        if (insn->symbol)
        {
            g_string_append_printf(rep, " (%s)", insn->symbol);
        }
        g_string_append_printf(rep, ", %" PRId64 ", %s\n",
                               insn->l2_misses, insn->disas_str);
    }

finish:
    qemu_plugin_outs(rep->str);
    g_list_free(miss_insns);
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    print_result();
    
    

    // log_stats();

    // if(prof_arch==PROF_ARCH_X86_64){
    //     sync_both_cache();
    //     print_result();
    //     clean_both_cache();
    // }

    // caches_free(l1_dcaches);
    // caches_free(l1_icaches);

    // g_free(l1_dcache_locks);
    // g_free(l1_icache_locks);

    // if (use_l2)
    // {
    //     caches_free(l2_ucaches);
    //     g_free(l2_ucache_locks);
    // }

    // g_hash_table_destroy(miss_ht);

    munmap(shm_ptr, SHM_SIZE);
    munmap(shm_cache_ptr, shm_cache_size);
    munmap(shm_L3_ptr, shm_L3_size);
    close(fd);
    close(fd_cache);
    close(fd_L3);
}

// stramash
// Create a L3 cache in share memory
static void *new_shared_L3(unsigned long size)
{

    if(shared_l3 == false){
        return _g_new0(uint8_t, size);
    }
    
    void *object = shm_L3_ptr_curr;
    shm_L3_ptr_curr += size;
    g_assert(shm_L3_ptr_curr < shm_L3_ptr_curr_limit);
    return object;
}

static void metadata_init_l3(Cache *cache)
{
    int i;

    for (i = 0; i < cache->num_sets; i++)
    {
        cache->sets[i].lru_priorities = new_L3(uint64_t, cache->assoc);
        cache->sets[i].lru_gen_counter = 0;
    }
}

static void L3_init(int blksize, int assoc, int cachesize)
{
    if (!use_l3)
    {
        return;
    }

    if (shared_l3 == true && prof_arch != PROF_ARCH_AARCH64)
    {
        return;
    }

    Cache *cache;
    int i;
    uint64_t blk_mask;
    
    if(shared_l3 == false){
        l3_ucache = _g_new0(Cache, 1);
    }

    cache = l3_ucache;
    cache->assoc = assoc;
    cache->cachesize = cachesize;
    cache->num_sets = cachesize / (blksize * assoc);
    cache->sets = new_L3(CacheSet, cache->num_sets);
    cache->blksize_shift = pow_of_two(blksize);
    cache->accesses = 0;
    cache->misses = 0;

    for (i = 0; i < cache->num_sets; i++)
    {
        cache->sets[i].blocks = new_L3(CacheBlock, assoc);
    }

    blk_mask = blksize - 1;
    cache->set_mask = ((cache->num_sets - 1) << cache->blksize_shift);
    cache->tag_mask = ~(cache->set_mask | blk_mask);

    metadata_init_l3(cache);
}

static void shm_init()
{
    // Create a shared memory for semaphore
    char shm_name[256];
    sprintf(shm_name, "%s%d", SHM_NAME, Stramash_id);
    fd = shm_open(shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRWXU);
    if (fd == -1)
    {
        printf("sem: failed to shm_open\n");
        return -1;
    }

    if (ftruncate(fd, SHM_SIZE) < 0)
    {
        printf("sem: truncation failure\n");
        return -1;
    }

    shm_ptr = mmap(NULL, SHM_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        printf("sem: mmap failed\n");
        return -1;
    }

    // Create a shared memory for cache
    char shm_cache_name[256];
    sprintf(shm_cache_name, "%s%d", SHM_CACHE_NAME, Stramash_id);
    fd_cache = shm_open(shm_cache_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRWXU);
    if (fd_cache == -1)
    {
        printf("Cache shm: failed to shm_open\n");
        return -1;
    }

    if (ftruncate(fd_cache, shm_cache_size) < 0)
    {
        printf("Cache shm: truncation failure\n");
        return -1;
    }

    // shared mem: /* --------x86--------- *//* --------arm--------- */
    //             /* -------------------- *//* -------------------- */
    // x86 use first half, arm use second half

    // hard coded
    void *preferred_address_L1_L2 = (void *)0xa0000000;
    shm_cache_ptr = mmap(preferred_address_L1_L2, shm_cache_size, PROT_WRITE, MAP_SHARED, fd_cache, 0);
    if (shm_cache_ptr == MAP_FAILED)
    {
        printf("Cache shm: mmap failed\n");
        return -1;
    }

    if (prof_arch == PROF_ARCH_AARCH64)
    {
        shm_cache_own_ptr = shm_cache_ptr + (int)((shm_cache_size + 1) / 2);
        shm_cache_other_ptr = shm_cache_ptr;
    }
    else
    {
        shm_cache_own_ptr = shm_cache_ptr;
        shm_cache_other_ptr = shm_cache_ptr + (int)((shm_cache_size + 1) / 2);
    }
    shm_cache_own_ptr_curr = shm_cache_own_ptr;
    shm_cache_own_ptr_limit = shm_cache_own_ptr + (int)(shm_cache_size / 2);

    printf("shm_L1_L2_size: %p to %p\n", preferred_address_L1_L2, preferred_address_L1_L2 + shm_cache_size);
    printf("Create shm for L1 L2 sim: own:%p,other:%p,own_limit:%p\n", shm_cache_own_ptr, shm_cache_other_ptr, shm_cache_own_ptr_limit);
    // init a helper struct for other process to access
    shmcatalog = _g_new0(ShmCatalog, 1);
    shmcatalog->isa = prof_arch;
    shmcatalog->running_info = _g_new0(RunningInfo, 1);
    shmcatalog->running_info2 = _g_new0(RunningInfo, 1);
    other_shmcatalog = (ShmCatalog *)shm_cache_other_ptr;

    // init L3 cache
    char shm_l3_name[256];
    sprintf(shm_l3_name, "%s%d", SHM_L3_NAME, Stramash_id);
    fd_L3 = shm_open(shm_l3_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRWXU);
    if (fd_L3 == -1)
    {
        printf("L3 Cache shm: failed to shm_open\n");
        return -1;
    }
    if (ftruncate(fd_L3, shm_L3_size) < 0)
    {
        printf("Cache shm: truncation failure\n");
        return -1;
    }
    void *preferred_address_L3 = (void *)0x1000000;
    shm_L3_ptr = mmap(preferred_address_L3, shm_L3_size, PROT_WRITE, MAP_SHARED, fd_L3, 0);
    if (shm_L3_ptr == MAP_FAILED)
    {
        printf("Cache shm: mmap failed\n");
        return -1;
    }

    shm_L3_ptr_curr = shm_L3_ptr;
    shm_L3_ptr_curr_limit = shm_L3_ptr + shm_L3_size;

    l3_ucache_locks = (GMutex *)shm_L3_ptr_curr;
    g_mutex_init(l3_ucache_locks);
    shm_L3_ptr_curr = shm_L3_ptr_curr + sizeof(GMutex);

    l3_ucache = (Cache *)shm_L3_ptr_curr;
    shm_L3_ptr_curr = shm_L3_ptr_curr + sizeof(Cache);

    printf("shm_L3_size: %p to %p\n", preferred_address_L3, preferred_address_L3 + shm_L3_size);
    printf("Create shm for L3 sim: own:%p,own_limit:%p\n", shm_L3_ptr, shm_L3_ptr_curr_limit);
}

static void policy_init(void)
{
    switch (policy)
    {
    case LRU:
        update_hit = lru_update_blk;
        update_miss = lru_update_blk;
        metadata_init = lru_priorities_init;
        metadata_destroy = lru_priorities_destroy;
        break;
    case FIFO:
        update_miss = fifo_update_on_miss;
        metadata_init = fifo_init;
        metadata_destroy = fifo_destroy;
        break;
    case RAND:
        rng = g_rand_new();
        break;
    default:
        g_assert_not_reached();
    }
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{    
    int i;
    int l1_iassoc, l1_iblksize, l1_icachesize;
    int l1_dassoc, l1_dblksize, l1_dcachesize;
    int l2_assoc, l2_blksize, l2_cachesize;
    int l3_assoc, l3_blksize, l3_cachesize;
    limit = 32;
    sys = info->system_emulation;

    l1_dassoc = 8;
    l1_dblksize = 64;
    l1_dcachesize = l1_dblksize * l1_dassoc * 32;

    l1_iassoc = 8;
    l1_iblksize = 64;
    l1_icachesize = l1_iblksize * l1_iassoc * 32;

    l2_assoc = 16;
    l2_blksize = 64;
    l2_cachesize = l2_assoc * l2_blksize * 2048;

    policy = LRU;

    // cores = sys ? qemu_plugin_n_vcpus() : 1;
    cores = 1;

    for (i = 0; i < argc; i++)
    {
        char *opt = argv[i];
        g_auto(GStrv) tokens = g_strsplit(opt, "=", 2);
        
        if (g_strcmp0(tokens[0], "iblksize") == 0)
        {
            l1_iblksize = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "us") == 0)
        {
            is_stramash=0;
        }
        else if (g_strcmp0(tokens[0], "mode") == 0)
        {
            
        }
        else if (g_strcmp0(tokens[0], "test") == 0)
        {
            test = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "stramashid") == 0)
        {
            Stramash_id = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "iassoc") == 0)
        {
            l1_iassoc = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "icachesize") == 0)
        {
            l1_icachesize = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "dblksize") == 0)
        {
            l1_dblksize = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "dassoc") == 0)
        {
            l1_dassoc = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "dcachesize") == 0)
        {
            l1_dcachesize = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "limit") == 0)
        {
            limit = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "cores") == 0)
        {
            cores = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "l2cachesize") == 0)
        {
            use_l2 = true;
            l2_cachesize = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "l2blksize") == 0)
        {
            use_l2 = true;
            l2_blksize = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "l2assoc") == 0)
        {
            use_l2 = true;
            l2_assoc = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "l2") == 0)
        {
            // stramash,cong, not support in qemu 6.0
            // if (!qemu_plugin_bool_parse(tokens[0], tokens[1], &use_l2)) {
            //     fprintf(stderr, "boolean argument parsing failed: %s\n", opt);
            //     return -1;
            // }
            use_l2 = STRTOLL(tokens[1]);
        }
        else if (g_strcmp0(tokens[0], "evict") == 0)
        {
            if (g_strcmp0(tokens[1], "rand") == 0)
            {
                policy = RAND;
            }
            else if (g_strcmp0(tokens[1], "lru") == 0)
            {
                policy = LRU;
            }
            else if (g_strcmp0(tokens[1], "fifo") == 0)
            {
                policy = FIFO;
            }
            else
            {
                fprintf(stderr, "invalid eviction policy: %s\n", opt);
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "option parsing failed: %s\n", opt);
            return -1;
        }
    }
    //////////////////////////////////////////////////////////////////////////////
    // stramash cache parameter init
    //////////////////////////////////////////////////////////////////////////////

    // Sunny_cove and Cortex-A77
    l1_dassoc = 8;
    l1_dblksize = 64;
    l1_dcachesize = l1_dblksize * l1_dassoc * 128;

    l1_iassoc = 8;
    l1_iblksize = 64;
    l1_icachesize = l1_iblksize * l1_iassoc * 128;

    l2_assoc = 16;
    l2_blksize = 64;
    l2_cachesize = l2_assoc * l2_blksize * 2048;

    l3_assoc = 32;
    l3_blksize = 64;
    l3_cachesize = l3_assoc * l3_blksize * 4096*2;

    //  l1_dassoc = 8;
    // l1_dblksize = 64;
    // l1_dcachesize = l1_dblksize * l1_dassoc * 128;

    // l1_iassoc = 8;
    // l1_iblksize = 64;
    // l1_icachesize = l1_iblksize * l1_iassoc * 128;

    // l2_assoc = 16;
    // l2_blksize = 64;
    // l2_cachesize = l2_assoc * l2_blksize * 2048;

    // l3_assoc = 32;
    // l3_blksize = 64;
    // l3_cachesize = l3_assoc * l3_blksize * 4096;

    cores = 1;
    policy = LRU;
    use_l2 = true;
    use_l3 = true;
    shared_l3 = false;

    // todo: change this Hard coded to scalable
    shm_cache_size =
        ((l1_dcachesize / l1_dblksize) * sizeof(CacheBlock) +
         ((l1_dcachesize / l1_dassoc)) * sizeof(CacheSet) +
         cores * sizeof(Cache) +
         (l2_cachesize / l2_blksize) * sizeof(CacheBlock) +
         ((l2_cachesize / l2_assoc)) * sizeof(CacheSet) +
         cores * sizeof(Cache)) *
            3 +
        380000000;

    shm_L3_size = (((l3_cachesize / l3_blksize) * sizeof(CacheBlock) + (l3_cachesize / l3_assoc) * sizeof(CacheSet) + sizeof(Cache)) * 2 + 409600);
    //////////////////////////////////////////////////////////////////////////////
    // stramash init
    //////////////////////////////////////////////////////////////////////////////
    // detect system architecture

    if (strcmp(info->target_name, "aarch64") == 0)
    {
        prof_arch = PROF_ARCH_AARCH64;
        _stop_switch = STOP_SWITCH_ARM;
        _switch = SWITCH_ARM;
    }
    else if (strcmp(info->target_name, "x86_64") == 0)
    {
        prof_arch = PROF_ARCH_X86_64;
        _stop_switch = STOP_SWITCH;
        _switch = SWITCH;
    }
    else
    {
        printf("failed to detect system arch\n");
        return -1;
    }

    shm_init();
    
    // init variables and share memory
    insn_count = 0;
    shmcatalog->start_sim = 0;
    shmcatalog->start_record = 1;

    if (prof_arch == PROF_ARCH_AARCH64)
    {
        sem_arm = (uint64_t)0;
    }
    else
    {
        sem_x86 = (uint64_t)0;
    }
    
    L3_init(l3_blksize, l3_assoc, l3_cachesize);

    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
    
    policy_init();

    l1_dcaches = caches_init(l1_dblksize, l1_dassoc, l1_dcachesize);
    shmcatalog->l1_dcaches = l1_dcaches;
    if (!l1_dcaches)
    {
        const char *err = cache_config_error(l1_dblksize, l1_dassoc, l1_dcachesize);
        fprintf(stderr, "dcache cannot be constructed from given parameters\n");
        fprintf(stderr, "%s\n", err);
        return -1;
    }

    l1_icaches = caches_init(l1_iblksize, l1_iassoc, l1_icachesize);
    shmcatalog->l1_icaches = l1_icaches;
    if (!l1_icaches)
    {
        const char *err = cache_config_error(l1_iblksize, l1_iassoc, l1_icachesize);
        fprintf(stderr, "icache cannot be constructed from given parameters\n");
        fprintf(stderr, "%s\n", err);
        return -1;
    }

    l2_ucaches = use_l2 ? caches_init(l2_blksize, l2_assoc, l2_cachesize) : NULL;
    shmcatalog->l2_ucaches = l2_ucaches;
    if (!l2_ucaches && use_l2)
    {
        const char *err = cache_config_error(l2_blksize, l2_assoc, l2_cachesize);
        fprintf(stderr, "L2 cache cannot be constructed from given parameters\n");
        fprintf(stderr, "%s\n", err);
        return -1;
    }

    // need g_mutex_init because our g_new0 will not init with 0
    // init private cache lock
    l1_dcache_locks = _g_new0(GMutex, cores);
    l1_icache_locks = _g_new0(GMutex, cores);
    l2_ucache_locks = use_l2 ? _g_new0(GMutex, cores) : NULL;

    // init shared locks
    g_mutex_init(&l1_dcache_locks[0]);
    shmcatalog->l1_dcache_locks = l1_dcache_locks;

    g_mutex_init(&l1_icache_locks[0]);
    shmcatalog->l1_icache_locks = l1_icache_locks;

    g_mutex_init(&l2_ucache_locks[0]);
    shmcatalog->l2_ucache_locks = l2_ucache_locks;

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    // miss_ht = g_hash_table_new_full(NULL, g_direct_equal, NULL, insn_free);    
    if (is_stramash)
    {
        // syncronization between two arch
        if (prof_arch == PROF_ARCH_AARCH64)
        {
            sem_arm = (uint64_t)1;
            printf("Waiting x86 to init the cache\n");
            while (sem_x86 != (uint64_t)1)
            {
            }
            printf("Cache-sim plugin inited\n");
            sem_x86 = (uint64_t)0;
        }
        else
        {
            sem_x86 = (uint64_t)1;
            printf("Waiting arm to init the cache\n");
            while (sem_arm != (uint64_t)1)
            {
            }
            printf("Cache-sim plugin inited\n");
            sem_arm = (uint64_t)0;
        }
    }

    return 0;
}