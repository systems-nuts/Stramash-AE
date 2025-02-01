//
// Created by eva on 4/4/21.
//

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qemu/cutils.h"
#include "hw/qdev-properties-system.h"
#include "migration/blocker.h"
#include "migration/vmstate.h"
#include "qemu/error-report.h"
#include "qemu/event_notifier.h"
#include "qemu/module.h"
#include "qom/object_interfaces.h"
#include "chardev/char-fe.h"
#include "sysemu/hostmem.h"
#include "sysemu/qtest.h"
#include "qapi/visitor.h"
#include "hw/irq.h"
#include "hw/sysbus.h"

#include "hw/misc/vintc.h"
#include "qom/object.h"

#define VINTC_MAX_PEERS UINT16_MAX
#define VINTC_IOEVENTFD   0
#define VINTC_MSI     1

#define VINTC_IO_SIZE 0x100
/*
#define VINTC_DPRINTF(fmt, ...)                       \
    do {                                                \
        if (VINTC_DEBUG) {                            \
            printf("VINTC: " fmt, ## __VA_ARGS__);    \
        }                                               \
    } while (0)
*/

typedef struct VIntcState VIntcState;
DECLARE_INSTANCE_CHECKER(VIntcState, VINTC,
                         TYPE_VINTC)


typedef struct Peer {
  int nb_eventfds;
  EventNotifier *eventfds;
} Peer;

typedef struct DeviceVector {
  DeviceState *dev;
  int virq;
  bool unmasked;
} DeviceVector;

struct VIntcState {
  SysBusDevice parent_obj;
  CharBackend server_chr; /* without interrupts */

  // memory region for mmaped io
  MemoryRegion vintc_mmio;

  // interrupt support
  Peer *peers;
  int number_peers; // number of peers
  uint64_t msg_buf; // buffer for messages
  int msg_buffered_bytes; // number in msg_buf used

  int vm_id;

  uint32_t vectors;

  // proper irqs
  qemu_irq irq;

  DeviceVector *dev_vectors;

  // register stuff
  uint32_t intrstatus;
  uint32_t intrmask;

  uint32_t *shared_reg_ptr;
};

/* registers for the Inter-VM shared memory device */
enum vintc_registers {
  INTRMASK = 0,
  INTRSTATUS = 4,
  IVPOSITION = 8,
  DOORBELL = 12,
  SHARED_REG_ADDR = 16,
  SHARED_REG_SIZE = 20,
};

enum ivshmem_shared_registers_offset {
  SHARED_ADDR_OFFSET = 0,
  SHARED_SIZE_OFFSET = 1,
};

static Property vintc_properties[] = {
    DEFINE_PROP_CHR("chardev", VIntcState, server_chr),
    DEFINE_PROP_UINT32("vectors", VIntcState, vectors, 1),
    DEFINE_PROP_END_OF_LIST(),
};

static const VMStateDescription vintc_vmsd = {
    .name = TYPE_VINTC,
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(intrstatus, VIntcState),
        VMSTATE_END_OF_LIST(),
    }
};

