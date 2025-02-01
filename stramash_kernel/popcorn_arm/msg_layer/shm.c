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

#define IPI_IPI_IPI
/* RING BUFFER */
#define RB_SIZE 1024
#define PCN_KMSG_RBUF_SIZE 1024


extern void *remote_address_space,*remote_address_space_high;
extern int msg_type_counter[40];
/* Physical meminfo */
#ifdef CONFIG_POPCORN_REMOTE_INFO
static struct sysinfo meminfo;
extern int popcorn_shm_slices;
extern int MAX_SHM_SLICES;
extern uint64_t msg_count;
#endif 
extern int popcorn__signal;
extern void interrupt_to_core(uint16_t);
extern int (*popcorn_ipi_handler_ptr)(void);
enum {
	SEND_FLAG_POSTED = 0,
};
static unsigned long insurance_recv, insurance_send=0;
/* Per-node handle for shared memory buffer */
struct shm_handle {
	int nid;
	struct pcn_kmsg_message *msg;
	struct pcn_kmsg_window *win_tx;
	struct pcn_kmsg_window *win_rx;
	struct task_struct *recv_handler;
};
static struct shm_handle *shm_handler;
struct pcn_kmsg_window {
	volatile unsigned long head;
	volatile unsigned long tail;
	volatile unsigned char int_enabled;
	volatile struct pcn_kmsg_message buffer[PCN_KMSG_RBUF_SIZE];
	//volatile int second_buffer[PCN_KMSG_RBUF_SIZE];
}__attribute__((packed));

// RING BUFFER RELATED

/* From Wikipedia page "Fetch and add", modified to work for u64 */
// fetch and add: FAA, atomic add a value to a memory variable
static inline unsigned long fetch_and_add(volatile unsigned long * variable,
                                          unsigned long value)
{
        asm volatile(
                     "lock; xaddq %%rax, %2;"
                     :"=a" (value)                   //Output
                     : "a" (value), "m" (*variable)  //Input
                     :"memory" );
        return value;
}

static inline unsigned long win_inuse(struct pcn_kmsg_window *win) 
{
	return win->head - win->tail; //if head = tail, ring buffer not in use
}

// put one message to the window
static inline int win_put(struct pcn_kmsg_window *win, 
			  struct pcn_kmsg_message *msg,
			  int no_block,
			  size_t size) 
{
	unsigned long ticket=insurance_send;
	if(msg->header.type < 0 || msg->header.type >= PCN_KMSG_TYPE_MAX)
        {

                printk("fuck %s msg->header.type: %d size: %d, from_nid: %d\n",__func__,msg->header.type,msg->header.size,msg->header.from_nid);
                return 101;
        }

	if (no_block &&(win_inuse(win) >= RB_SIZE)) {
		printk("window full, caller should try again...\n");
		return -EAGAIN;
	}
	//copy msg to the ring buffer
	memcpy((void*)&win->buffer[ticket%PCN_KMSG_RBUF_SIZE].payload,
	       (void*)&msg->payload, size);

	memcpy((void*)&(win->buffer[ticket%PCN_KMSG_RBUF_SIZE].header),
	       (void*)&(msg->header), sizeof(struct pcn_kmsg_hdr));
	
	win->buffer[ticket%PCN_KMSG_RBUF_SIZE].ready = 1;
	
	mb();	
again:
        // grab ticket 
        ticket = __sync_fetch_and_add(&win->head, 1);
        while(unlikely(ticket-insurance_send>1)) //easy fix for cache error
        {
                printk("cache head coherrance error, reset...%ld \n",ticket
);
                win->head=insurance_send;
                printk("win_head reset %ld\n",win->head);
                goto again;
        }

        //printk("head: %ld\n",ticket);
        insurance_send=win->head;
	win->buffer[ticket%PCN_KMSG_RBUF_SIZE].last_ticket = insurance_send;

	//printk("%s: ticket = %lu, head = %lu, tail = %lu\n",
          //               __func__, ticket, win->head, win->tail);
	return 0;
}

