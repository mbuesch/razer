/*
 *   Lowlevel hardware access for the
 *   Razer Krait mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering, only.
 *
 *   Copyright (C) 2007-2009 Michael Buesch <m@bues.ch>
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


struct krait_private {
	struct razer_mouse *m;

	struct razer_mouse_dpimapping *cur_dpimapping;
	struct razer_mouse_profile profile;
	struct razer_mouse_dpimapping dpimapping[2];
};


static int krait_usb_write(struct krait_private *priv,
			   int request, int command,
			   void *buf, size_t size)
{
	int err;

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, 0,
		buf, size,
		RAZER_USB_TIMEOUT);
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

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, 0,
		(unsigned char *)buf, size,
		RAZER_USB_TIMEOUT);
	if (err != size)
		return err;
	return 0;
}
#endif

static int krait_commit(struct krait_private *priv)
{
	uint8_t value;
	int err;

	switch (priv->cur_dpimapping->res) {
	case RAZER_MOUSE_RES_400DPI:
		value = 6;
		break;
	case RAZER_MOUSE_RES_1600DPI:
		value = 4;
		break;
	default:
		return -EINVAL;
	}
	err = krait_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
			      0x02, &value, sizeof(value));
	if (err)
		return err;

	return 0;
}

static int krait_reconfigure(struct razer_mouse *m)
{
	struct krait_private *priv = m->drv_data;

	if (!m->claim_count)
		return -EBUSY;
	return krait_commit(priv);
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
	struct krait_private *priv = m->drv_data;

	return &priv->profile;
}

static int krait_supported_dpimappings(struct razer_mouse *m,
				       struct razer_mouse_dpimapping **res_ptr)
{
	struct krait_private *priv = m->drv_data;

	*res_ptr = &priv->dpimapping[0];

	return ARRAY_SIZE(priv->dpimapping);
}

static struct razer_mouse_dpimapping * krait_get_dpimapping(struct razer_mouse_profile *p,
							    struct razer_axis *axis)
{
	struct krait_private *priv = p->mouse->drv_data;

	return priv->cur_dpimapping;
}

static int krait_set_dpimapping(struct razer_mouse_profile *p,
				struct razer_axis *axis,
				struct razer_mouse_dpimapping *d)
{
	struct krait_private *priv = p->mouse->drv_data;
	struct razer_mouse_dpimapping *old_d;
	int err;

	if (!priv->m->claim_count)
		return -EBUSY;

	old_d = priv->cur_dpimapping;
	priv->cur_dpimapping = d;
	err = krait_commit(priv);
	if (err) {
		priv->cur_dpimapping = old_d;
		return err;
	}

	return 0;
}

int razer_krait_init(struct razer_mouse *m,
		     struct libusb_device *usbdev)
{
	struct krait_private *priv;
	int err;

	priv = zalloc(sizeof(struct krait_private));
	if (!priv)
		return -ENOMEM;
	priv->m = m;
	m->drv_data = priv;

	err = razer_usb_add_used_interface(m->usb_ctx, 0, 0);
	if (err) {
		free(priv);
		return err;
	}

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
	razer_generic_usb_gen_idstr(usbdev, NULL, "Krait", 1,
				    NULL, m->idstr);

	m->reconfigure = krait_reconfigure;
	m->get_profiles = krait_get_profiles;
	m->supported_resolutions = krait_supported_resolutions;
	m->supported_dpimappings = krait_supported_dpimappings;

	return 0;
}

void razer_krait_release(struct razer_mouse *m)
{
	struct krait_private *priv = m->drv_data;

	free(priv);
}
