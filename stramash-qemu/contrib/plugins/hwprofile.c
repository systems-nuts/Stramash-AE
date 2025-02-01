/*
 * Copyright (C) 2020, Alex Bennée <alex.bennee@linaro.org>
 * usage example:
 *
 * #if defined(CONFIG_X86_64)
 *   #define ARCH_TRIGGER_ADDR = 0x0f200000ULL
 * #elif defined(CONFIG_ARM64)
 *   #define ARCH_TRIGGER_ADDR = 0x1200000000ULL
 * #endif
 *
 * void *trigger_address = memremap(ARCH_TRIGGER_ADDR, 0x10, MEMREMAP_WB);
 * memset(trigger_address, 0, 0x10);
 * memunmap(trigger_address);
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <math.h>
#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

typedef enum
{
    PROF_ARCH_AARCH64,
    PROF_ARCH_X86_64,
    PROF_ARCH_INCOMPATIBLE
} ProfileArch;

static uint8_t icount_on;
static bool enable_log;
static uint64_t insn_count;
static FILE *fp;
static enum qemu_plugin_mem_rw rw = QEMU_PLUGIN_MEM_RW;
static ProfileArch prof_arch;
static uint32_t prof_cost;
static char *shm_path;
const uint64_t shm_size = 0x00100000;
static void *shm_ptr;
static uint64_t prof_base;
static char *primesim_socket_path;
static int primesim_socket_fd;

int client_connect_socket(const char *server_socket_path)
{
    int client_sock, rc, len;
    struct sockaddr_un server_sockaddr;
    char buf[256];
    memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));

    /**************************************/
    /* Create a UNIX domain stream socket */
    /**************************************/
    client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_sock == -1)
    {
        printf("SOCKET ERROR = %s\n", strerror(errno));
        exit(1);
    }

    server_sockaddr.sun_family = AF_UNIX;
    strcpy(server_sockaddr.sun_path, server_socket_path);
    rc = connect(client_sock, (struct sockaddr *)&server_sockaddr, sizeof(struct sockaddr_un));
    if (rc == -1)
    {
        printf("CONNECT ERROR = %s\n", strerror(errno));
        close(client_sock);
        exit(1);
    }

    return client_sock;
}

void client_send_message(int client_sock, uint64_t buf)
{
    int ret;
    do
    {
        ret = write(client_sock, (void *)&buf, sizeof(buf));
    } while (ret <= 0);
}
void server_recv(int server_sock, uint64_t *buf)
{
    int ret = 0;
    do
    {
        ret = read(server_sock, buf, sizeof(uint64_t));
    } while (ret <= 0);
}

void client_close_socket(int socket_fd)
{
    close(socket_fd);
}

static void plugin_init(void)
{
    icount_on = 1;
    insn_count = 0;
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    g_autofree gchar *out = g_strdup_printf("insns: %" PRIu64 "\n", insn_count);
    qemu_plugin_outs(out);
    munmap(shm_ptr, shm_size);
    shm_unlink(shm_path);

    if (1)
    {
        client_send_message(primesim_socket_fd, 8);
        client_send_message(primesim_socket_fd, 8);
        client_send_message(primesim_socket_fd, 8);
        client_send_message(primesim_socket_fd, 8);
        client_send_message(primesim_socket_fd, 8);
        client_send_message(primesim_socket_fd, 8);

        printf("done\n");
    }
    client_close_socket(primesim_socket_fd);
}

static void toggle_icount(void)
{
    icount_on = icount_on == 1 ? 0 : 1;
}

// int check_icount(void)
// {
//     if (!icount_on)
//         return 1;
//     else
//         return 0;
// }
static int stramash_index = 0;
static uint64_t stramash_bias = 0;

static void vcpu_haddr(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                       uint64_t vaddr, void *udata)
{
    struct qemu_plugin_hwaddr *hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);
    uint64_t insn_diff;
    // if (!hwaddr || !qemu_plugin_hwaddr_is_io(hwaddr))
    if (!hwaddr)
        return;

    uint64_t phy_addr = qemu_plugin_hwaddr_phys_addr(hwaddr);
    uint64_t *haddr = (uint64_t *)udata;
    bool c;
    uint64_t delta_bias = 0;
    // STRMASH TIMING MODEL
    uint64_t bias = qemu_plugin_get_bias();
    if (bias > stramash_bias)
    {
        delta_bias = bias - stramash_bias;
        stramash_bias = bias;
    }
    if (icount_on)
    {
        insn_count += delta_bias;
        stramash_index += 1;
        c = qemu_plugin_mem_is_store(meminfo);
        //		client_send_message(primesim_socket_fd, *haddr);
        /*
                if(stramash_index % 1000000 == 0)
                {
                //	printf("%d addr %lx type %lx\n",insn_count, phy_addr, c);
                    //printf("it's time to sync\n");
                    do
                    {
                        server_recv(primesim_socket_fd,&insn_diff);
                        //printf("received %ld\n",insn_diff);
                    }while(insn_diff != 22222);
                    server_recv(primesim_socket_fd,&insn_diff);
                    insn_count+=insn_diff;
                    qemu_plugin_feedback(insn_diff);
                }
               
        if(stramash_index % 1000000 == 0)
        {
            // printf("icount local %ld bias %ld icount real %ld feedback %ld\n",insn_count, bias, qemu_plugin_get_icount(), insn_diff);
        }*/
        client_send_message(primesim_socket_fd, insn_count);

        client_send_message(primesim_socket_fd, c);
        client_send_message(primesim_socket_fd, phy_addr);
    }

    if (phy_addr >> 8 != prof_base)
    {
        return;
    }

    uint32_t offset = phy_addr & 0xFF;

    uint64_t reset_cnt;

    void *rst_shm_offset, *ctr_shm_offset;
    switch (phy_addr & 0xFF)
    {
    case 0x00:
        insn_count -= prof_cost;
        toggle_icount();
        break;
    case 0x08:
        break;
    case 0x10:
        break;
    case 0x18:
        rst_shm_offset = (void *)(((uint64_t *)shm_ptr) + 2);
        memcpy(&reset_cnt, rst_shm_offset, sizeof(uint64_t));
        if (reset_cnt == 0xdeadbeefULL)
        {
            insn_count = 0;
            memset(rst_shm_offset, 0, sizeof(uint64_t));
        }
        ctr_shm_offset = (void *)(((uint64_t *)shm_ptr) + 1);
        memcpy(ctr_shm_offset, &insn_count, sizeof(uint64_t));
        break;
    default:
        break;
    }

    return;
}

