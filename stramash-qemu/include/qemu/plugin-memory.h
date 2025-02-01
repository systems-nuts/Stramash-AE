/*
 * Plugin Memory API
 *
 * Copyright (c) 2019 Linaro Ltd
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLUGIN_MEMORY_H
#define PLUGIN_MEMORY_H

#include "exec/cpu-defs.h"
#include "exec/hwaddr.h"

struct qemu_plugin_hwaddr {
    bool is_io;
    bool is_store;
    union {
        struct {
            MemoryRegionSection *section;
            hwaddr    offset;
        } io;
        struct {
            void *hostaddr;
        } ram;
    } v;
};

struct qemu_plugin_page_counter {
    uint64_t tlb_miss_counter;
    uint64_t page_fault_counter;
};

/**
 * tlb_plugin_lookup: query last TLB lookup
 * @cpu: cpu environment
 *
 * This function can be used directly after a memory operation to
 * query information about the access. It is used by the plugin
 * infrastructure to expose more information about the address.
 *
 * It would only fail if not called from an instrumented memory access
 * which would be an abuse of the API.
 */
bool tlb_plugin_lookup(CPUState *cpu, target_ulong addr, int mmu_idx,
                       bool is_store, struct qemu_plugin_hwaddr *data);
void tlb_plugin_read_counters(struct qemu_plugin_page_counter *ctr);

#endif /* PLUGIN_MEMORY_H */
