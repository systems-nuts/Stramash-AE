/*
 * ARM GICv3 support - common bits of emulated and KVM kernel model
 *
 * Copyright (c) 2012 Linaro Limited
 * Copyright (c) 2015 Huawei.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Written by Peter Maydell
 * Reworked for GICv3 by Shlomo Pongratz and Pavel Fedin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/core/cpu.h"
#include "hw/qdev-properties-system.h"
#include "hw/intc/arm_gicv3_common.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "gicv3_internal.h"
#include "hw/arm/linux-boot-if.h"
#include "sysemu/kvm.h"

static void gicv3_process_msg_disconnect(GICv3State *s, uint16_t posn, Error **errp)
{
    event_notifier_cleanup(&s->eventfd[posn]);
}

static void gicv3_process_msg_connect(GICv3State *s, uint16_t posn, int fd,
                                Error **errp)
{
  /*
   * The N-th connect message for this peer comes with the file
   * descriptor for vector N-1.  Count messages to find the vector.
   */
  ARMGICv3CommonClass *agcc = ARM_GICV3_COMMON_GET_CLASS(s);
  
  event_notifier_init_fd(&s->eventfd[posn], fd);
  fcntl_setfl(fd, O_NONBLOCK);

  if (posn == s->vm_id) {
      int eventfd = event_notifier_get_fd(&s->eventfd[posn]);
      //TODO: set opaque as null, pass arguments as we see fit
      qemu_set_fd_handler(eventfd, agcc->interrupt_cb, NULL, s);
  }

}

static void process_msg(GICv3State *s, int64_t msg, int fd, Error **errp)
{

  if (msg == -1) {
    //TODO: disable shm in server in the future
    return;
  }

  if (fd >= 0) {
    gicv3_process_msg_connect(s, msg, fd, errp);
  } else {
    gicv3_process_msg_disconnect(s, msg, errp);
  }
}



static int64_t gicv3_recv_msg(GICv3State *s, int *pfd, Error **errp)
{
  int64_t msg;
  int n, ret;

  n = 0;
  do {
    ret = qemu_chr_fe_read_all(&s->chr, (uint8_t *)&msg + n,
                               sizeof(msg) - n);
    if (ret < 0) {
      if (ret == -EINTR) {
        continue;
      }
      error_setg_errno(errp, -ret, "read from server failed");
      return INT64_MIN;
    }
    n += ret;
  } while (n < sizeof(msg));

  *pfd = qemu_chr_fe_get_msgfd(&s->chr);
  return le64_to_cpu(msg);
}

static void gicv3_read(void *opaque, const uint8_t *buf, int size)
{
  GICv3State *s = opaque;
  Error *err = NULL;
  int fd;
  int64_t msg;

  assert(size >= 0 && s->msg_buffered_bytes + size <= sizeof(s->msg_buf));
  memcpy((unsigned char *)&s->msg_buf + s->msg_buffered_bytes, buf, size);
  s->msg_buffered_bytes += size;
  if (s->msg_buffered_bytes < sizeof(s->msg_buf)) {
    return;
  }
  msg = le64_to_cpu(s->msg_buf);
  
  
  s->msg_buffered_bytes = 0;

  fd = qemu_chr_fe_get_msgfd(&s->chr);

  process_msg(s, msg, fd, &err);
  if (err) {
    error_report_err(err);
  }
}

static int gicv3_can_receive(void *opaque)
{
  GICv3State *s = opaque;

  assert(s->msg_buffered_bytes < sizeof(s->msg_buf));
  return sizeof(s->msg_buf) - s->msg_buffered_bytes;
}

static void gicv3_send_interrupt(void *opaque, int target)
{
    if (target > 1) {
        printf("target is too large\n");
        return;
    }
    int recv_buf;
    GICv3State *s = opaque;
    event_notifier_set(&s->eventfd[target]);
    read(s->ipi_fifo_fd_in, &recv_buf, sizeof(recv_buf));
	//THIS IS SEND!! TONG
	//printf("QEMU I'm receving \n");
    if (recv_buf != 0xdead) {
        printf("Data received is %x instead of 0xdead for arm!\n", recv_buf);
    }
}