static void allocate_shared_reg(const char* shared_reg_name, int shared_reg_size, void *opaque) {
    VIntcState *s = opaque;
    int shared_reg_fd = shm_open(shared_reg_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (shared_reg_fd == -1) {
        printf("allocating shared memory failed\n");
        g_assert(0);
    }
    if (ftruncate(shared_reg_fd, shared_reg_size) == -1) {
        printf("Ftruncating shared register failed\n");
        g_assert(0);
    }
    s->shared_reg_ptr = (uint32_t *)mmap(NULL, shared_reg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_reg_fd, 0);
}

static uint32_t read_shared_register(void *opaque, size_t offset) {
    VIntcState *s = opaque;
    return s->shared_reg_ptr[offset];
}

static void write_shared_register(void *opaque, uint32_t write_data, size_t offset) {
    VIntcState *s = opaque;
    s->shared_reg_ptr[offset] = write_data;
}


static void vintc_IntrMask_write(VIntcState *s, uint32_t val)
{
  //VINTC_DPRINTF("interrupt mask write(w) val = 0x%04x\n", val);
  

  s->intrmask = val;
}

static uint32_t vintc_IntrMask_read(VIntcState *s)
{

  uint32_t ret = s->intrmask;
  //VINTC_DPRINTF("interrupt status read(r) val = 0x%04x\n", ret);
  return ret;
}

static void vintc_IntrStatus_write(VIntcState *s, uint32_t val)
{
  //VINTC_DPRINTF("interrupt status write(w) val = 0x%04x\n", val);

  s->intrstatus = val;
}

static uint32_t vintc_IntrStatus_read(VIntcState *s)
{

  uint32_t ret = s->intrstatus;
  //VINTC_DPRINTF("interrupt status read(r) val = 0x%04x\n", ret);
  s->intrstatus = 0;
  return ret;
}


static uint64_t vintc_io_read(void *opaque, hwaddr addr,
                                unsigned size)
{

  VIntcState *s = opaque;
  uint32_t ret;

  switch (addr)
  {
    case INTRMASK:
      ret = vintc_IntrMask_read(s);
      break;

    case INTRSTATUS:
      ret = vintc_IntrStatus_read(s);
      break;

    case IVPOSITION:
      ret = s->vm_id;
      qemu_set_irq(s->irq, 0);
      break;

      case SHARED_REG_ADDR:
          ret = read_shared_register(s, SHARED_ADDR_OFFSET);
          break;

      case SHARED_REG_SIZE:
          ret = read_shared_register(s, SHARED_SIZE_OFFSET);
          break;

    default:
      ret = 0;
  }

  return ret;
}

static void vintc_io_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
  VIntcState *s = opaque;

  uint16_t dest = val >> 16;
  uint16_t vector = val & 0xff;



  addr &= 0xfc;

  //VINTC_DPRINTF("writing to addr " TARGET_FMT_plx "\n", addr);
  switch (addr)
  {
    case INTRMASK:
      vintc_IntrMask_write(s, val);
      break;

    case INTRSTATUS:
      vintc_IntrStatus_write(s, val);
      break;

    case DOORBELL:
        if (vector != 0) {
            //VINTC_DPRINTF("Vector is not equal to 0, aborting\n");
        }
      //VINTC_DPRINTF("writing to doorbell register\n");
      /* check that dest VM ID is reasonable */
      if (dest >= s->number_peers) {
        //VINTC_DPRINTF("Current number of peers is %d\n", s->number_peers);
        //VINTC_DPRINTF("Invalid destination VM ID (%d)\n", dest);
        break;
      }

      /* check doorbell range */
      break;

      case SHARED_REG_ADDR:
          //VINTC_DPRINTF("Writing to shared address register\n");
          write_shared_register(s, val, SHARED_ADDR_OFFSET);
          break;

      case SHARED_REG_SIZE:
          //VINTC_DPRINTF("Writing to shared size register\n");
          write_shared_register(s, val, SHARED_SIZE_OFFSET);
          break;

    default:
      //VINTC_DPRINTF("Unhandled write " TARGET_FMT_plx "\n", addr);
  }
}



static const MemoryRegionOps vintc_mmio_ops = {
    .read = vintc_io_read,
    .write = vintc_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};


static void vintc_vector_notify(void *opaque)
{
  DeviceVector *entry = opaque;
  DeviceState *dev = entry->dev;
  VIntcState *s = VINTC(dev);
  int vector = entry - s->dev_vectors;
  EventNotifier *n = &s->peers[s->vm_id].eventfds[vector];

  if (!event_notifier_test_and_clear(n)) {
    return;
  }

  //VINTC_DPRINTF("interrupt on vector %p %d\n", dev, vector);
  //VINTC_DPRINTF("interrupt o");
  //FIXME: this part differs from the original implementation, because we're raising irq directly

  qemu_set_irq(s->irq, 1);
  vintc_IntrStatus_write(s, 1);
}

static void watch_vector_notifier(VIntcState *s, EventNotifier *n,
                                  int vector)
{
  int eventfd = event_notifier_get_fd(n);

  assert(!s->dev_vectors[vector].dev);
  s->dev_vectors[vector].dev = DEVICE(s);

  qemu_set_fd_handler(eventfd, vintc_vector_notify,
                      NULL, &s->dev_vectors[vector]);
}



static void setup_interrupt(VIntcState *s, int vector, Error **errp)
{
  EventNotifier  *n = &s->peers[s->vm_id].eventfds[vector];
  //VINTC_DPRINTF("setting up interrupt for vector %d\n", vector);

  watch_vector_notifier(s, n, vector);
}

