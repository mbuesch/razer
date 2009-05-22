/*
 *   Razer device access library
 *
 *   Copyright (C) 2007-2008 Michael Buesch <mb@bu3sch.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include "librazer.h"
#include "razer_private.h"

#include "hw_deathadder.h"
#include "hw_krait.h"
#include "hw_lachesis.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <usb.h>


enum razer_devtype {
	RAZER_DEVTYPE_MOUSE,
};

/** struct razer_mouse_base_ops - Basic device-init operations
 *
 * @type: The type ID.
 *
 * @gen_idstr: Generate an ID string that uniquely identifies the
 *	       device in the machine.
 *
 * @init: Init the private data structures.
 *
 * @release: Release the private data structures.
 *
 * @assign_usb_device: (re)assign a USB device to a mouse.
 */
struct razer_mouse_base_ops {
	enum razer_mouse_type type;
	void (*gen_idstr)(struct usb_device *udev, char *buf);
	int (*init)(struct razer_mouse *m, struct usb_device *udev);
	void (*release)(struct razer_mouse *m);
	void (*assign_usb_device)(struct razer_mouse *m, struct usb_device *udev);
};

struct razer_usb_device {
	uint16_t vendor;	/* Vendor ID */
	uint16_t product;	/* Product ID */
	enum razer_devtype type;
	union {
		const struct razer_mouse_base_ops *mouse_ops;
	} u;
};

static const struct razer_mouse_base_ops razer_deathadder_base_ops = {
	.type			= RAZER_MOUSETYPE_DEATHADDER,
	.gen_idstr		= razer_deathadder_gen_idstr,
	.init			= razer_deathadder_init_struct,
	.release		= razer_deathadder_release,
	.assign_usb_device	= razer_deathadder_assign_usb_device,
};

static const struct razer_mouse_base_ops razer_krait_base_ops = {
	.type			= RAZER_MOUSETYPE_KRAIT,
	.gen_idstr		= razer_krait_gen_idstr,
	.init			= razer_krait_init_struct,
	.release		= razer_krait_release,
	.assign_usb_device	= razer_krait_assign_usb_device,
};

static const struct razer_mouse_base_ops razer_lachesis_base_ops = {
	.type			= RAZER_MOUSETYPE_LACHESIS,
	.gen_idstr		= razer_lachesis_gen_idstr,
	.init			= razer_lachesis_init_struct,
	.release		= razer_lachesis_release,
	.assign_usb_device	= razer_lachesis_assign_usb_device,
};


#define USBVENDOR_ANY	0xFFFF
#define USBPRODUCT_ANY	0xFFFF

#define USB_MOUSE(_vendor, _product, _mouse_ops)	\
	{ .vendor = _vendor, .product = _product,	\
	  .type = RAZER_DEVTYPE_MOUSE,			\
	  .u = { .mouse_ops = _mouse_ops, }, }

/* Table of supported USB devices. */
static const struct razer_usb_device razer_usbdev_table[] = {
	USB_MOUSE(0x1532, 0x0007, &razer_deathadder_base_ops),
	USB_MOUSE(0x1532, 0x0003, &razer_krait_base_ops),
	USB_MOUSE(0x1532, 0x000C, &razer_lachesis_base_ops),
	{ 0, }, /* List end */
};
#undef USB_MOUSE



static bool librazer_initialized;
static struct razer_mouse *mice_list = NULL;


static int match_usbdev(const struct usb_device_descriptor *desc,
			const struct razer_usb_device *id)
{
	if ((desc->idVendor != id->vendor) &&
	    (id->vendor != USBVENDOR_ANY))
		return 0;
	if ((desc->idProduct != id->product) &&
	    (id->product != USBPRODUCT_ANY))
		return 0;
	return 1;
}

static const struct razer_usb_device * usbdev_lookup(const struct usb_device_descriptor *desc)
{
	const struct razer_usb_device *id = &(razer_usbdev_table[0]);

	while (id->vendor || id->product) {
		if (match_usbdev(desc, id))
			return id;
		id++;
	}
	return NULL;
}

static void mouse_list_add(struct razer_mouse **base, struct razer_mouse *new_entry)
{
	struct razer_mouse *i;

	new_entry->next = NULL;
	if (!(*base)) {
		*base = new_entry;
		return;
	}
	for (i = *base; i->next; i = i->next)
		;
	i->next = new_entry;
}

