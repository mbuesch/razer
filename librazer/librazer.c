/*
 *   Razer device access library
 *
 *   Copyright (C) 2007-2009 Michael Buesch <mb@bu3sch.de>
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
#include "hw_copperhead.h"

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

static const struct razer_mouse_base_ops razer_copperhead_base_ops = {
	.type			= RAZER_MOUSETYPE_COPPERHEAD,
	.gen_idstr		= razer_copperhead_gen_idstr,
	.init			= razer_copperhead_init_struct,
	.release		= razer_copperhead_release,
	.assign_usb_device	= razer_copperhead_assign_usb_device,
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
	USB_MOUSE(0x1532, 0x0016, &razer_deathadder_base_ops),
	USB_MOUSE(0x1532, 0x0003, &razer_krait_base_ops),
	USB_MOUSE(0x1532, 0x000C, &razer_lachesis_base_ops),
//FIXME	USB_MOUSE(0x1532, 0x0101, &razer_copperhead_base_ops),
	{ 0, }, /* List end */
};
#undef USB_MOUSE



static bool librazer_initialized;
static struct razer_mouse *mice_list = NULL;
/* We currently only have one handler. */
static razer_event_handler_t event_handler;


int razer_register_event_handler(razer_event_handler_t handler)
{
	if (event_handler)
		return -EEXIST;
	event_handler = handler;
	return 0;
}

void razer_unregister_event_handler(razer_event_handler_t handler)
{
	event_handler = NULL;
}

static void razer_notify_event(enum razer_event type,
			       const struct razer_event_data *data)
{
	if (event_handler)
		event_handler(type, data);
}

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
	struct razer_event_data ev;
	struct razer_mouse *m;
	int err;

	m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));
	m->flags |= RAZER_MOUSEFLG_NEW;
	m->base_ops = id->u.mouse_ops;
	err = m->base_ops->init(m, udev);
	if (err) {
		free(m);
		return NULL;
	}

	dprintf("Allocated and initialized new mouse (type=%d)\n",
		m->base_ops->type);

	ev.u.mouse = m;
	razer_notify_event(RAZER_EV_MOUSE_ADD, &ev);

	return m;
}