static void vcpu_insn_exec_before(unsigned int cpu_index, void *udata)
{
    insn_count += icount_on;
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    size_t i;

    for (i = 0; i < n; i++)
    {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t vaddr = qemu_plugin_insn_vaddr(insn);
        qemu_plugin_register_vcpu_insn_exec_cb(
            insn, vcpu_insn_exec_before, QEMU_PLUGIN_CB_NO_REGS,
            GUINT_TO_POINTER(vaddr));

        uint64_t haddr = (uint64_t)qemu_plugin_insn_haddr(insn);

        //        client_send_message(primesim_socket_fd, haddr);
        gpointer udata = (gpointer)(qemu_plugin_insn_vaddr(insn));
        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_haddr,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         rw, udata);
    }
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    int i;
    g_autoptr(GString) matches_raw = g_string_new("");
//Tong PATCH START
	enable_log = true;
	prof_cost = 1;
    for (i = 0; i < argc; i++) {
        char *opt = argv[i];
        g_autofree char **tokens = g_strsplit(opt, "=", 2);

        if (g_strcmp0(tokens[0], "track") == 0) {
            if (g_strcmp0(tokens[1], "read") == 0) {
                rw = QEMU_PLUGIN_MEM_R;
            } else if (g_strcmp0(tokens[1], "write") == 0) {
                rw = QEMU_PLUGIN_MEM_W;
			} else if (g_strcmp0(tokens[1], "log") == 0) {
				enable_log = true;
			} else if (g_str_has_prefix(opt, "overhead") == 0){
				gchar **parts = g_strsplit(opt, "=", 2);
				prof_cost = atoi(parts[1]);
				fprintf(stderr, "prof_cost %d\n", prof_cost);
				g_strfreev(parts);
            } else {
                fprintf(stderr, "invalid value for track: %s\n", tokens[1]);
                return -1;
            }
			/*
        } else if (g_strcmp0(tokens[0], "pattern") == 0) {
            if (!qemu_plugin_bool_parse(tokens[0], tokens[1], &pattern)) {
                fprintf(stderr, "boolean argument parsing failed: %s\n", opt);
                return -1;
            }
        } else if (g_strcmp0(tokens[0], "source") == 0) {
            if (!qemu_plugin_bool_parse(tokens[0], tokens[1], &source)) {
                fprintf(stderr, "boolean argument parsing failed: %s\n", opt);
                return -1;
            }
        } else if (g_strcmp0(tokens[0], "match") == 0) {
            check_match = true;
            g_string_append_printf(matches_raw, "%s,", tokens[1]);
			*/

        } else {
            fprintf(stderr, "option parsing failed: %s\n", opt);
            return -1;
        }
    }
	printf("qemu target is %s\n", info->target_name);
	// detect system architecture
    if (strcmp(info->target_name, "aarch64") == 0)
    {
        prof_arch = PROF_ARCH_AARCH64;
        shm_path = "/icount_aarch64";
        prof_base = 0x0f2000ULL;
        // FIXME: change the arch
        primesim_socket_path = "/tmp/hwprofile_aarch64";
    }
    else if (strcmp(info->target_name, "x86_64") == 0)
    {
        prof_arch = PROF_ARCH_X86_64;
        shm_path = "/icount_x64";
        prof_base = 0x12000000ULL;
        primesim_socket_path = "/tmp/hwprofile_x64";
    }
    else
    {
        prof_arch = PROF_ARCH_INCOMPATIBLE;
    }

    int fd = shm_open(shm_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRWXU);
    if (fd == -1)
    {
        printf("failed to shm_open\n");
        return -1;
    }

    if (ftruncate(fd, shm_size) < 0)
    {
        printf("truncation failure\n");
        return -1;
    }

    shm_ptr = mmap(NULL, shm_size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        printf("mmap failed\n");
        return -1;
    }

/*
    if (check_match) {
        matches = g_strsplit(matches_raw->str, ",", -1);
    }
    if (source && pattern) {
        fprintf(stderr, "can only currently track either source or pattern.\n");
        return -1;
    }
*/
    if (!info->system_emulation) {
        fprintf(stderr, "hwprofile: plugin only useful for system emulation\n");
        return -1;
    }

    /* Just warn about overflow */
    if (info->system.smp_vcpus > 64 ||
        info->system.max_vcpus > 64) {
        fprintf(stderr, "hwprofile: can only track up to 64 CPUs\n");
    }
	if (enable_log)
    {
        primesim_socket_fd = client_connect_socket(primesim_socket_path);
        if (primesim_socket_fd == -1)
        {
            fprintf(stderr, "failed to connect to primesim\n");
            return -1;
        }
    }

    plugin_init();

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    return 0;
}
