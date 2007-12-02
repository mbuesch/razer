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

#include "hw_deathadder.h"
#include "razer_private.h"

#include <usb.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>


enum razer_devtype {
	RAZER_DEVTYPE_MOUSE,
};

struct razer_usb_device {
	uint16_t vendor;	/* Vendor ID */
	uint16_t product;	/* Product ID */
	enum razer_devtype type;
	union {
		enum razer_mouse_type mouse_type;
	} u;
};

#define USBVENDOR_ANY	0xFFFF
#define USBPRODUCT_ANY	0xFFFF

#define USB_MOUSE(_vendor, _product, _mouse_type)	\
	{ .vendor = _vendor, .product = _product,	\
	  .type = RAZER_DEVTYPE_MOUSE,			\
	  .u = { .mouse_type = _mouse_type, }, }

/* Table of supported USB devices. */
static const struct razer_usb_device razer_usbdev_table[] = {
	USB_MOUSE(0x1532, 0x0007, RAZER_MOUSETYPE_DEATHADDER),
	USB_MOUSE(0x1532, 0x0003, RAZER_MOUSETYPE_KRAIT),
	{ 0, }, /* List end */
};
#undef USB_MOUSE


#define for_each_usbbus(bus, buslist) \
	for (bus = buslist; bus; bus = bus->next)
#define for_each_usbdev(dev, devlist) \
	for (dev = devlist; dev; dev = dev->next)


static bool librazer_initialized;


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

int razer_scan_mice(struct razer_mouse **mice_list)
{
	struct usb_bus *bus, *buslist;
	struct usb_device *dev;
	const struct usb_device_descriptor *desc;
	const struct razer_usb_device *id;
	struct razer_mouse *list = NULL, *mouse, *prev_mouse = NULL;
	int err, count = 0;

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
			count++;
			err = -ENOMEM;
			mouse = malloc(sizeof(struct razer_mouse));
			if (!mouse)
				goto err_unwind;
			memset(mouse, 0, sizeof(*mouse));
			if (!list)
				list = mouse;
			switch (id->u.mouse_type) {
			case RAZER_MOUSETYPE_DEATHADDER:
				err = razer_deathadder_init_struct(mouse, dev);
				if (err)
					goto err_unwind;
				break;
			case RAZER_MOUSETYPE_KRAIT:
				//TODO
				break;
			case RAZER_MOUSETYPE_LACHESIS:
				//TODO
				break;
			}
			if (prev_mouse)
				prev_mouse->next = mouse;
			prev_mouse = mouse;
		}
	}
	*mice_list = list;

	return count;

err_unwind:
	for (mouse = list; mouse; ) {
		struct razer_mouse *next = mouse->next;
		free(mouse);
		mouse = next;
	}
	return err;
}

void razer_free_mice(struct razer_mouse *mouse_list)
{
	struct razer_mouse *mouse, *next;

	for (mouse = mouse_list; mouse; ) {
		next = mouse->next;

		switch (mouse->type) {
		case RAZER_MOUSETYPE_DEATHADDER:
			razer_deathadder_release(mouse);
			break;
		case RAZER_MOUSETYPE_KRAIT:
			//TODO
			break;
		case RAZER_MOUSETYPE_LACHESIS:
			//TODO
			break;
		}
		free(mouse);

		mouse = next;
	}
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
