/*
 * QEMU Host Memory Backend
 *
 * Copyright (C) 2013-2014 Red Hat Inc
 *
 * Authors:
 *   Igor Mammedov <imammedo@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "sysemu/hostmem.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qom/object_interfaces.h"
#include "stramash_config.h"

static void
ram_backend_memory_alloc(HostMemoryBackend *backend, Error **errp)
{
    //uint32_t ram_flags;
    char *name;
	printf("Tong: Initializing memory backend this way with %s\n", __func__ );

    if (!backend->size) {
        error_setg(errp, "can't create backend with size 0");
        return;
    }

    name = host_memory_backend_get_name(backend);
    //ram_flags = backend->share ? RAM_SHARED : 0;
    //ram_flags |= backend->reserve ? 0 : RAM_NORESERVE;
	//Tong: apply patch +-
    //memory_region_init_ram_flags_nomigrate(&backend->mr, OBJECT(backend), name,
    //                                     backend->size, ram_flags, errp);

    char resultStr[40];
    char strStramashid[40];
    sprintf(strStramashid, "%d", Stramashid);
    strcpy(resultStr, "/dev/shm/ddr-share");
    strcat(resultStr, strStramashid);

    memory_region_init_ram_from_file(&backend->mr, OBJECT(backend), name, 
								backend->size, 0, RAM_SHARED, resultStr, false ,errp);
	//Tong: patch done
    g_free(name);
}

static void
ram_backend_class_init(ObjectClass *oc, void *data)
{
    HostMemoryBackendClass *bc = MEMORY_BACKEND_CLASS(oc);

    bc->alloc = ram_backend_memory_alloc;
}

static const TypeInfo ram_backend_info = {
    .name = TYPE_MEMORY_BACKEND_RAM,
    .parent = TYPE_MEMORY_BACKEND,
    .class_init = ram_backend_class_init,
};

static void register_types(void)
{
    type_register_static(&ram_backend_info);
}

type_init(register_types);
