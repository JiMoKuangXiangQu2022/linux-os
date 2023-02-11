/*
 * Cedarx framework ION memory allocator.
 *
 * Copyright (c) 2020-2023 Leng Xujun <lengxujun2007@126.com>.
 *
 * This file is part of Cedarx.
 *
 * Cedarx is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

typedef unsigned int u32;

#include <linux/ion.h>
#include <linux/ion_sunxi.h>

#include <log.h>

#include "ion_alloc.h"


#define ION_DEVICE "/dev/ion"
#define ION_ALN_DEFAULT 8


struct ion_buffer_data {
	long virt, phys;
	size_t size;
	int fd;
	struct ion_handle_data handle_data;
	struct ion_buffer_data *next;
};

static int ion_fd = -1;
static pthread_mutex_t ion_bfl_lock; /* ion lock for buffers' list */
static struct ion_buffer_data *ion_bfl_head = NULL;


static struct ion_buffer_data *buffer_data_find(int type, long addr);
static void buffer_data_insert(struct ion_buffer_data *buffer_data);
static int buffer_data_remove(long virt, struct ion_buffer_data **buffer_data);

static int buffer_data_phys(struct ion_buffer_data *buffer_data);


int ion_alloc_open(void)
{
	if (ion_fd >= 0)
		return 0;

	ion_fd = open(ION_DEVICE, O_RDONLY/* | O_DSYNC*/);
	if (ion_fd < 0) {
		loge("open device %s failed, error: ", strerror(errno));
		return -1;
	}

	pthread_mutex_init(&ion_bfl_lock, NULL);

	return 0;
}

int ion_alloc_close(void)
{
	if (ion_fd >= 0)
		close(ion_fd);
	
	pthread_mutex_destroy(&ion_bfl_lock);

	return 0;
}

long ion_alloc_alloc(int size)
{
	struct ion_allocation_data alloc_data;
	struct ion_handle_data handle_data;
	struct ion_fd_data fd_data;
	struct ion_buffer_data *buffer_data;
	void *virt = NULL;

	/* 
	 * Allocate only from CARVEOUT type heaps: 
	 * We need to make sure that physical contious.
	 */
	alloc_data.len = size;
	alloc_data.align = ION_ALN_DEFAULT;
	alloc_data.heap_id_mask = ION_HEAP_CARVEOUT_MASK;
	alloc_data.flags = 0; /* no cache */
	if (ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data) < 0) {
		loge("ION_IOC_ALLOC failed with error: %s", strerror(errno));
		goto ion_error_exit;
	}
	
	handle_data.handle = alloc_data.handle;

	fd_data.handle = alloc_data.handle;
	if (ioctl(ion_fd, ION_IOC_MAP, &fd_data) < 0) {
		loge("ION_IOC_MAP failed with error: %s", strerror(errno));
		goto ion_free;
	}

	virt = mmap(0, alloc_data.len, 
				PROT_READ | PROT_WRITE, MAP_SHARED, fd_data.fd, 0);
	if (virt == MAP_FAILED) {
		loge("%s: failed to map the allocated memory: %s", 
				__func__, strerror(errno));
		goto ion_close;
	}

	buffer_data = malloc(sizeof(*buffer_data));
	if (NULL == buffer_data) {
		loge("%s: allocate ion buffer data failed", __func__);
		goto ion_unmap;
	}
	
	buffer_data->handle_data = handle_data;
	buffer_data->virt = (long)virt;
	buffer_data->size = alloc_data.len;
	buffer_data_phys(buffer_data);
	buffer_data->fd = fd_data.fd;
	
	buffer_data_insert(buffer_data);
	
#if 0
	memset(virt, 0, alloc_data.len); /* zero buffer */
	/* clean cache after memset */
    clean_buffer(virt, data.size, data.offset, fd_data.fd,
                 CACHE_CLEAN_AND_INVALIDATE);
#endif

	logd("ion allocated buffer: @%p(virt=0x%08lx phys=0x%08lx size=%d fd=%d)",
         buffer_data, buffer_data->virt, buffer_data->phys, alloc_data.len, fd_data.fd);
	
	return (long)virt;

ion_unmap:
	munmap((void *)virt, alloc_data.len);
ion_close:
	close(fd_data.fd);
ion_free:
	ioctl(ion_fd, ION_IOC_FREE, &handle_data);
ion_error_exit:	
	return (long)NULL;
}

int ion_alloc_free(void *virt)
{
	struct ion_buffer_data *buffer_data;

	if (buffer_data_remove((long)virt, &buffer_data) < 0 || !buffer_data)
		return -1;
	
	if (munmap((void *)buffer_data->virt, buffer_data->size) < 0) {
		loge("%s: failed to unmap memory at 0x%08lx : %s", 
			 __func__, buffer_data->virt, strerror(errno));
		return -1;
	}

	if (ioctl(ion_fd, ION_IOC_FREE, &buffer_data->handle_data) < 0) {
		loge("ION_IOC_FREE failed with error: %s", strerror(errno));
		return -1;
	}

	logd("ion free buffer: @%p(virt=0x%08lx phys=0x%08lx size=%d, fd=%d)", 
		 buffer_data, buffer_data->virt, buffer_data->phys, buffer_data->size, buffer_data->fd);

	free(buffer_data);
	
	return 0;
}

