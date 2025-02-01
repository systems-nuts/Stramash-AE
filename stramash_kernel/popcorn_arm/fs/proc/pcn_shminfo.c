// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <popcorn/remote_meminfo.h>
#include <popcorn/mem_reassign.h>

#include<linux/ktime.h>
#define TEST_SLICE_SIZE
extern int MAX_SHM_SLICES;
extern unsigned long popcorn_shm_start_addr;
extern unsigned long popcorn_shm_end_addr;
extern unsigned long popcorn_shm_hotplug_size;
extern int get_popcorn_shm_slices(void);
extern void do_hot_plug(mem_reassign_response_t *mem_reassign);


static int shm_info_show(struct seq_file *m, void *v)
{
#ifdef TEST_SLICE_SIZE
	mem_reassign_response_t *mem_reassign;
        int pcn_time =0;
        unsigned long long duration;
        ktime_t calltime, delta, finishtime, rettime;
        while(get_popcorn_shm_slices()!=MAX_SHM_SLICES)
        {
	calltime = ktime_get();
#ifdef CONFIG_ARM64
                mem_reassign = send_mem_reassign_request(0);
#else           
                mem_reassign = send_mem_reassign_request(1);
#endif  
		finishtime = ktime_get();
                do_hot_plug(mem_reassign);
        
        rettime = ktime_get();
        delta = ktime_sub(finishtime,calltime);
        duration = (unsigned long long) ktime_to_ns(delta) >> 10;
	seq_printf(m, "%lld, ",duration);
	delta = ktime_sub(rettime,finishtime);
        duration = (unsigned long long) ktime_to_ns(delta) >> 10;
	seq_printf(m, "%lld\n",duration);
	}
	//seq_printf(m, "round trip for msg: %d\n",pcn_time);
#endif


#ifdef CONFIG_ARM64
	unsigned long current_addr = popcorn_shm_start_addr + popcorn_shm_hotplug_size*(MAX_SHM_SLICES - get_popcorn_shm_slices());
#else
	unsigned long current_addr = popcorn_shm_start_addr + popcorn_shm_hotplug_size*get_popcorn_shm_slices();
#endif
/*
	seq_printf(m, "0x%lx - 0x%lx\n", popcorn_shm_start_addr,popcorn_shm_end_addr);
	seq_printf(m, "Current:\n");
	seq_printf(m, "x86: 0x%lx - 0x%lx\n",popcorn_shm_start_addr,current_addr);
	seq_printf(m, "ARM: 0x%lx - 0x%lx\n",current_addr,popcorn_shm_end_addr);
	seq_printf(m, "My slices: %d\n",get_popcorn_shm_slices());
	seq_printf(m, "Set up:\n");
	seq_printf(m, "MAX_SHM_SLICES: %d \n",MAX_SHM_SLICES);
	seq_printf(m, "popcorn_shm_hotplug_size: 0x%lx \n",popcorn_shm_hotplug_size);
*/
	return 0;
}

static int __init proc_cmdline_init(void)
{
	proc_create_single("popcorn_shm", 0, NULL, shm_info_show);
	return 0;
}
fs_initcall(proc_cmdline_init);