static void gicv3_recv_setup(GICv3State *s, Error **errp)
{
  Error *err = NULL;
  int64_t msg;
  int fd;

  msg = gicv3_recv_msg(s, &fd, &err);
  if (err) {
    error_propagate(errp, err);
    return;
  }
  if (msg != 0x0) {
    error_setg(errp, "server sent version %" PRId64 ", expecting %d",
               msg, 0x0);
    return;
  }

  //if (fd != -1) {
  //  error_setg(errp, "server sent invalid version message");
  //  return;
  //}


  /*
   * vintc-server sends the remaining initial messages in a fixed
   * order, but the device has always accepted them in any order.
   * Stay as compatible as practical, just in case people use
   * servers that behave differently.
   */

  /*
   * vintc_device_spec.txt has always required the ID message
   * right here, and vintc-server has always complied.  However,
   * older versions of the device accepted it out of order, but
   * broke when an interrupt setup message arrived before it.
   */
  msg = gicv3_recv_msg(s, &fd, &err);
  if (err) {
    error_propagate(errp, err);
    return;
  }
  //if (fd != -1 || msg < 0 || msg > 1) {
  //  error_setg(errp, "server sent invalid ID message");
  //  return;
  //}

  s->vm_id = msg;
  
  /*
   * Receive more messages until we got shared memory.
   */
  do {
    msg = gicv3_recv_msg(s, &fd, &err);
    if (err) {
      error_propagate(errp, err);
      return;
    }
    process_msg(s, msg, fd, &err);
    if (err) {
      error_propagate(errp, err);
      return;
    }
  } while (msg != -1);
}





static void gicv3_gicd_no_migration_shift_bug_post_load(GICv3State *cs)
{
    if (cs->gicd_no_migration_shift_bug) {
        return;
    }

    /* Older versions of QEMU had a bug in the handling of state save/restore
     * to the KVM GICv3: they got the offset in the bitmap arrays wrong,
     * so that instead of the data for external interrupts 32 and up
     * starting at bit position 32 in the bitmap, it started at bit
     * position 64. If we're receiving data from a QEMU with that bug,
     * we must move the data down into the right place.
     */
    memmove(cs->group, (uint8_t *)cs->group + GIC_INTERNAL / 8,
            sizeof(cs->group) - GIC_INTERNAL / 8);
    memmove(cs->grpmod, (uint8_t *)cs->grpmod + GIC_INTERNAL / 8,
            sizeof(cs->grpmod) - GIC_INTERNAL / 8);
    memmove(cs->enabled, (uint8_t *)cs->enabled + GIC_INTERNAL / 8,
            sizeof(cs->enabled) - GIC_INTERNAL / 8);
    memmove(cs->pending, (uint8_t *)cs->pending + GIC_INTERNAL / 8,
            sizeof(cs->pending) - GIC_INTERNAL / 8);
    memmove(cs->active, (uint8_t *)cs->active + GIC_INTERNAL / 8,
            sizeof(cs->active) - GIC_INTERNAL / 8);
    memmove(cs->edge_trigger, (uint8_t *)cs->edge_trigger + GIC_INTERNAL / 8,
            sizeof(cs->edge_trigger) - GIC_INTERNAL / 8);

    /*
     * While this new version QEMU doesn't have this kind of bug as we fix it,
     * so it needs to set the flag to true to indicate that and it's necessary
     * for next migration to work from this new version QEMU.
     */
    cs->gicd_no_migration_shift_bug = true;
}

static int gicv3_pre_save(void *opaque)
{
    GICv3State *s = (GICv3State *)opaque;
    ARMGICv3CommonClass *c = ARM_GICV3_COMMON_GET_CLASS(s);

    if (c->pre_save) {
        c->pre_save(s);
    }

    return 0;
}

static int gicv3_post_load(void *opaque, int version_id)
{
    GICv3State *s = (GICv3State *)opaque;
    ARMGICv3CommonClass *c = ARM_GICV3_COMMON_GET_CLASS(s);

    gicv3_gicd_no_migration_shift_bug_post_load(s);

    if (c->post_load) {
        c->post_load(s);
    }
    return 0;
}

static bool virt_state_needed(void *opaque)
{
    GICv3CPUState *cs = opaque;

    return cs->num_list_regs != 0;
}