//get one message from the window
static inline int win_get(struct pcn_kmsg_window *win,
				        struct pcn_kmsg_message **msg)
{
	struct pcn_kmsg_message *rcvd;
	if (!win_inuse(win)) {
		return -1;
	}
	while(unlikely(win->tail-insurance_recv!=0))//easy fix for cache error
	{
		printk("cache tail coherrance error, reset...%lld \n",win->tail);
		win->tail=insurance_recv;
		printk("win_tail reset %ld\n",win->tail);
	}
	//printk("tail: %ld\n",win->tail);
	rcvd =(struct pcn_kmsg_message*) &(win->buffer[win->tail % PCN_KMSG_RBUF_SIZE]);
	insurance_recv=win->buffer[win->tail % PCN_KMSG_RBUF_SIZE].last_ticket;
	__sync_fetch_and_add(&win->tail, 1);
	mb();
	*msg=rcvd;

	return 0;
}

//init
static inline int pcn_kmsg_window_init(struct pcn_kmsg_window *window)
{
	window->head = 0;
	window->tail = 0;
	memset((void*)window->buffer, 0,
	       sizeof(struct pcn_kmsg_window));
	window->int_enabled = 1;
	return 0;
}

/*
 * win_enable_int
 * win_disable_int
 * win_int_enabled
 *
 * These functions will inhibit senders to send a message while
 * the receiver is processing IPI from any sender.
 *
static inline void win_enable_int(struct pcn_kmsg_window *win) {
	        win->int_enabled = 1;
	        wmb(); // enforce ordering
}
static inline void win_disable_int(struct pcn_kmsg_window *win) {
	        win->int_enabled = 0;
	        wmb(); // enforce ordering
}
static inline unsigned char win_int_enabled(struct pcn_kmsg_window *win) {
    		rmb(); //not sure this is required (Antonio)
	        return win->int_enabled;
}

*/

/**
 * Handle inbound messages 
 */

static int __pcn_kmsg_receive(struct shm_handle *sh)
{
	// receive the message, then check the sanity, then process it.
	struct pcn_kmsg_window* win = shm_handler->win_rx;
	struct pcn_kmsg_message *msg;
	struct pcn_kmsg_hdr header;
	struct pcn_kmsg_message *data;
	unsigned long insurance,ticket;
pull_msg:
	while (!win_get(win, &msg) ) {
		header = msg->header;	
	#ifdef CONFIG_POPCORN_CHECK_SANITY
		BUG_ON(header.type < 0 || header.type >= PCN_KMSG_TYPE_MAX);
		BUG_ON(header.size < 0 || header.size >  PCN_KMSG_MAX_SIZE);
	#endif
				
		/* compose body */
		
		data = kmalloc(sizeof(struct pcn_kmsg_message), GFP_KERNEL);
		BUG_ON(!data && "Unable to alloc a message");
		int i;
		data->header=header;
		for(i =0; i<header.size;i++)
			data->payload[i]=msg->payload[i];
	
		pcn_kmsg_process(data);
		
		mb();
		msg->ready = 0;
		insurance=win->tail;
		__sync_fetch_and_add(&win->tail, 1);
		}
//	win_enable_int(win);
	if ( win_inuse(win) ) {
//		win_disable_int(win);
		goto pull_msg;
	}
	return 0;
}
/*
	IPI handler
*/
int IPI__pcn_kmsg_receive(void)
{
        struct pcn_kmsg_window* win = shm_handler->win_rx;
        struct pcn_kmsg_message *msg;
        struct pcn_kmsg_hdr header;
	struct pcn_kmsg_message *data;
	
pull_msg:
        if(!win_get(win, &msg) ) {
                header = msg->header;
        #ifdef CONFIG_POPCORN_CHECK_SANITY
                BUG_ON(header.type < 0 || header.type >= PCN_KMSG_TYPE_MAX);
                BUG_ON(header.size < 0 || header.size >  PCN_KMSG_MAX_SIZE);
        #endif
		msg_type_counter[header.type]+=1;
               /* compose body */
		data = kmalloc(sizeof(struct pcn_kmsg_message), GFP_KERNEL); //will be free in side pcn_kmsg_process.
		BUG_ON(!data && "Unable to alloc a message");
		memcpy((void*)&data->payload,(void*)&msg->payload, header.size);
		memcpy((void*)&data->header,(void*)&msg->header, sizeof(header));
		//printk("%s msg->size: %d\n",__func__,msg->header.size);
		pcn_kmsg_process(data);

		mb();
                msg->ready = 0;
        }
	msg_count++;
	if ( win_inuse(win) ) {
                goto pull_msg;
        }
        return 0;
}
EXPORT_SYMBOL(IPI__pcn_kmsg_receive);

