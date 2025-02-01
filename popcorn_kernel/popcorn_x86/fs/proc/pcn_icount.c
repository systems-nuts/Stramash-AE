// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/io.h>
#if defined(CONFIG_X86_64)
  #define ARCH_TRIGGER_ADDR 0x1200000000
#elif defined(CONFIG_ARM64)
  #define ARCH_TRIGGER_ADDR 0x0f200000
#endif

static uint64_t ipi_count;
EXPORT_SYMBOL(ipi_count);
static int icount_switch(struct seq_file *m, void *v)
{
        void __iomem *trigger_address = ioremap(ARCH_TRIGGER_ADDR, 0x1000);
        readl(trigger_address);
        iounmap(trigger_address);
	seq_printf(m, "switch icount\n");
	return 0;
}
static int icount_read(struct seq_file *m, void *v)
{
	uint64_t rt;
        void __iomem *trigger_address = ioremap(ARCH_TRIGGER_ADDR, 0x1000);
	readl(trigger_address+0x18);
        rt=readq(trigger_address+8);
        iounmap(trigger_address);
        seq_printf(m, "%llu\n",rt);
        return 0;
}
static int icount_reset(struct seq_file *m, void *v)
{
        void __iomem *trigger_address = ioremap(ARCH_TRIGGER_ADDR, 0x1000);
        writel(0xdeadbeef,trigger_address+16);
        iounmap(trigger_address);
        seq_printf(m, "reset\n");
        return 0;
}
static int ipi_read(struct seq_file *m, void *v)
{
	seq_printf(m, "ipi %llu\n",ipi_count);
}
static int ipi_reset(struct seq_file *m, void *v)
{
	seq_printf(m, "ipi %llu\n",ipi_count);
	seq_printf(m, "ipi reset\n");
	ipi_count=0;
	seq_printf(m, "ipi %llu\n",ipi_count);
}
static int __init proc_cmdline_init(void)
{
	ipi_count=0;
	proc_create_single("popcorn_icount_switch", 0, NULL, icount_switch);
	proc_create_single("popcorn_icount_read", 0, NULL, icount_read);
	proc_create_single("popcorn_icount_reset", 0, NULL, icount_reset);
	proc_create_single("popcorn_ipi_read", 0, NULL, ipi_read);
	proc_create_single("popcorn_ipi_reset", 0, NULL, ipi_reset);
	return 0;
}
fs_initcall(proc_cmdline_init);
