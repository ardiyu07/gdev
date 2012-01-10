/*
 * Copyright 2011 Shinpei Kato
 *
 * University of California, Santa Cruz
 * Systems Research Lab.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "gdev_api.h"
#include "gdev_device.h"
#include "gdev_list.h"
#include "gdev_sched.h"
#include "gdev_time.h"

/* open a new Gdev object associated with the specified device. */
struct gdev_device *gdev_dev_open(int minor)
{
	return gdev_raw_dev_open(minor);
}

/* close the specified Gdev object. */
void gdev_dev_close(struct gdev_device *gdev)
{
	gdev_raw_dev_close(gdev);
}

/* add a new VAS object into the device VAS list. */
static void __gdev_vas_list_add(struct gdev_vas *vas)
{
	struct gdev_device *gdev = vas->gdev;
	unsigned long flags;
	
	gdev_lock_save(&gdev->vas_lock, &flags);
	gdev_list_add(&vas->list_entry, &gdev->vas_list);
	gdev_unlock_restore(&gdev->vas_lock, &flags);
}

/* delete the VAS object from the device VAS list. */
static void __gdev_vas_list_del(struct gdev_vas *vas)
{
	struct gdev_device *gdev = vas->gdev;
	unsigned long flags;
	
	gdev_lock_save(&gdev->vas_lock, &flags);
	gdev_list_del(&vas->list_entry);
	gdev_unlock_restore(&gdev->vas_lock, &flags);
}

/* allocate a new virual address space (VAS) object. */
struct gdev_vas *gdev_vas_new(struct gdev_device *gdev, uint64_t size, void *handle)
{
	struct gdev_vas *vas;

	if (!(vas = gdev_raw_vas_new(gdev, size))) {
		return NULL;
	}

	vas->handle = handle;
	vas->gdev = gdev;
	vas->prio = GDEV_PRIO_DEFAULT;
	gdev_list_init(&vas->list_entry, (void *) vas); /* entry to VAS list. */
	gdev_list_init(&vas->mem_list, NULL); /* device memory list. */
	gdev_list_init(&vas->dma_mem_list, NULL); /* host dma memory list. */
	gdev_lock_init(&vas->lock);

	__gdev_vas_list_add(vas);

	return vas;
}

/* free the specified virtual address space object. */
void gdev_vas_free(struct gdev_vas *vas)
{
	__gdev_vas_list_del(vas);
	gdev_raw_vas_free(vas);
}

/* create a new GPU context object. */
struct gdev_ctx *gdev_ctx_new(struct gdev_device *gdev, struct gdev_vas *vas)
{
	struct gdev_ctx *ctx;
	struct gdev_compute *compute = gdev->compute;

	if (!(ctx = gdev_raw_ctx_new(gdev, vas))) {
		return NULL;
	}

	ctx->vas = vas;

	/* initialize the channel. */
	compute->init(ctx);

	return ctx;
}

/* destroy the specified GPU context object. */
void gdev_ctx_free(struct gdev_ctx *ctx)
{
	gdev_raw_ctx_free(ctx);
}

/* get context ID. */
int gdev_ctx_get_cid(struct gdev_ctx *ctx)
{
	return ctx->cid;
}

/* lock the device globally. */
void gdev_global_lock(struct gdev_device *gdev)
{
	struct gdev_device *phys = gdev->parent;
	
	if (phys) {
		int physid = phys->id;
		int i, j = 0;
		for (i = 0; i < physid; i++)
			j += VCOUNT_LIST[i];
		for (i = j; i < j + VCOUNT_LIST[physid]; i++) {
			gdev_mutex_lock(&gdev_vds[i].shm_mutex);
		}
	}
	else {
		gdev_mutex_lock(&gdev->shm_mutex);
	}
}

/* unlock the device globally. */
void gdev_global_unlock(struct gdev_device *gdev)
{
	struct gdev_device *phys = gdev->parent;

	if (phys) {
		int physid = phys->id;
		int i, j = 0;
		for (i = 0; i < physid; i++)
			j += VCOUNT_LIST[i];
		for (i = j + VCOUNT_LIST[physid] - 1; i >= j ; i--) {
			gdev_mutex_unlock(&gdev_vds[i].shm_mutex);
		}
	}
	else {
		gdev_mutex_unlock(&gdev->shm_mutex);
	}
}