static int recv_handler(void* arg0)
{
	struct shm_handle *sh = arg0;
	printk("RECV handler for %d is ready\n", sh->nid);
	while (!kthread_should_stop()) { 
		msleep(1); //polling solution, later update it to lovely IPI
		__pcn_kmsg_receive(sh);
	}
	return 0;
}


/**
 * Handle outbound messages
 */
static int shm__pcn_kmsg_send(struct shm_handle *sh,size_t size)
{
	int rc,no_block=0;
	struct pcn_kmsg_window *dest_window = sh->win_tx;
	struct pcn_kmsg_message *msg = sh->msg;
	if (unlikely(!dest_window)) {
		printk("!dest_window\n");
		return -1;
	}

	if (unlikely(!msg)) {
		printk("Passed in a null pointer to msg!\n");
		return -1;
	}

	/* set source CPU */
	//printk("%s msg->size: %d\n",__func__,msg->header.size);
	//printk("%s msg->header.type: %d size: %d, from_nid: %d\n",__func__,msg->header.type,msg->header.size,msg->header.from_nid);
	rc = win_put(dest_window, msg, no_block,size); //no_block
	if (rc) {
		if (rc == 101)
		{
			msg = sh->msg;
			printk("%s msg->header.type: %d size: %d, from_nid: %d\n",__func__,msg->header.type,msg->header.size,msg->header.from_nid);
			return EAGAIN;
		}
		if (no_block && (rc == EAGAIN)) {
			return rc;
		}
		printk("Failed to place message in dest win!\n");
		return -1;
	}

#ifdef IPI_IPI_IPI
#if 1 
	//udelay(350);
	arch_send_call_function_cross_arch_ipi();
#else 
#ifdef CONFIG_ARM64
	interrupt_to_core(2);
#else
	interrupt_to_core(0);
#endif
#endif 
#endif

	return 0;
}

#define WORKAROUND_POOL
/***********************************************
 * Manage send buffer, other code may call to get the message
 ***********************************************/
struct pcn_kmsg_message *shm_kmsg_get(size_t size)
{
	struct pcn_kmsg_message *msg;
	might_sleep();
	msg = kmalloc(size, GFP_KERNEL);
	
	return msg;	
}

void shm_kmsg_put(struct pcn_kmsg_message *msg)
{
	kfree(msg);
}


/***********************************************
 * This is the interface for message layer
 ***********************************************/
int shm_kmsg_send(int dest_nid, struct pcn_kmsg_message *msg, size_t size)
{
	struct shm_handle *sh = shm_handler;
	sh->msg=msg;
	shm__pcn_kmsg_send(sh,size);
	return 0;
}

int shm_kmsg_post(int dest_nid, struct pcn_kmsg_message *msg, size_t size)
{
	struct shm_handle *sh = shm_handler;
	sh->msg=msg;
	shm__pcn_kmsg_send(sh,size);
	return 0;
}


// release the msg
void shm_kmsg_done(struct pcn_kmsg_message *msg)
{
	kfree(msg);
	
}
// keep same
void shm_kmsg_stat(struct seq_file *seq, void *v)
{
	if (seq) {
		seq_printf(seq, POPCORN_STAT_FMT,
				"socket");
	}
}

