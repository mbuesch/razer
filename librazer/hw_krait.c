/*
 *   Lowlevel hardware access for the
 *   Razer Krait mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering, only.
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

#include "hw_krait.h"
#include "razer_private.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>


struct krait_private {
	unsigned int claimed;
	struct razer_usb_context usb;
	struct razer_mouse_dpimapping *cur_dpimapping;
	struct razer_mouse_profile profile;
	struct razer_mouse_dpimapping dpimapping[2];
};

#define KRAIT_USB_TIMEOUT	3000

static int krait_usb_write(struct krait_private *priv,
			   int request, int command,
			   void *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      KRAIT_USB_TIMEOUT);
	if (err != size)
		return err;
	return 0;
}

#if 0
static int krait_usb_read(struct krait_private *priv,
			  int request, int command,
			  char *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      KRAIT_USB_TIMEOUT);
	if (err != size)
		return err;
	return 0;
}
#endif

static int krait_claim(struct razer_mouse *m)
{
	struct krait_private *priv = m->internal;
	int err;

	if (!priv->claimed) {
		err = razer_generic_usb_claim(&priv->usb);
		if (err)
			return err;
	}
	priv->claimed++;

	return 0;
}

static void krait_release(struct razer_mouse *m)
{
	struct krait_private *priv = m->internal;

	priv->claimed--;
	if (!priv->claimed)
		razer_generic_usb_release(&priv->usb);
}

static int krait_supported_resolutions(struct razer_mouse *m,
				       enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	const int count = 2;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	list[0] = RAZER_MOUSE_RES_400DPI;
	list[1] = RAZER_MOUSE_RES_1600DPI;

	*res_list = list;

	return count;
}

static struct razer_mouse_profile * krait_get_profiles(struct razer_mouse *m)
{
	struct krait_private *priv = m->internal;

	return &priv->profile;
}

static struct razer_mouse_profile * krait_get_active_profile(struct razer_mouse *m)
{
	struct krait_private *priv = m->internal;

	return &priv->profile;
}

static int krait_supported_dpimappings(struct razer_mouse *m,
				       struct razer_mouse_dpimapping **res_ptr)
{
	struct krait_private *priv = m->internal;

	*res_ptr = &priv->dpimapping[0];

	return ARRAY_SIZE(priv->dpimapping);
}

static struct razer_mouse_dpimapping * krait_get_dpimapping(struct razer_mouse_profile *p,
							    struct razer_axis *axis)
{
	struct krait_private *priv = p->mouse->internal;

	return priv->cur_dpimapping;
}

static int krait_set_dpimapping(struct razer_mouse_profile *p,
				struct razer_axis *axis,
				struct razer_mouse_dpimapping *d)
{
	struct krait_private *priv = p->mouse->internal;
	int err;
	char value;

	if (!priv->claimed)
		return -EBUSY;

	switch (d->res) {
	case RAZER_MOUSE_RES_400DPI:
		value = 6;
		break;
	case RAZER_MOUSE_RES_1600DPI:
		value = 4;
		break;
	default:
		return -EINVAL;
	}
	err = krait_usb_write(priv, USB_REQ_SET_CONFIGURATION,
			      0x02, &value, sizeof(value));
	if (!err)
		priv->cur_dpimapping = d;

	return err;
}

void razer_krait_gen_idstr(struct usb_device *udev, char *buf)
{
	char devid[64];
	char serial[64];
	char buspos[512];
	unsigned int serial_index;
	int err;
	struct razer_usb_context usbctx = { .dev = udev, };

	err = -EINVAL;
	serial_index = udev->descriptor.iSerialNumber;
	if (serial_index) {
		err = razer_generic_usb_claim(&usbctx);
		if (err) {
			razer_error("Failed to claim device for serial fetching.\n");
		} else {
			err = usb_get_string_simple(usbctx.h, serial_index,
						    serial, sizeof(serial));
			razer_generic_usb_release(&usbctx);
		}
	}
	if (err <= 0)
		strcpy(serial, "0");

	snprintf(devid, sizeof(devid), "%04X-%04X-%s",
		 udev->descriptor.idVendor,
		 udev->descriptor.idProduct, serial);
	snprintf(buspos, sizeof(buspos), "%s-%s",
		 udev->bus->dirname, udev->filename);

	razer_create_idstr(buf, BUSTYPESTR_USB, buspos,
			   DEVTYPESTR_MOUSE, "Krait", devid);
}

void razer_krait_assign_usb_device(struct razer_mouse *m,
				   struct usb_device *usbdev)
{
	struct krait_private *priv = m->internal;

	priv->usb.dev = usbdev;
}

int razer_krait_init_struct(struct razer_mouse *m,
				 struct usb_device *usbdev)
{
	struct krait_private *priv;

	priv = malloc(sizeof(struct krait_private));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));
	m->internal = priv;

	razer_krait_assign_usb_device(m, usbdev);

	priv->profile.nr = 0;
	priv->profile.get_dpimapping = krait_get_dpimapping;
	priv->profile.set_dpimapping = krait_set_dpimapping;
	priv->profile.mouse = m;

	priv->dpimapping[0].nr = 0;
	priv->dpimapping[0].res = RAZER_MOUSE_RES_400DPI;
	priv->dpimapping[0].change = NULL;
	priv->dpimapping[0].mouse = m;

	priv->dpimapping[1].nr = 1;
	priv->dpimapping[1].res = RAZER_MOUSE_RES_1600DPI;
	priv->dpimapping[1].change = NULL;
	priv->dpimapping[1].mouse = m;

	priv->cur_dpimapping = &priv->dpimapping[1];

	m->type = RAZER_MOUSETYPE_KRAIT;
	razer_krait_gen_idstr(usbdev, m->idstr);

	m->claim = krait_claim;
	m->release = krait_release;
	m->nr_profiles = 1;
	m->get_profiles = krait_get_profiles;
	m->get_active_profile = krait_get_active_profile;
	m->supported_resolutions = krait_supported_resolutions;
	m->supported_dpimappings = krait_supported_dpimappings;

	return 0;
}

void razer_krait_release(struct razer_mouse *m)
{
	struct krait_private *priv = m->internal;

	krait_release(m);
	free(priv);
}
