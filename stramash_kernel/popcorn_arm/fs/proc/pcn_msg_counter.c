// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>


static uint64_t msg_count;
EXPORT_SYMBOL(msg_count);
static int msg_type_counter[40] = {0};
EXPORT_SYMBOL(msg_type_counter);

int allocated_pages;
EXPORT_SYMBOL(allocated_pages);
int remote_page_fault;
EXPORT_SYMBOL(remote_page_fault);

#ifdef CONFIG_ARM64
extern int ioremap_bypass,ioremap_total,ioremap_fucked;
#endif

static int page_read(struct seq_file *m, void *v)
{
        seq_printf(m, ",allocated_pages: %llu\n",allocated_pages);
        return 0;
}
static int page_reset(struct seq_file *m, void *v)
{
        allocated_pages =0;
        seq_printf(m, "reset pages: %llu\n",allocated_pages);
        return 0;
}

static int do_page_read(struct seq_file *m, void *v)
{
        seq_printf(m, "remote_page_fault: %llu\n",remote_page_fault);
        return 0;
}
static int do_page_reset(struct seq_file *m, void *v)
{
        remote_page_fault =0;
        seq_printf(m, "reset pages: %llu\n",remote_page_fault);
        return 0;
}



static int icount_read(struct seq_file *m, void *v)
{
        seq_printf(m, "%llu\n",msg_count);
	int i;
	for(i=0;i<40;i++)
	{
		printk("%d:%d\n",i,msg_type_counter[i]);
	}
        return 0;
}
static int icount_reset(struct seq_file *m, void *v)
{
	msg_count =0;
        seq_printf(m, "reset msg_count: %llu\n",msg_count);
	int i;
	for(i=0;i<40;i++)
        {
		msg_type_counter[i]=0;
        }
        return 0;
}
#ifdef CONFIG_ARM64
static int ioremap_reset(struct seq_file *m, void *v)
{
	//seq_printf(m,"total software walk %d, message %d, total fault %d\n",ioremap_total,ioremap_bypass,ioremap_fucked);
	seq_printf(m,"message copy %d, message non-copy %d, total fault %d\n",ioremap_total,ioremap_bypass,ioremap_fucked);
	ioremap_bypass=0;
	ioremap_total=0;
	ioremap_fucked=0;
}
#endif
static int __init proc_cmdline_init(void)
{
	msg_count=0;
	allocated_pages=0;
	remote_page_fault=0;
	proc_create_single("stramash_page_read", 0, NULL, page_read);
        proc_create_single("stramash_page_reset", 0, NULL, page_reset);
	
	proc_create_single("stramash_do_page_read", 0, NULL, do_page_read);
        proc_create_single("stramash_do_page_reset", 0, NULL, do_page_reset);

	
	proc_create_single("popcorn_msg_read", 0, NULL, icount_read);
	proc_create_single("popcorn_msg_reset", 0, NULL, icount_reset);
#ifdef CONFIG_ARM64
	proc_create_single("popcorn_ioremap_reset", 0, NULL, ioremap_reset);
#endif
	return 0;
}
fs_initcall(proc_cmdline_init);