struct pcn_kmsg_transport transport_shm = {
	.name = "shm",
	.features = 0,

	.get = shm_kmsg_get,
	.put = shm_kmsg_put,
	.stat = shm_kmsg_stat,

	.send = shm_kmsg_send,
	.post = shm_kmsg_post,
	.done = shm_kmsg_done,
};

/*
 * Check for memory usuage
 *
 */

extern void do_hot_plug(mem_reassign_response_t *mem_reassign);

int __check_hot_plug_ok(void)
{
	si_meminfo(&meminfo);
        if(unlikely(meminfo.freeram * 4 < meminfo.totalram)){
		if (popcorn_shm_slices==MAX_SHM_SLICES) return 0; //I own all the memory slices
		//if (!migration_check_ok) return 0; //during migration failed
		else return 1;
	}
	else return 0;
}
static int __memory_allocator(void* arg0)
{
	mem_reassign_response_t *mem_reassign;
	while (!kthread_should_stop()) {
                msleep(1000);
		if(unlikely(__check_hot_plug_ok()))
		{
			printk("freemem small than 25 percent, send message!\n");
#ifdef CONFIG_ARM64
		        mem_reassign = send_mem_reassign_request(0);
#else
			mem_reassign = send_mem_reassign_request(1);
#endif
			do_hot_plug(mem_reassign);
		}
        }	
        return 0;
}
static int * __init __start_memory_allocator(const int nid)
{
	struct task_struct *tsk_allocator;
	tsk_allocator = kthread_run(__memory_allocator,NULL,"mem_allocator");
	if (IS_ERR(tsk_allocator)) {
		kthread_stop(tsk_allocator);
		return PTR_ERR(tsk_allocator);
	}
	return 0;
}

/*
 Init	
*/

static struct task_struct * __init __start_handler(const int nid, const char *type, int (*handler)(void *data))
{
	char name[40];
	struct task_struct *tsk;

	sprintf(name, "pcn_%s_%d", type, nid);
	tsk = kthread_run(handler, shm_handler, name);
	if (IS_ERR(tsk)) {
		printk(KERN_ERR "Cannot create %s handler, %ld\n", name, PTR_ERR(tsk));
		return tsk;
	}

	return tsk;
}