static void vintc_init(Object *obj) {
  VIntcState *s = VINTC(obj);
  SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

  //TODO: extract size into define
  memory_region_init_io(&s->vintc_mmio, OBJECT(s), &vintc_mmio_ops, s,
                        "vintc-mmio", VINTC_IO_SIZE);

  sysbus_init_mmio(sbd, &s->vintc_mmio);
  sysbus_init_irq(sbd, &s->irq);
}

static void resize_peers(VIntcState *s, int nb_peers)
{
  int old_nb_peers = s->number_peers;
  int i;

  assert(nb_peers > old_nb_peers);
  //VINTC_DPRINTF("bumping storage to %d peers\n", nb_peers);

  s->peers = g_realloc(s->peers, nb_peers * sizeof(Peer));
  s->number_peers = nb_peers;

  for (i = old_nb_peers; i < nb_peers; i++) {
    s->peers[i].eventfds = g_new0(EventNotifier, s->vectors);
    s->peers[i].nb_eventfds = 0;
  }
}

static int64_t vintc_recv_msg(VIntcState *s, int *pfd, Error **errp)
{
  int64_t msg;
  int n, ret;

  n = 0;
  do {
    ret = qemu_chr_fe_read_all(&s->server_chr, (uint8_t *)&msg + n,
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

  *pfd = qemu_chr_fe_get_msgfd(&s->server_chr);
  return le64_to_cpu(msg);
}

static void vintc_del_eventfd(VIntcState *s, int posn, int i)
{
  memory_region_del_eventfd(&s->vintc_mmio,
                            DOORBELL,
                            4,
                            true,
                            (posn << 16) | i,
                            &s->peers[posn].eventfds[i]);
}

static void close_peer_eventfds(VIntcState *s, int posn)
{
  //TODO: verify function here is correct
  int i, n;

  assert(posn >= 0 && posn < s->number_peers);
  n = s->peers[posn].nb_eventfds;

    memory_region_transaction_begin();
    for (i = 0; i < n; i++) {
      vintc_del_eventfd(s, posn, i);
    }
    memory_region_transaction_commit();

  for (i = 0; i < n; i++) {
    event_notifier_cleanup(&s->peers[posn].eventfds[i]);
  }

  g_free(s->peers[posn].eventfds);
  s->peers[posn].nb_eventfds = 0;
}
static void process_msg_disconnect(VIntcState *s, uint16_t posn,
                                   Error **errp)
{
  //VINTC_DPRINTF("posn %d has gone away\n", posn);
  if (posn >= s->number_peers || posn == s->vm_id) {
    error_setg(errp, "invalid peer %d", posn);
    return;
  }
  close_peer_eventfds(s, posn);
}

static void vintc_add_eventfd(VIntcState *s, int posn, int i)
{
  memory_region_add_eventfd(&s->vintc_mmio,
                            DOORBELL,
                            4,
                            true,
                            (posn << 16) | i,
                            &s->peers[posn].eventfds[i]);
}

static void process_msg_connect(VIntcState *s, uint16_t posn, int fd,
                                Error **errp)
{
  //VINTC_DPRINTF("Processing incoming CONNECT message");
  Peer *peer = &s->peers[posn];
  int vector;

  /*
   * The N-th connect message for this peer comes with the file
   * descriptor for vector N-1.  Count messages to find the vector.
   */
  if (peer->nb_eventfds >= s->vectors) {
    error_setg(errp, "Too many eventfd received, device has %d vectors",
               s->vectors);
    close(fd);
    return;
  }
  vector = peer->nb_eventfds++;
  //VINTC_DPRINTF("vector size is %d\n", vector);

  //VINTC_DPRINTF("eventfds[%d][%d] = %d\n", posn, vector, fd);
  event_notifier_init_fd(&peer->eventfds[vector], fd);
  fcntl_setfl(fd, O_NONBLOCK); /* msix/irqfd poll non block */

  if (posn == s->vm_id) {
    //VINTC_DPRINTF("setting up interrupt\n");
    setup_interrupt(s, vector, errp);
    /* TODO do we need to handle the error? */
  }

    vintc_add_eventfd(s, posn, vector);
}

static void process_msg(VIntcState *s, int64_t msg, int fd, Error **errp)
{
  //VINTC_DPRINTF("posn is %" PRId64 ", fd is %d\n", msg, fd);

  if (msg < -1 || msg > VINTC_MAX_PEERS) {
    error_setg(errp, "server sent invalid message %" PRId64, msg);
    close(fd);
    return;
  }

  if (msg == -1) {
    //TODO: disable shm in server in the future
    //VINTC_DPRINTF("Processing shared memory, ignore");

    return;
  }

  if (msg >= s->number_peers) {
    resize_peers(s, msg + 1);
  }

  if (fd >= 0) {
    process_msg_connect(s, msg, fd, errp);
  } else {
    process_msg_disconnect(s, msg, errp);
  }
}

static void vintc_recv_setup(VIntcState *s, Error **errp)
{
  Error *err = NULL;
  int64_t msg;
  int fd;

  msg = vintc_recv_msg(s, &fd, &err);
  if (err) {
    error_propagate(errp, err);
    return;
  }
  if (msg != IVSHMEM_PROTOCOL_VERSION) {
    error_setg(errp, "server sent version %" PRId64 ", expecting %d",
               msg, IVSHMEM_PROTOCOL_VERSION);
    return;
  }
  if (fd != -1) {
    error_setg(errp, "server sent invalid version message");
    return;
  }

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
  msg = vintc_recv_msg(s, &fd, &err);
  if (err) {
    error_propagate(errp, err);
    return;
  }
  if (fd != -1 || msg < 0 || msg > VINTC_MAX_PEERS) {
    error_setg(errp, "server sent invalid ID message");
    return;
  }
  s->vm_id = msg;

  /*
   * Receive more messages until we got shared memory.
   */
  do {
    msg = vintc_recv_msg(s, &fd, &err);
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

  /*
   * This function must either map the shared memory or fail.  The
   * loop above ensures that: it terminates normally only after it
   * successfully processed the server's shared memory message.
   * Assert that actually mapped the shared memory:
   */
  //TODO: I've disabled correctly mapping the shared memory as above
  // please update in the future
//  g_assert(0);
}

static int vintc_can_receive(void *opaque)
{
  VIntcState *s = opaque;

  assert(s->msg_buffered_bytes < sizeof(s->msg_buf));
  return sizeof(s->msg_buf) - s->msg_buffered_bytes;
}

static void vintc_read(void *opaque, const uint8_t *buf, int size)
{
  VIntcState *s = opaque;
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

  fd = qemu_chr_fe_get_msgfd(&s->server_chr);

  process_msg(s, msg, fd, &err);
  if (err) {
    error_report_err(err);
  }
}

static int vintc_setup_interrupts(VIntcState *s, Error **errp)
{
  //FIXME: this differs a lot from the original approach
  /* allocate QEMU callback data for receiving interrupts */
  s->dev_vectors = g_malloc0(s->vectors * sizeof(DeviceState));


  return 0;
}

static void vintc_realize(DeviceState *dev, Error **errp) {
  VIntcState *s = VINTC(dev);
  Error *err = NULL;

  if (!qemu_chr_fe_backend_connected(&s->server_chr)) {
    error_setg(errp, "You must specify a 'chardev'");
    return;
  }



  Chardev *chr = qemu_chr_fe_get_driver(&s->server_chr);
  assert(chr);

  //VINTC_DPRINTF("using shared memory server (socket = %s)\n",
                  //chr->filename);

  // receive messages from server
  /* we allocate enough space for 16 peers and grow as needed */
  resize_peers(s, 16);

  /*
   * Receive setup messages from server synchronously.
   * Older versions did it asynchronously, but that creates a
   * number of entertaining race conditions.
   */
  vintc_recv_setup(s, &err);
  if (err) {
    error_propagate(errp, err);
    return;
  }

  qemu_chr_fe_set_handlers(&s->server_chr,vintc_can_receive,
                           vintc_read, NULL, NULL, s, NULL, true);

  if (vintc_setup_interrupts(s, errp) < 0) {
    error_prepend(errp, "Failed to initialize interrupts: ");
    return;
  }

    allocate_shared_reg("/qemu_shared_register_ipi", 32, s);
}

static void vintc_class_init(ObjectClass *klass, void *data) {
  DeviceClass *dc = DEVICE_CLASS(klass);
  dc->vmsd = &vintc_vmsd;
  //FIXME
  dc->realize = vintc_realize;
  device_class_set_props(dc, vintc_properties);
}

static const TypeInfo vintc_info = {
    .name = TYPE_VINTC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(VIntcState),
    .class_init = vintc_class_init,
    .instance_init = vintc_init,
};

static void vintc_register_types(void) {
  type_register_static(&vintc_info);
}

type_init(vintc_register_types);
