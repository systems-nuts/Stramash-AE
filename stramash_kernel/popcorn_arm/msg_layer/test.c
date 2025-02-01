/**
 * shm.c
 * Messaging transport layer over Shared Memory
 *
 * Authors:
 *  Tong Xing <Tong.Xing@ed.ac.uk>
 */
#include <linux/kthread.h>
#include <popcorn/stat.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/memremap.h>
#include <linux/inet.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/memory_hotplug.h>
#include "ring_buffer.h"
#include "common.h"
#include <popcorn/remote_meminfo.h>
#include <popcorn/mem_reassign.h>
#include <linux/mm.h>
#include <linux/semaphore.h>
#include <popcorn/types.h>
static ulong addr=0;
module_param(addr,ulong,0660);

static struct page *test;

int init_test(void)
{
	test = (struct page*)ioremap(addr,sizeof(struct page));	
	printk("ioremap %lx \n page %lx page* %lx phys %lx\n",addr,test,*test,page_to_phys(test));
	return 0;
}
void exit_test(void)
{
	iounmap(test);
	printk("done\n");
}
module_init(init_test);
module_exit(exit_test);
MODULE_LICENSE("GPL");
