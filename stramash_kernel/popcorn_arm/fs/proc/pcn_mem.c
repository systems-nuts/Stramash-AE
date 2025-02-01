// SPDX-License-Identifier: GPL-2.0
#include <linux/cpufreq.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <popcorn/mem_reassign.h>


__weak void arch_freq_prepare_all(void)
{
}

extern const struct seq_operations cpuinfo_op;
static int cpuinfo_open(struct inode *inode, struct file *file)
{
	send_remote_cpu_info_request(0);
	remote_proc_cpu_info(file,0,get_number_cpus_from_remote_node(0));
	arch_freq_prepare_all();
	return seq_open(file, &cpuinfo_op);
}

static const struct file_operations proc_cpuinfo_operations = {
	
	.open		= cpuinfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init proc_cpuinfo_init(void)
{
	proc_create("popcorn_cpuinfo", 0, NULL, &proc_cpuinfo_operations);
	return 0;
}
fs_initcall(proc_cpuinfo_init);