long ion_alloc_vir2phy(void *virt)
{
	struct ion_buffer_data *p;

	if ((p = buffer_data_find(0, (long)virt)) == NULL)
		loge("%s: unknown virtual address %p", __func__, virt);
	else
		logd("virt2phys: %p -> 0x%08lx", 
			 virt, p->phys + ((long)virt - p->virt));
	return (p ? p->phys + ((long)virt - p->virt) : -1);
}

long ion_alloc_phy2vir(void *phys)
{
	struct ion_buffer_data *p;
	
	if ((p = buffer_data_find(1, (long)phys)) == NULL)
		loge("%s: unknown physical address %p", __func__, phys);
	else
		logd("phys2virt: %p -> 0x%08lx", 
			 phys, p->virt + ((long)phys - p->phys));
	return (p ? p->virt + ((long)phys - p->phys) : -1);
}

void ion_flush_cache(void *virt, int size)
{
	sunxi_cache_range cache_range;
	struct ion_custom_data data;

	cache_range.start = (long)virt;
	cache_range.end = cache_range.start + size;

	data.cmd = ION_IOC_SUNXI_FLUSH_RANGE;
	data.arg = (unsigned long)&cache_range;
	if (ioctl(ion_fd, ION_IOC_CUSTOM, &data) < 0) {
		loge("%s: ION_IOC_SUNXI_FLUSH_RANGE failed with error - %s",
             __func__, strerror(errno));
	}
}

#if 0
static int clean_buffer(void *virt, size_t size, int offset, int fd, int op)
{
	struct ion_flush_data flush_data;
    struct ion_fd_data fd_data;
    struct ion_handle_data handle_data;
    struct ion_handle *handle;
    int err = 0;
	
    fd_data.fd = fd;
    if (ioctl(ion_fd, ION_IOC_IMPORT, &fd_data) < 0) {
        loge("%s: ION_IOC_IMPORT failed with error - %s",
             __func__, strerror(errno));
        return -1;
    }

    handle_data.handle = fd_data.handle;
	
    flush_data.handle  = fd_data.handle;
    flush_data.virt   = virt;
    flush_data.offset  = offset;
    flush_data.length  = size;

    struct ion_custom_data d;
    switch (op) {
    case CACHE_CLEAN:
        d.cmd = ION_IOC_CLEAN_CACHES;
        break;
    case CACHE_INVALIDATE:
		d.cmd = ION_IOC_INV_CACHES;
        break;
    case CACHE_CLEAN_AND_INVALIDATE:
    default:
        d.cmd = ION_IOC_CLEAN_INV_CACHES;
    }
	
    d.arg = (unsigned long int)&flush_data;
	
    if (ioctl(ion_fd, ION_IOC_CUSTOM, &d) < 0) {
        loge("%s: ION_IOC_CLEAN_INV_CACHES failed with error - %s",
             __func__, strerror(errno));
        ioctl(ion_fd, ION_IOC_FREE, &handle_data);
        return -1;
    }
    ioctl(ion_fd, ION_IOC_FREE, &handle_data);
	
    return 0;
}
#endif

static struct ion_buffer_data *buffer_data_find(int type, long addr)
{
	struct ion_buffer_data *curr, *prev;

	if (!ion_bfl_head || (type < 0 || type > 1))
		return NULL;

	pthread_mutex_lock(&ion_bfl_lock);
	for (prev = curr = ion_bfl_head; 
		 curr != NULL; prev = curr, curr = curr->next) {
		if (type == 0 && 
			curr->virt <= addr && addr < curr->virt + curr->size)
			break;
		else if (type == 1 && 
				 curr->phys <= addr && addr < curr->phys + curr->size)
			break;
	}
	pthread_mutex_unlock(&ion_bfl_lock);

	return curr;
}

static void buffer_data_insert(struct ion_buffer_data *buffer_data)
{
	pthread_mutex_lock(&ion_bfl_lock);
	buffer_data->next = ion_bfl_head;
	ion_bfl_head = buffer_data;
	pthread_mutex_unlock(&ion_bfl_lock);
}

static int buffer_data_remove(long virt, struct ion_buffer_data **buffer_data)
{
	struct ion_buffer_data *curr, *prev;
	
	if (!ion_bfl_head)
		return -1;

	pthread_mutex_lock(&ion_bfl_lock);
	for (prev = curr = ion_bfl_head; 
		 curr != NULL; prev = curr, curr = curr->next) {
		if (curr->virt == virt) {
			if (curr == ion_bfl_head)
				ion_bfl_head = curr->next;
			else
				prev->next = curr->next;
			break;
		}
	}
	pthread_mutex_unlock(&ion_bfl_lock);

	if (buffer_data)
		*buffer_data = curr;

	return 0;
}


static int buffer_data_phys(struct ion_buffer_data *buffer_data)
{
	sunxi_phys_data phys_data;
	struct ion_custom_data data;

	if (NULL == buffer_data)
		return -1;

	buffer_data->phys = -1;

	phys_data.handle = buffer_data->handle_data.handle;
	data.cmd = ION_IOC_SUNXI_PHYS_ADDR;
	data.arg = (unsigned long)&phys_data;
	if (ioctl(ion_fd, ION_IOC_CUSTOM, &data) < 0) {
		loge("%s: ION_IOC_SUNXI_PHYS_ADDR failed with error: %s",
             __func__, strerror(errno));
		return -1;
	}

	buffer_data->phys = phys_data.phys_addr;
	
	return 0;
}