static void mouse_list_del(struct razer_mouse **base, struct razer_mouse *del_entry)
{
	struct razer_mouse *i;

	if (del_entry == *base) {
		*base = (*base)->next;
		return;
	}
	for (i = *base; i && (i->next != del_entry); i = i->next)
		;
	if (i)
		i->next = del_entry->next;
}

struct razer_mouse * razer_mouse_list_find(struct razer_mouse *base, const char *idstr)
{
	struct razer_mouse *m;

	razer_for_each_mouse(m, base) {
		if (strncmp(m->idstr, idstr, RAZER_IDSTR_MAX_SIZE) == 0)
			return m;
	}

	return NULL;
}

static struct razer_mouse * mouse_new(const struct razer_usb_device *id,
				      struct usb_device *udev)
{
	struct razer_mouse *m;

	m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));
	m->flags |= RAZER_MOUSEFLG_NEW;
	m->base_ops = id->u.mouse_ops;
	m->base_ops->init(m, udev);

	dprintf("Allocated and initialized new mouse (type=%d)\n",
		m->base_ops->type);

	return m;
}

static void razer_free_mouse(struct razer_mouse *m)
{
	dprintf("Freeing mouse (type=%d)\n",
		m->base_ops->type);

	m->base_ops->release(m);
	memset(m, 0, sizeof(*m));
	free(m);
}

static void razer_free_mice(struct razer_mouse *mouse_list)
{
	struct razer_mouse *mouse, *next;

	for (mouse = mouse_list; mouse; ) {
		next = mouse->next;
		razer_free_mouse(mouse);
		mouse = next;
	}
}

struct razer_mouse * razer_rescan_mice(void)
{
	struct usb_bus *bus, *buslist;
	struct usb_device *dev;
	const struct usb_device_descriptor *desc;
	const struct razer_usb_device *id;
	char idstr[RAZER_IDSTR_MAX_SIZE + 1] = { 0, };
	struct razer_mouse *mouse, *new_list = NULL;

	usb_find_busses();
	usb_find_devices();

	buslist = usb_get_busses();
	for_each_usbbus(bus, buslist) {
		for_each_usbdev(dev, bus->devices) {
			desc = &dev->descriptor;
			id = usbdev_lookup(desc);
			if (!id)
				continue;
			if (id->type != RAZER_DEVTYPE_MOUSE)
				continue;
			id->u.mouse_ops->gen_idstr(dev, idstr);
			mouse = razer_mouse_list_find(mice_list, idstr);
			if (mouse) {
				/* We already have this mouse. Delete it from the global
				 * mice list. It will be added back later. */
				mouse_list_del(&mice_list, mouse);
				mouse->base_ops->assign_usb_device(mouse, dev);
			} else {
				/* We don't have this mouse, yet. Create a new one. */
				mouse = mouse_new(id, dev);
				if (!mouse)
					continue;
			}
			mouse_list_add(&new_list, mouse);
		}
	}
	/* Kill the remaining entries in the old list.
	 * They are not connected to the machine anymore. */
	razer_free_mice(mice_list);
	/* And finally set the pointer to the new list. */
	mice_list = new_list;

	return mice_list;
}

void razer_free_freq_list(enum razer_mouse_freq *freq_list, int count)
{
	if (freq_list)
		free(freq_list);
}

void razer_free_resolution_list(enum razer_mouse_res *res_list, int count)
{
	if (res_list)
		free(res_list);
}

void razer_free_leds(struct razer_led *led_list)
{
	struct razer_led *led, *next;

	for (led = led_list; led; ) {
		next = led->next;
		free(led);
		led = next;
	}
}

int razer_init(void)
{
	if (librazer_initialized)
		return 0;
	usb_init();
	librazer_initialized = 1;

	return 0;
}

void razer_exit(void)
{
	if (!librazer_initialized)
		return;
	razer_free_mice(mice_list);
	librazer_initialized = 0;
}

static int reconnect_kdrv_hack(usb_dev_handle *_h, int interf)
{
#define IOCTL_USB_IOCTL		_IOWR('U', 18, struct usb_ioctl)
#define IOCTL_USB_CONNECT	_IO('U', 23)

	struct fake_usb_dev_handle {
		int fd;
		/* ... there's more */
	} *h = (struct fake_usb_dev_handle *)_h;
	struct usb_ioctl {
		int ifno;
		int ioctl_code;
		void *data;
	} cmd;
	int err;

	cmd.ifno = interf;
	cmd.ioctl_code = IOCTL_USB_CONNECT;
	cmd.data = NULL;

	err = ioctl(h->fd, IOCTL_USB_IOCTL, &cmd);
	if (err > 0)
		err = 0;

	return err;
}