static void razer_free_mouse(struct razer_mouse *m)
{
	struct razer_event_data ev;

	dprintf("Freeing mouse (type=%d)\n",
		m->base_ops->type);

	ev.u.mouse = m;
	razer_notify_event(RAZER_EV_MOUSE_REMOVE, &ev);

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
	mice_list = NULL;
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
int razer_usb_force_reinit(struct razer_usb_context *device_ctx)
{
	struct usb_dev_handle *h;
	struct usb_device *hub;
	struct razer_usb_reconnect_guard rg;
	int err;

	dprintf("Forcing device reinitialization\n");

	razer_usb_reconnect_guard_init(&rg, device_ctx);

	hub = device_ctx->dev->bus->root_dev;
	dprintf("Resetting root hub %s:%s\n",
		hub->bus->dirname, hub->filename);

	h = usb_open(hub);
	if (!h) {
		fprintf(stderr, "razer_usb_force_reinit: usb_open failed\n");
		return -ENODEV;
	}
	usb_reset(h);
	usb_close(h);

	err = razer_usb_reconnect_guard_wait(&rg, 1);
	if (err) {
		fprintf(stderr, "razer_usb_force_reinit: "
			"Failed to discover the reconnected device\n");
	}

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
					      unsigned int filename,
					      bool hub_reset)
{
	struct usb_bus *bus, *buslist;
	struct usb_device *dev;
	unsigned int dev_filename;
	int res;

	usb_find_busses();
	usb_find_devices();

	buslist = usb_get_busses();
	for_each_usbbus(bus, buslist) {
		for_each_usbdev(dev, bus->devices) {
			if (memcmp(desc, &dev->descriptor, sizeof(*desc)) != 0)
				continue;
			if (strncmp(dev->bus->dirname, dirname, PATH_MAX) != 0)
				continue;
			res = sscanf(dev->filename, "%03u", &dev_filename);
			if (res != 1) {
				fprintf(stderr, "guard_find_usb_dev: Could not parse filename.\n");
				return NULL;
			}
			if (hub_reset) {
				if (dev_filename < filename)
					continue;
			} else {
				if (dev_filename != filename)
					continue;
			}
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
 *
 * hub_reset is true, if the device reconnects due to a HUB reset event.
 * Otherwise it's assumed that the device reconnects on behalf of itself.
 * If hub_reset is false, the device is expected to be claimed.
 */
int razer_usb_reconnect_guard_wait(struct razer_usb_reconnect_guard *guard, bool hub_reset)
{
	unsigned int reconn_filename;
	unsigned int old_filename_nr;
	int res, errorcode = 0;
	struct usb_device *dev;
	struct timeval now, timeout;

	if (!hub_reset) {
		/* Release the device, so the kernel can detect the bus reconnect. */
		razer_generic_usb_release(guard->ctx);
	}

	res = sscanf(guard->old_filename, "%03u", &old_filename_nr);
	if (res != 1) {
		fprintf(stderr, "razer_usb_reconnect_guard: Could not parse filename.\n");
		errorcode = -EINVAL;
		goto reclaim;
	}

	/* Wait for the device to disconnect. */
	gettimeofday(&now, NULL);
	memcpy(&timeout, &now, sizeof(timeout));
	timeval_add_msec(&timeout, 3000);
	while (guard_find_usb_dev(&guard->old_desc,
				  guard->old_dirname,
				  old_filename_nr,
				  hub_reset)) {
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

	/* Construct the filename the device will reconnect on.
	 * On a device reset the new filename will match reconn_filename.
	 * In case of a hub_reset, the new filename will be >= reconn_filename.
	 * FIXME: The 7bit(?) number will wrap.
	 */
	reconn_filename = old_filename_nr + 1;

	/* Wait for the device to reconnect. */
	gettimeofday(&now, NULL);
	memcpy(&timeout, &now, sizeof(timeout));
	timeval_add_msec(&timeout, 3000);
	while (1) {
		dev = guard_find_usb_dev(&guard->old_desc,
					 guard->old_dirname,
					 reconn_filename,
					 hub_reset);
		if (dev)
			break;
		gettimeofday(&now, NULL);
		if (timeval_after(&now, &timeout)) {
			fprintf(stderr, "razer_usb_reconnect_guard: The device did not "
				"reconnect! It might not work anymore. Try to replug it.\n");
			dprintf("Expected reconnect busid was: %s:%s%03u\n",
				guard->old_dirname, (hub_reset ? ">=" : ""), reconn_filename);
			errorcode = -EBUSY;
			goto out;
		}
		razer_msleep(1);
	}
	/* Update the USB context. */
	guard->ctx->dev = dev;

reclaim:
	if (!hub_reset) {
		/* Reclaim the new device. */
		res = razer_generic_usb_claim(guard->ctx);
		if (res) {
			fprintf(stderr, "razer_usb_reconnect_guard: Reclaim failed.\n");
			return res;
		}
	}
out:
	return errorcode;
}

le16_t razer_xor16_checksum(const void *_buffer, size_t size)
{
	const unsigned char *buffer = _buffer;
	uint16_t sum = 0;
	size_t i;

	for (i = 0; i < size; i += 2) {
		sum ^= buffer[i] & 0xFF;
		if (i < size - 1)
			sum ^= ((uint16_t)(buffer[i + 1])) << 8;
	}

	return cpu_to_le16(sum);
}

#ifdef DEBUG
void razer_dump(const char *prefix, const void *_buf, size_t size)
{
	const unsigned char *buf = _buf;
	size_t i;

	for (i = 0; i < size; i++) {
		if (i % 16 == 0) {
			if (i != 0)
				printf("\n");
			printf("%s-[%04X]:  ", prefix, i);
		}
		printf("%02X%s", buf[i], (i % 2) ? " " : "");
	}
	printf("\n\n");
}
#endif