static int __start_handlers(const int nid)
{
	struct task_struct *tsk_recv;

	tsk_recv = __start_handler(nid, "recv", recv_handler);
	if (IS_ERR(tsk_recv)) {
		kthread_stop(tsk_recv);
		return PTR_ERR(tsk_recv);
	}
	shm_handler->recv_handler = tsk_recv;
	return 0;
}
/*
//TODO add more better way to check the connection. 

static int __init __check_connection(void)
{
	msleep(5000);
	int ret;
	struct pcn_kmsg_window* win_send = shm_handler->win_tx;
#ifdef CONFIG_ARM64
	win_send->buffer[0].payload[0] = 'l';
	win_send->buffer[0].payload[1] = 'm';
#else
	win_send->buffer[0].payload[0] = 'b';
	win_send->buffer[0].payload[1] = 'l';
#endif
	mb();
	return 0;

}
*/
static void __exit exit_kmsg_shm(void)
{
	//kfree(shm_handler);
	printk("Successfully unloaded module!\n");
}
static int __init init_kmsg_shm(void)
{
#if 1 
#ifdef IPI_IPI_IPI
	popcorn_ipi_handler_ptr=IPI__pcn_kmsg_receive; //global pointer point to handler function.
#endif
	int i, ret; 
	struct pcn_kmsg_window *shm_tx, *shm_rx, *win_virt_addr;
	
	printk("Loading Popcorn messaging layer over Shard Memory...\n");
#ifdef CONFIG_ARM64
        shm_rx = (struct pcn_kmsg_window*)memremap(0x80000000,sizeof(struct pcn_kmsg_window),MEMREMAP_WB); //receive buffer
        shm_tx = (struct pcn_kmsg_window*)memremap(0x90000000,sizeof(struct pcn_kmsg_window),MEMREMAP_WB); //send buffer
#else
        shm_tx = (struct pcn_kmsg_window*)memremap(0x40000000,sizeof(struct pcn_kmsg_window),MEMREMAP_WB); //send buffer
        shm_rx = (struct pcn_kmsg_window*)memremap(0x50000000,sizeof(struct pcn_kmsg_window),MEMREMAP_WB); //receive buffer
#endif

	/*init the ring buffer window*/
	pcn_kmsg_window_init(shm_tx);
	pcn_kmsg_window_init(shm_rx);
	/*allocate the pcn_kmesg_reverse_message buffer to the physical address.
	shm_tx -> [ head | tail | int_enable | buffer [ kmsg1 | kmsg2 | kmsg3 | ... ] | empty ] 
	*/
	pcn_kmsg_set_transport(&transport_shm);

	shm_handler = kmalloc(sizeof(struct shm_handle),GFP_KERNEL);
	struct shm_handle *sh = shm_handler;
	sh->win_tx = shm_tx;
	sh->win_rx = shm_rx;
	_identify_myself();

	sh->nid = my_nid;
	/* Wait for a while so that nodes are ready to listen to connections */
	set_popcorn_node_online(0,true);
	set_popcorn_node_online(1,true);
	msleep(5000);
#ifndef IPI_IPI_IPI
	__start_handlers(sh->nid);
#endif
#ifdef CONFIG_POPCORN_REMOTE_INFO 
	printk("popcorn_shm_slices : %d\n",popcorn_shm_slices);
	//__start_memory_allocator(my_nid);
#endif
	//printk("start_handeler done\n");
	printk("Ready on Shared Memory\n");
	broadcast_my_node_info(2);
	printk("done broadcast_\n");
#ifdef CONFIG_X86_64
	printk("online_pages(0x100000,0x100000,MMOP_ONLINE_MOVABLE)");
	online_pages(0x100000,0x100000,MMOP_ONLINE_MOVABLE);
#else
	printk("online_pages(0x150000,0x50000,MMOP_ONLINE_MOVABLE)");
    online_pages(0x150000,0x50000,MMOP_ONLINE_MOVABLE);

#endif
#endif
	printk("%lx\n",(UL(1) << (VA_BITS - 1)));
        printk("Direct mapping %lx -- 0xffffffffffffffff\n",PAGE_OFFSET);
        printk("KIMAGE_VADDR %lx\n",KIMAGE_VADDR);
        printk("MODULES_VADDR %lx -- %lx\n",MODULES_VADDR,MODULES_END);
        printk("VMEMMAP_START %lx -- %lx\n",VMEMMAP_START,PAGE_OFFSET);
        printk("PCI_IO_START %lx -- %lx\n",PCI_IO_START,PCI_IO_END);
	printk("VMALLOC_START %lx -- %lx\n",VMALLOC_START,VMALLOC_END);
	printk("remote address space map\n");
    remote_address_space = ioremap(0x40000000, 0xc0000000);
	remote_address_space_high = ioremap(0x100000000, 0x13fffffff);
	printk(" (PGDIR_SHIFT %lx + PAGE_SHIFT %lx - 3)\n ", PGDIR_SHIFT,PAGE_SHIFT,(PGDIR_SHIFT + PAGE_SHIFT - 3),(PGDIR_SHIFT + PAGE_SHIFT - 3));	
	//the ioremap range is allign with the remote direct map range
    printk("done, remote_address_space %lx remote_address_space_high %lx\n",remote_address_space,remote_address_space_high);
	struct page *test_page = alloc_page(GFP_KERNEL);
	printk("page %lx page* %lx phys %lx\n",test_page,*test_page,page_to_phys(test_page));
	return 0;


out_exit:
	exit_kmsg_shm();
//	return ret;
}


module_init(init_kmsg_shm);
module_exit(exit_kmsg_shm);
MODULE_LICENSE("GPL");
