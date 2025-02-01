#ifndef _LINUX_POPCORN_MEM_REASSIGN_H
#define _LINUX_POPCORN_MEM_REASSIGN_H
#include <popcorn/pcn_kmsg.h>


/* Physical memory reassign */

#define MEM_REASSIGN_REQUEST_FIELDS \
        int nid; \
        int origin_ws; 

DEFINE_PCN_KMSG(mem_reassign_request_t, MEM_REASSIGN_REQUEST_FIELDS);

#define MEM_REASSIGN_RESPONSE_FIELDS \
        int nid; \
        int origin_ws; \
        unsigned long long offline_start; \
        unsigned long offline_size; \
        bool success;   

DEFINE_PCN_KMSG(mem_reassign_response_t, MEM_REASSIGN_RESPONSE_FIELDS);
static void process_mem_reassign_request(struct work_struct *work);
static int handle_mem_reassign_response(struct pcn_kmsg_message *inc_msg);
mem_reassign_response_t *send_mem_reassign_request(unsigned int nid);
#endif
