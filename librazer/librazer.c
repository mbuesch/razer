/*
 *   Razer device access library
 *
 *   Copyright (C) 2007 Michael Buesch <mb@bu3sch.de>
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
#include <sys/ioctl.h>
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
 */
struct razer_mouse_base_ops {
	enum razer_mouse_type type;
	void (*gen_idstr)(struct usb_device *udev, char *buf);
	int (*init)(struct razer_mouse *m, struct usb_device *udev);
	void (*release)(struct razer_mouse *m);
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
	.type		= RAZER_MOUSETYPE_DEATHADDER,
	.gen_idstr	= razer_deathadder_gen_idstr,
	.init		= razer_deathadder_init_struct,
	.release	= razer_deathadder_release,
};

static const struct razer_mouse_base_ops razer_krait_base_ops = {
	.type		= RAZER_MOUSETYPE_KRAIT,
	.gen_idstr	= razer_krait_gen_idstr,
	.init		= razer_krait_init_struct,
	.release	= razer_krait_release,
};

static const struct razer_mouse_base_ops razer_lachesis_base_ops = {
	.type		= RAZER_MOUSETYPE_LACHESIS,
	.gen_idstr	= razer_lachesis_gen_idstr,
	.init		= razer_lachesis_init_struct,
	.release	= razer_lachesis_release,
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


#define for_each_usbbus(bus, buslist) \
	for (bus = buslist; bus; bus = bus->next)
#define for_each_usbdev(dev, devlist) \
	for (dev = devlist; dev; dev = dev->next)


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

	return m;
}

static void razer_free_mouse(struct razer_mouse *m)
{
	m->base_ops->release(m);
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
	if (err == -EBUSY) {
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
		err = usb_detach_kernel_driver_np(ctx->h, ctx->interf);
		if (err)
			return err;
		ctx->kdrv_detached = 1;
		err = usb_claim_interface(ctx->h, ctx->interf);
		if (err) {
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

int razer_generic_usb_claim(struct usb_device *dev,
			    struct razer_usb_context *ctx)
{
	int err;

	ctx->interf = dev->config->interface->altsetting[0].bInterfaceNumber;
	ctx->h = usb_open(dev);
	if (!ctx->h)
		return -ENODEV;
	err = razer_usb_claim(ctx);
	if (err)
		return err;

	return 0;
}

void razer_generic_usb_release(struct usb_device *dev,
			       struct razer_usb_context *ctx)
{
	razer_usb_release(ctx);
	usb_close(ctx->h);
}
