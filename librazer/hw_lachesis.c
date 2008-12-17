/*
 *   Lowlevel hardware access for the
 *   Razer Lachesis mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering, only.
 *
 *   Copyright (C) 2008 Michael Buesch <mb@bu3sch.de>
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

#include "hw_lachesis.h"
#include "razer_private.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>


struct lachesis_private {
	bool claimed;
	struct usb_device *usbdev;
	struct razer_usb_context usb;
	/* The currently set resolution. */
	enum razer_mouse_res resolution;
};

#define LACHESIS_USB_TIMEOUT	3000

static int lachesis_usb_write(struct lachesis_private *priv,
			      int request, int command,
			      char *buf, size_t size)
{
	int i, err;

	/* Send a command a few times. Otherwise it might get lost somehow.
	 * FIXME: Check why. */
	for (i = 0; i < 5; i++) {
		err = usb_control_msg(priv->usb.h,
				      USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				      request, command, 0,
				      buf, size,
				      LACHESIS_USB_TIMEOUT);
	}
	if (err != size)
		return err;
	return 0;
}

#if 0
static int lachesis_usb_read(struct lachesis_private *priv,
			     int request, int command,
			     char *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      LACHESIS_USB_TIMEOUT);
	if (err != size)
		return err;
	return 0;
}
#endif

static int lachesis_claim(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;
	int err;

	err = razer_generic_usb_claim(priv->usbdev, &priv->usb);
	if (err)
		return err;
	priv->claimed = 1;

	return 0;
}

static void lachesis_release(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	if (!priv->claimed)
		return;

	razer_generic_usb_release(priv->usbdev, &priv->usb);
	priv->claimed = 0;
}

static int lachesis_get_fw_version(struct razer_mouse *m)
{
	return -EOPNOTSUPP;
}

static int lachesis_get_leds(struct razer_mouse *m,
			     struct razer_led **leds_list)
{
	return 0; //TODO
}

static int lachesis_supported_freqs(struct razer_mouse *m,
				    enum razer_mouse_freq **freq_list)
{
	enum razer_mouse_freq *list;
	const int count = 1;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

//FIXME
	list[0] = RAZER_MOUSE_FREQ_UNKNOWN;

	*freq_list = list;

	return count;
}

static enum razer_mouse_freq lachesis_get_freq(struct razer_mouse *m)
{
	return RAZER_MOUSE_FREQ_UNKNOWN;
}

static int lachesis_set_freq(struct razer_mouse *m,
			  enum razer_mouse_freq freq)
{
	return -EOPNOTSUPP;
}

static int lachesis_supported_resolutions(struct razer_mouse *m,
				       enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	const int count = 2;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

//FIXME
	list[0] = RAZER_MOUSE_RES_400DPI;
	list[1] = RAZER_MOUSE_RES_1600DPI;

	*res_list = list;

	return count;
}

static enum razer_mouse_res lachesis_get_resolution(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	return priv->resolution;
}

static int lachesis_set_resolution(struct razer_mouse *m,
				     enum razer_mouse_res res)
{
	struct lachesis_private *priv = m->internal;
	char value;
	int err;

//FIXME
	switch (res) {
	case RAZER_MOUSE_RES_400DPI:
		value = 6;
		break;
	case RAZER_MOUSE_RES_1600DPI:
		value = 4;
		break;
	default:
		return -EINVAL;
	}
	if (!priv->claimed)
		return -EBUSY;

	err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				   0x02, &value, sizeof(value));
	if (!err)
		priv->resolution = res;

	return err;
}

void razer_lachesis_gen_idstr(struct usb_device *udev, char *buf)
{
	/* We can't include the USB device number, because that changes on the
	 * automatic reconnects the device firmware does.
	 * The serial number is zero, so that's not very useful, too.
	 * Basically, that means we have a pretty bad ID string due to
	 * major design faults in the hardware. :(
	 */
	snprintf(buf, RAZER_IDSTR_MAX_SIZE, "lachesis:usb-%s:%04X:%04X:%02X",
		 udev->bus->dirname,
		 udev->descriptor.idVendor,
		 udev->descriptor.idProduct,
		 udev->descriptor.iSerialNumber);
}

int razer_lachesis_init_struct(struct razer_mouse *m,
				 struct usb_device *usbdev)
{
	struct lachesis_private *priv;

	priv = malloc(sizeof(struct lachesis_private));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));

	priv->usbdev = usbdev;
	priv->resolution = RAZER_MOUSE_RES_UNKNOWN;

	m->internal = priv;
	m->type = RAZER_MOUSETYPE_LACHESIS;
	razer_lachesis_gen_idstr(usbdev, m->idstr);

	m->claim = lachesis_claim;
	m->release = lachesis_release;
	m->get_fw_version = lachesis_get_fw_version;
	m->get_leds = lachesis_get_leds;
	m->supported_freqs = lachesis_supported_freqs;
	m->get_freq = lachesis_get_freq;
	m->set_freq = lachesis_set_freq;
	m->supported_resolutions = lachesis_supported_resolutions;
	m->get_resolution = lachesis_get_resolution;
	m->set_resolution = lachesis_set_resolution;

	return 0;
}

void razer_lachesis_release(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	lachesis_release(m);
	free(priv);
}