static void razer_reattach_usb_kdrv(struct razer_usb_context *ctx)
{
	int err;

	/* Reattach the kernel driver, if needed. */
	if (!ctx->kdrv_detached)
		return;
#ifdef LIBUSB_HAS_ATTACH_KERNEL_DRIVER_NP
	err = usb_attach_kernel_driver_np(ctx->h, ctx->interf);
	if (!err) {
		ctx->kdrv_detached = 0;
		return;
	}
#endif
	/* libUSB version is too old and doesn't have the attach function.
	 * Try to hack around it by directly calling the kernel IOCTL. */
	err = reconnect_kdrv_hack(ctx->h, ctx->interf);
	if (!err) {
		ctx->kdrv_detached = 0;
		return;
	}

	fprintf(stderr, "librazer: Failed to reconnect the kernel driver.\n"
		"The device most likely won't work now. Try to replug it.\n");
}

static int razer_usb_claim(struct razer_usb_context *ctx)
{
	int err;

	ctx->kdrv_detached = 0;
	err = usb_claim_interface(ctx->h, ctx->interf);
	if (err && err != -EBUSY)
		fprintf(stderr, "razer_usb_claim: first claim failed %d\n", err);
	if (err == -EBUSY) {
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
		err = usb_detach_kernel_driver_np(ctx->h, ctx->interf);
		if (err) {
			fprintf(stderr, "razer_usb_claim: detach failed %d\n", err);
			return err;
		}
		ctx->kdrv_detached = 1;
		err = usb_claim_interface(ctx->h, ctx->interf);
		if (err) {
			fprintf(stderr, "razer_usb_claim: claim failed %d\n", err);
			razer_reattach_usb_kdrv(ctx);
			return err;
		}
#endif
	}

	return err;
}

static void razer_usb_release(struct razer_usb_context *ctx)
{
	usb_release_interface(ctx->h, ctx->interf);
	razer_reattach_usb_kdrv(ctx);
}

int razer_generic_usb_claim(struct razer_usb_context *ctx)
{
	int err;

	ctx->interf = ctx->dev->config->interface->altsetting[0].bInterfaceNumber;
	ctx->h = usb_open(ctx->dev);
	if (!ctx->h) {
		fprintf(stderr, "razer_generic_usb_claim: usb_open failed\n");
		return -ENODEV;
	}
	err = razer_usb_claim(ctx);
	if (err)
		return err;

	return 0;
}

void razer_generic_usb_release(struct razer_usb_context *ctx)
{
	razer_usb_release(ctx);
	usb_close(ctx->h);
}

/** razer_usb_force_reinit - Force the USB-level reinitialization of a device,
  * so it enters a known state.
  */
int razer_usb_force_reinit(struct razer_usb_context *ctx)
{
printf("FORCE REINIT\n");
	//TODO

	return 0;
}

static void timeval_add_msec(struct timeval *tv, unsigned int msec)
{
	unsigned int seconds, usec;

	seconds = msec / 1000;
	msec = msec % 1000;
	usec = msec * 1000;

	tv->tv_usec += usec;
	while (tv->tv_usec >= 1000000) {
		tv->tv_sec++;
		tv->tv_usec -= 1000000;
	}
	tv->tv_sec += seconds;
}

/* Returns true, if a is after b. */
static bool timeval_after(const struct timeval *a, const struct timeval *b)
{
	if (a->tv_sec > b->tv_sec)
		return 1;
	if ((a->tv_sec == b->tv_sec) && (a->tv_usec > b->tv_usec))
		return 1;
	return 0;
}

void razer_msleep(unsigned int msecs)
{
	int err;
	struct timespec time;

	time.tv_sec = 0;
	while (msecs >= 1000) {
		time.tv_sec++;
		msecs -= 1000;
	}
	time.tv_nsec = msecs;
	time.tv_nsec *= 1000000;
	do {
		err = nanosleep(&time, &time);
	} while (err && errno == EINTR);
	if (err) {
		fprintf(stderr, "nanosleep() failed with: %s\n",
			strerror(errno));
	}
}