static const VMStateDescription vmstate_gicv3_cpu_virt = {
    .name = "arm_gicv3_cpu/virt",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = virt_state_needed,
    .fields = (VMStateField[]) {
        VMSTATE_UINT64_2DARRAY(ich_apr, GICv3CPUState, 3, 4),
        VMSTATE_UINT64(ich_hcr_el2, GICv3CPUState),
        VMSTATE_UINT64_ARRAY(ich_lr_el2, GICv3CPUState, GICV3_LR_MAX),
        VMSTATE_UINT64(ich_vmcr_el2, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    }
};

static int vmstate_gicv3_cpu_pre_load(void *opaque)
{
    GICv3CPUState *cs = opaque;

   /*
    * If the sre_el1 subsection is not transferred this
    * means SRE_EL1 is 0x7 (which might not be the same as
    * our reset value).
    */
    cs->icc_sre_el1 = 0x7;
    return 0;
}

static bool icc_sre_el1_reg_needed(void *opaque)
{
    GICv3CPUState *cs = opaque;

    return cs->icc_sre_el1 != 7;
}

const VMStateDescription vmstate_gicv3_cpu_sre_el1 = {
    .name = "arm_gicv3_cpu/sre_el1",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = icc_sre_el1_reg_needed,
    .fields = (VMStateField[]) {
        VMSTATE_UINT64(icc_sre_el1, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    }
};

static bool gicv4_needed(void *opaque)
{
    GICv3CPUState *cs = opaque;

    return cs->gic->revision > 3;
}

const VMStateDescription vmstate_gicv3_gicv4 = {
    .name = "arm_gicv3_cpu/gicv4",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = gicv4_needed,
    .fields = (VMStateField[]) {
        VMSTATE_UINT64(gicr_vpropbaser, GICv3CPUState),
        VMSTATE_UINT64(gicr_vpendbaser, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_gicv3_cpu = {
    .name = "arm_gicv3_cpu",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_load = vmstate_gicv3_cpu_pre_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(level, GICv3CPUState),
        VMSTATE_UINT32(gicr_ctlr, GICv3CPUState),
        VMSTATE_UINT32_ARRAY(gicr_statusr, GICv3CPUState, 2),
        VMSTATE_UINT32(gicr_waker, GICv3CPUState),
        VMSTATE_UINT64(gicr_propbaser, GICv3CPUState),
        VMSTATE_UINT64(gicr_pendbaser, GICv3CPUState),
        VMSTATE_UINT32(gicr_igroupr0, GICv3CPUState),
        VMSTATE_UINT32(gicr_ienabler0, GICv3CPUState),
        VMSTATE_UINT32(gicr_ipendr0, GICv3CPUState),
        VMSTATE_UINT32(gicr_iactiver0, GICv3CPUState),
        VMSTATE_UINT32(edge_trigger, GICv3CPUState),
        VMSTATE_UINT32(gicr_igrpmodr0, GICv3CPUState),
        VMSTATE_UINT32(gicr_nsacr, GICv3CPUState),
        VMSTATE_UINT8_ARRAY(gicr_ipriorityr, GICv3CPUState, GIC_INTERNAL),
        VMSTATE_UINT64_ARRAY(icc_ctlr_el1, GICv3CPUState, 2),
        VMSTATE_UINT64(icc_pmr_el1, GICv3CPUState),
        VMSTATE_UINT64_ARRAY(icc_bpr, GICv3CPUState, 3),
        VMSTATE_UINT64_2DARRAY(icc_apr, GICv3CPUState, 3, 4),
        VMSTATE_UINT64_ARRAY(icc_igrpen, GICv3CPUState, 3),
        VMSTATE_UINT64(icc_ctlr_el3, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    },
    .subsections = (const VMStateDescription * []) {
        &vmstate_gicv3_cpu_virt,
        &vmstate_gicv3_cpu_sre_el1,
        &vmstate_gicv3_gicv4,
        NULL
    }
};

static int gicv3_pre_load(void *opaque)
{
    GICv3State *cs = opaque;

   /*
    * The gicd_no_migration_shift_bug flag is used for migration compatibility
    * for old version QEMU which may have the GICD bmp shift bug under KVM mode.
    * Strictly, what we want to know is whether the migration source is using
    * KVM. Since we don't have any way to determine that, we look at whether the
    * destination is using KVM; this is close enough because for the older QEMU
    * versions with this bug KVM -> TCG migration didn't work anyway. If the
    * source is a newer QEMU without this bug it will transmit the migration
    * subsection which sets the flag to true; otherwise it will remain set to
    * the value we select here.
    */
    if (kvm_enabled()) {
        cs->gicd_no_migration_shift_bug = false;
    }

    return 0;
}

static bool needed_always(void *opaque)
{
    return true;
}

const VMStateDescription vmstate_gicv3_gicd_no_migration_shift_bug = {
    .name = "arm_gicv3/gicd_no_migration_shift_bug",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = needed_always,
    .fields = (VMStateField[]) {
        VMSTATE_BOOL(gicd_no_migration_shift_bug, GICv3State),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_gicv3 = {
    .name = "arm_gicv3",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_load = gicv3_pre_load,
    .pre_save = gicv3_pre_save,
    .post_load = gicv3_post_load,
    .priority = MIG_PRI_GICV3,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(gicd_ctlr, GICv3State),
        VMSTATE_UINT32_ARRAY(gicd_statusr, GICv3State, 2),
        VMSTATE_UINT32_ARRAY(group, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(grpmod, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(enabled, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(pending, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(active, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(level, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(edge_trigger, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT8_ARRAY(gicd_ipriority, GICv3State, GICV3_MAXIRQ),
        VMSTATE_UINT64_ARRAY(gicd_irouter, GICv3State, GICV3_MAXIRQ),
        VMSTATE_UINT32_ARRAY(gicd_nsacr, GICv3State,
                             DIV_ROUND_UP(GICV3_MAXIRQ, 16)),
        VMSTATE_STRUCT_VARRAY_POINTER_UINT32(cpu, GICv3State, num_cpu,
                                             vmstate_gicv3_cpu, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    },
    .subsections = (const VMStateDescription * []) {
        &vmstate_gicv3_gicd_no_migration_shift_bug,
        NULL
    }
};

void gicv3_init_irqs_and_mmio(GICv3State *s, qemu_irq_handler handler,
                              const MemoryRegionOps *ops)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);
    int i;
    int cpuidx;

    /* For the GIC, also expose incoming GPIO lines for PPIs for each CPU.
     * GPIO array layout is thus:
     *  [0..N-1] spi
     *  [N..N+31] PPIs for CPU 0
     *  [N+32..N+63] PPIs for CPU 1
     *   ...
     */
    i = s->num_irq - GIC_INTERNAL + GIC_INTERNAL * s->num_cpu;
    qdev_init_gpio_in(DEVICE(s), handler, i);

    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_irq);
    }
    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_fiq);
    }
    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_virq);
    }
    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_vfiq);
    }

    memory_region_init_io(&s->iomem_dist, OBJECT(s), ops, s,
                          "gicv3_dist", 0x10000);
    sysbus_init_mmio(sbd, &s->iomem_dist);

    s->redist_regions = g_new0(GICv3RedistRegion, s->nb_redist_regions);
    cpuidx = 0;
    for (i = 0; i < s->nb_redist_regions; i++) {
        char *name = g_strdup_printf("gicv3_redist_region[%d]", i);
        GICv3RedistRegion *region = &s->redist_regions[i];

        region->gic = s;
        region->cpuidx = cpuidx;
        cpuidx += s->redist_region_count[i];

        memory_region_init_io(&region->iomem, OBJECT(s),
                              ops ? &ops[1] : NULL, region, name,
                              s->redist_region_count[i] * gicv3_redist_size(s));
        sysbus_init_mmio(sbd, &region->iomem);
        g_free(name);
    }
}

static void arm_gicv3_common_realize(DeviceState *dev, Error **errp)
{
    GICv3State *s = ARM_GICV3_COMMON(dev);
	ARMGICv3CommonClass *agcc = ARM_GICV3_COMMON_GET_CLASS(s);

    int i, rdist_capacity, cpuidx;

    /*
     * This GIC device supports only revisions 3 and 4. The GICv1/v2
     * is a separate device.
     * Note that subclasses of this device may impose further restrictions
     * on the GIC revision: notably, the in-kernel KVM GIC doesn't
     * support GICv4.
     */
    if (s->revision != 3 && s->revision != 4) {
        error_setg(errp, "unsupported GIC revision %d", s->revision);
        return;
    }

    if (s->num_irq > GICV3_MAXIRQ) {
        error_setg(errp,
                   "requested %u interrupt lines exceeds GIC maximum %d",
                   s->num_irq, GICV3_MAXIRQ);
        return;
    }
    if (s->num_irq < GIC_INTERNAL) {
        error_setg(errp,
                   "requested %u interrupt lines is below GIC minimum %d",
                   s->num_irq, GIC_INTERNAL);
        return;
    }
    if (s->num_cpu == 0) {
        error_setg(errp, "num-cpu must be at least 1");
        return;
    }

    /* ITLinesNumber is represented as (N / 32) - 1, so this is an
     * implementation imposed restriction, not an architectural one,
     * so we don't have to deal with bitfields where only some of the
     * bits in a 32-bit word should be valid.
     */
    if (s->num_irq % 32) {
        error_setg(errp,
                   "%d interrupt lines unsupported: not divisible by 32",
                   s->num_irq);
        return;
    }

    if (s->lpi_enable && !s->dma) {
        error_setg(errp, "Redist-ITS: Guest 'sysmem' reference link not set");
        return;
    }

    rdist_capacity = 0;
    for (i = 0; i < s->nb_redist_regions; i++) {
        rdist_capacity += s->redist_region_count[i];
    }
    if (rdist_capacity != s->num_cpu) {
        error_setg(errp, "Capacity of the redist regions(%d) "
                   "does not match the number of vcpus(%d)",
                   rdist_capacity, s->num_cpu);
        return;
    }

    if (s->lpi_enable) {
        address_space_init(&s->dma_as, s->dma,
                           "gicv3-its-sysmem");
    }

    s->cpu = g_new0(GICv3CPUState, s->num_cpu);

    for (i = 0; i < s->num_cpu; i++) {
        CPUState *cpu = qemu_get_cpu(i);
        uint64_t cpu_affid;

        s->cpu[i].cpu = cpu;
        s->cpu[i].gic = s;
        /* Store GICv3CPUState in CPUARMState gicv3state pointer */
        gicv3_set_gicv3state(cpu, &s->cpu[i]);

        /* Pre-construct the GICR_TYPER:
         * For our implementation:
         *  Top 32 bits are the affinity value of the associated CPU
         *  CommonLPIAff == 01 (redistributors with same Aff3 share LPI table)
         *  Processor_Number == CPU index starting from 0
         *  DPGS == 0 (GICR_CTLR.DPG* not supported)
         *  Last == 1 if this is the last redistributor in a series of
         *            contiguous redistributor pages
         *  DirectLPI == 0 (direct injection of LPIs not supported)
         *  VLPIS == 1 if vLPIs supported (GICv4 and up)
         *  PLPIS == 1 if LPIs supported
         */
        cpu_affid = object_property_get_uint(OBJECT(cpu), "mp-affinity", NULL);

        /* The CPU mp-affinity property is in MPIDR register format; squash
         * the affinity bytes into 32 bits as the GICR_TYPER has them.
         */
        cpu_affid = ((cpu_affid & 0xFF00000000ULL) >> 8) |
                     (cpu_affid & 0xFFFFFF);
        s->cpu[i].gicr_typer = (cpu_affid << 32) |
            (1 << 24) |
            (i << 8);

        if (s->lpi_enable) {
            s->cpu[i].gicr_typer |= GICR_TYPER_PLPIS;
            if (s->revision > 3) {
                s->cpu[i].gicr_typer |= GICR_TYPER_VLPIS;
            }
        }
    }
	
	    if (!qemu_chr_fe_backend_connected(&s->chr)) {
        error_setg(errp, "GICv3 requires a backend connected to a character device");
        return;
    }


    Chardev *chr = qemu_chr_fe_get_driver(&s->chr);
    if (!chr) {
        error_setg(errp, "GICv3 requires a backend connected to a character device");
        return;
    }


    Error *err = NULL;
    agcc->recv_setup(s, &err);

    if (err) {
        error_propagate(errp, err);
        return;
    }

    qemu_chr_fe_set_handlers(&s->chr, agcc->can_read_cb,
                             agcc->read_cb, NULL, NULL, s, NULL, true);

    /* create a named fifo */
    //s->ipi_fifo_name = "/tmp/cross_arch_ipi_fifo";
    //mkfifo(s->ipi_fifo_name, 0666);
    //s->ipi_fifo_fd = open(s->ipi_fifo_name, O_RDWR);
	
	s->ipi_fifo_path_in = "/tmp/cross_arch_ipi_fifo_out";
	//x86 out is arm in, x86 write, arm only read
    s->ipi_fifo_path_out = "/tmp/cross_arch_ipi_fifo_in";
	//x86 in is arm out, x86 read, arm only write
    mkfifo(s->ipi_fifo_path_in, 0666);
    mkfifo(s->ipi_fifo_path_out, 0666);
    s->ipi_fifo_fd_in = open(s->ipi_fifo_path_in, O_RDWR);
    s->ipi_fifo_fd_out = open(s->ipi_fifo_path_out, O_RDWR);

    //TODO: close the fd

    /*
     * Now go through and set GICR_TYPER.Last for the final
     * redistributor in each region.
     */
    cpuidx = 0;
    for (i = 0; i < s->nb_redist_regions; i++) {
        cpuidx += s->redist_region_count[i];
        s->cpu[cpuidx - 1].gicr_typer |= GICR_TYPER_LAST;
    }

    s->itslist = g_ptr_array_new();
}

static void arm_gicv3_finalize(Object *obj)
{
    GICv3State *s = ARM_GICV3_COMMON(obj);

    g_free(s->redist_region_count);
}

static void arm_gicv3_common_reset_hold(Object *obj)
{
    GICv3State *s = ARM_GICV3_COMMON(obj);
    int i;

    for (i = 0; i < s->num_cpu; i++) {
        GICv3CPUState *cs = &s->cpu[i];

        cs->level = 0;
        cs->gicr_ctlr = 0;
        if (s->lpi_enable) {
            /* Our implementation supports clearing GICR_CTLR.EnableLPIs */
            cs->gicr_ctlr |= GICR_CTLR_CES;
        }
        cs->gicr_statusr[GICV3_S] = 0;
        cs->gicr_statusr[GICV3_NS] = 0;
        cs->gicr_waker = GICR_WAKER_ProcessorSleep | GICR_WAKER_ChildrenAsleep;
        cs->gicr_propbaser = 0;
        cs->gicr_pendbaser = 0;
        cs->gicr_vpropbaser = 0;
        cs->gicr_vpendbaser = 0;
        /* If we're resetting a TZ-aware GIC as if secure firmware
         * had set it up ready to start a kernel in non-secure, we
         * need to set interrupts to group 1 so the kernel can use them.
         * Otherwise they reset to group 0 like the hardware.
         */
        if (s->irq_reset_nonsecure) {
            cs->gicr_igroupr0 = 0xffffffff;
        } else {
            cs->gicr_igroupr0 = 0;
        }

        cs->gicr_ienabler0 = 0;
        cs->gicr_ipendr0 = 0;
        cs->gicr_iactiver0 = 0;
        cs->edge_trigger = 0xffff;
        cs->gicr_igrpmodr0 = 0;
        cs->gicr_nsacr = 0;
        memset(cs->gicr_ipriorityr, 0, sizeof(cs->gicr_ipriorityr));

        cs->hppi.prio = 0xff;
        cs->hpplpi.prio = 0xff;
        cs->hppvlpi.prio = 0xff;

        /* State in the CPU interface must *not* be reset here, because it
         * is part of the CPU's reset domain, not the GIC device's.
         */
    }

    /* For our implementation affinity routing is always enabled */
    if (s->security_extn) {
        s->gicd_ctlr = GICD_CTLR_ARE_S | GICD_CTLR_ARE_NS;
    } else {
        s->gicd_ctlr = GICD_CTLR_DS | GICD_CTLR_ARE;
    }

    s->gicd_statusr[GICV3_S] = 0;
    s->gicd_statusr[GICV3_NS] = 0;

    memset(s->group, 0, sizeof(s->group));
    memset(s->grpmod, 0, sizeof(s->grpmod));
    memset(s->enabled, 0, sizeof(s->enabled));
    memset(s->pending, 0, sizeof(s->pending));
    memset(s->active, 0, sizeof(s->active));
    memset(s->level, 0, sizeof(s->level));
    memset(s->edge_trigger, 0, sizeof(s->edge_trigger));
    memset(s->gicd_ipriority, 0, sizeof(s->gicd_ipriority));
    memset(s->gicd_irouter, 0, sizeof(s->gicd_irouter));
    memset(s->gicd_nsacr, 0, sizeof(s->gicd_nsacr));
    /* GICD_IROUTER are UNKNOWN at reset so in theory the guest must
     * write these to get sane behaviour and we need not populate the
     * pointer cache here; however having the cache be different for
     * "happened to be 0 from reset" and "guest wrote 0" would be
     * too confusing.
     */
    gicv3_cache_all_target_cpustates(s);

    if (s->irq_reset_nonsecure) {
        /* If we're resetting a TZ-aware GIC as if secure firmware
         * had set it up ready to start a kernel in non-secure, we
         * need to set interrupts to group 1 so the kernel can use them.
         * Otherwise they reset to group 0 like the hardware.
         */
        for (i = GIC_INTERNAL; i < s->num_irq; i++) {
            gicv3_gicd_group_set(s, i);
        }
    }
    s->gicd_no_migration_shift_bug = true;
}

static void arm_gic_common_linux_init(ARMLinuxBootIf *obj,
                                      bool secure_boot)
{
    GICv3State *s = ARM_GICV3_COMMON(obj);

    if (s->security_extn && !secure_boot) {
        /* We're directly booting a kernel into NonSecure. If this GIC
         * implements the security extensions then we must configure it
         * to have all the interrupts be NonSecure (this is a job that
         * is done by the Secure boot firmware in real hardware, and in
         * this mode QEMU is acting as a minimalist firmware-and-bootloader
         * equivalent).
         */
        s->irq_reset_nonsecure = true;
    }
}

static Property arm_gicv3_common_properties[] = {
    DEFINE_PROP_CHR("arm-chr", GICv3State, chr),
    DEFINE_PROP_UINT32("num-cpu", GICv3State, num_cpu, 1),
    DEFINE_PROP_UINT32("num-irq", GICv3State, num_irq, 32),
    DEFINE_PROP_UINT32("revision", GICv3State, revision, 3),
    DEFINE_PROP_BOOL("has-lpi", GICv3State, lpi_enable, 0),
    DEFINE_PROP_BOOL("has-security-extensions", GICv3State, security_extn, 0),
    /*
     * Compatibility property: force 8 bits of physical priority, even
     * if the CPU being emulated should have fewer.
     */
    DEFINE_PROP_BOOL("force-8-bit-prio", GICv3State, force_8bit_prio, 0),
    DEFINE_PROP_ARRAY("redist-region-count", GICv3State, nb_redist_regions,
                      redist_region_count, qdev_prop_uint32, uint32_t),
    DEFINE_PROP_LINK("sysmem", GICv3State, dma, TYPE_MEMORY_REGION,
                     MemoryRegion *),
    DEFINE_PROP_END_OF_LIST(),
};

static void arm_gicv3_common_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    ARMLinuxBootIfClass *albifc = ARM_LINUX_BOOT_IF_CLASS(klass);
	ARMGICv3CommonClass *agcc = ARM_GICV3_COMMON_CLASS(klass);

    rc->phases.hold = arm_gicv3_common_reset_hold;
    dc->realize = arm_gicv3_common_realize;
    device_class_set_props(dc, arm_gicv3_common_properties);
    dc->vmsd = &vmstate_gicv3;
    albifc->arm_linux_init = arm_gic_common_linux_init;
//Tong patch
	agcc->process_msg_disconnect = gicv3_process_msg_disconnect;
    agcc->process_msg_connect = gicv3_process_msg_connect;
    agcc->process_msg = process_msg;
    agcc->recv_msg = gicv3_recv_msg;
    agcc->read_cb = gicv3_read;
    agcc->can_read_cb = gicv3_can_receive;
    agcc->send_interrupt = gicv3_send_interrupt;
    agcc->recv_setup = gicv3_recv_setup;

}

static const TypeInfo arm_gicv3_common_type = {
    .name = TYPE_ARM_GICV3_COMMON,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(GICv3State),
    .class_size = sizeof(ARMGICv3CommonClass),
    .class_init = arm_gicv3_common_class_init,
    .instance_finalize = arm_gicv3_finalize,
    .abstract = true,
    .interfaces = (InterfaceInfo []) {
        { TYPE_ARM_LINUX_BOOT_IF },
        { },
    },
};

static void register_types(void)
{
    type_register_static(&arm_gicv3_common_type);
}

type_init(register_types)