/** razer_usb_reconnect_guard_init - Init the reconnect-guard context
 *
 * Call this _before_ triggering any device operations that might
 * reset the device.
 */
int razer_usb_reconnect_guard_init(struct razer_usb_reconnect_guard *guard,
				   struct razer_usb_context *ctx)
{
	guard->ctx = ctx;
	memcpy(&guard->old_desc, &ctx->dev->descriptor, sizeof(guard->old_desc));
	memcpy(guard->old_dirname, ctx->dev->bus->dirname, sizeof(guard->old_dirname));
	memcpy(guard->old_filename, ctx->dev->filename, sizeof(guard->old_filename));

	return 0;
}

static struct usb_device * guard_find_usb_dev(const struct usb_device_descriptor *desc,
					      const char *dirname,
					      const char *filename)
{
	struct usb_bus *bus, *buslist;
	struct usb_device *dev;

	usb_find_busses();
	usb_find_devices();

	buslist = usb_get_busses();
	for_each_usbbus(bus, buslist) {
		for_each_usbdev(dev, bus->devices) {
			if (memcmp(desc, &dev->descriptor, sizeof(*desc)) != 0)
				continue;
			if (strncmp(dev->bus->dirname, dirname, PATH_MAX) != 0)
				continue;
			if (strncmp(dev->filename, filename, PATH_MAX) != 0)
				continue;
			/* found it! */
			return dev;
		}
	}

	return NULL;
}

/** razer_usb_reconnect_guard_wait - Protect against a firmware reconnect.
 *
 * If the firmware does a reconnect of the device on the USB bus, this
 * function tries to keep track of the device and it will update the
 * usb context information.
 * Of course, this is not completely race-free, but we try to do our best.
 */
int razer_usb_reconnect_guard_wait(struct razer_usb_reconnect_guard *guard)
{
	char reconn_filename[PATH_MAX + 1];
	unsigned int old_filename_nr;
	int res, errorcode = 0;
	struct usb_device *dev;
	struct timeval now, timeout;

	/* Release the device, so the kernel can detect the bus reconnect. */
	razer_generic_usb_release(guard->ctx);

	/* Wait for the device to disconnect. */
	gettimeofday(&now, NULL);
	memcpy(&timeout, &now, sizeof(timeout));
	timeval_add_msec(&timeout, 3000);
	while (guard_find_usb_dev(&guard->old_desc,
				  guard->old_dirname,
				  guard->old_filename)) {
		gettimeofday(&now, NULL);
		if (timeval_after(&now, &timeout)) {
			/* Timeout. Hm. It seems the device won't reconnect.
			 * That's OK. We can reclaim the device now. */
			dprintf("razer_usb_reconnect_guard: "
				"Didn't disconnect, huh?\n");
			goto reclaim;
		}
		razer_msleep(10);
	}

	/* Construct the filename the device will reconnect on. */
	res = sscanf(guard->old_filename, "%03u", &old_filename_nr);
	if (res != 1) {
		fprintf(stderr, "razer_usb_reconnect_guard: Could not parse filename.\n");
		errorcode = -EINVAL;
		goto reclaim;
	}
	snprintf(reconn_filename, sizeof(reconn_filename), "%03u",
		 old_filename_nr + 1);

	/* Wait for the device to reconnect. */
	gettimeofday(&now, NULL);
	memcpy(&timeout, &now, sizeof(timeout));
	timeval_add_msec(&timeout, 1000);
	while (1) {
		dev = guard_find_usb_dev(&guard->old_desc,
					 guard->old_dirname,
					 reconn_filename);
		if (dev)
			break;
		gettimeofday(&now, NULL);
		if (timeval_after(&now, &timeout)) {
			fprintf(stderr, "razer_usb_reconnect_guard: The device did not "
				"reconnect! It might not work anymore. Try to replug it.\n");
			errorcode = -EBUSY;
			goto out;
		}
		razer_msleep(1);
	}
	/* Update the USB context. */
	guard->ctx->dev = dev;

reclaim:
	/* Reclaim the new device. */
	res = razer_generic_usb_claim(guard->ctx);
	if (res) {
		fprintf(stderr, "razer_usb_reconnect_guard: Reclaim failed.\n");
		return res;
	}
out:
	return errorcode;
}
