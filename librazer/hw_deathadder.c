/*
 *   Lowlevel hardware access for the
 *   Razer Deathadder mouse
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

#include "hw_deathadder.h"
#include "razer_private.h"

#include <usb.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>


enum {
	DEATHADDER_LED_SCROLL = 0,
	DEATHADDER_LED_LOGO,
	DEATHADDER_NR_LEDS,
};


struct deathadder_private {
	bool claimed;
	/* Firmware version number. */
	uint16_t fw_version;
	struct usb_device *usbdev;
	struct razer_usb_context usb;
	/* The currently set LED states. */
	bool led_states[DEATHADDER_NR_LEDS];
	/* The currently set frequency. */
	enum razer_mouse_freq frequency;
	/* The currently set resolution. */
	enum razer_mouse_res resolution;
};

#define DEATHADDER_USB_TIMEOUT	3000

static int deathadder_usb_write(struct deathadder_private *priv,
				int request, int command,
				char *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      DEATHADDER_USB_TIMEOUT);
	if (err != size)
		return err;
	return 0;
}

static int deathadder_usb_read(struct deathadder_private *priv,
			       int request, int command,
			       char *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      DEATHADDER_USB_TIMEOUT);
	if (err != size)
		return err;
	return 0;
}

static int deathadder_read_fw_ver(struct deathadder_private *priv)
{
	char buf[2];
	uint16_t ver;
	int err;

	err = deathadder_usb_read(priv, USB_REQ_CLEAR_FEATURE,
				  0x05, buf, sizeof(buf));
	if (err)
		return err;
	ver = buf[0];
	ver <<= 8;
	ver |= buf[1];

	return ver;
}

static int deathadder_claim(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;
	int err, fwver;

	err = razer_generic_usb_claim(priv->usbdev, &priv->usb);
	if (err)
		return err;
	fwver = deathadder_read_fw_ver(priv);
	if (fwver < 0)
		return fwver;
	priv->fw_version = fwver;
	priv->claimed = 1;

	return 0;
}

static void deathadder_release(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;

	if (!priv->claimed)
		return;

	razer_generic_usb_release(priv->usbdev, &priv->usb);
	priv->claimed = 0;
}

static int deathadder_get_fw_version(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;

	/* Version is read on claim. */
	if (!priv->claimed)
		return -EBUSY;
	return priv->fw_version;
}

static int deathadder_led_toggle(struct razer_led *led,
				 enum razer_led_state new_state)
{
	struct razer_mouse *m = led->u.mouse;
	struct deathadder_private *priv = m->internal;
	char value = 0;
	int err;

	if (led->id >= DEATHADDER_NR_LEDS)
		return -EINVAL;
	if ((new_state != RAZER_LED_OFF) &&
	    (new_state != RAZER_LED_ON))
		return -EINVAL;

	if (!priv->claimed)
		return -EBUSY;

	priv->led_states[led->id] = new_state;

	if (priv->led_states[DEATHADDER_LED_LOGO])
		value |= 0x01;
	if (priv->led_states[DEATHADDER_LED_SCROLL])
		value |= 0x02;
	err = deathadder_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				   0x06, &value, sizeof(value));

	return err;
}

static int deathadder_get_leds(struct razer_mouse *m,
			       struct razer_led **leds_list)
{
	struct deathadder_private *priv = m->internal;
	struct razer_led *scroll, *logo;

	scroll = malloc(sizeof(struct razer_led));
	if (!scroll)
		return -ENOMEM;
	logo = malloc(sizeof(struct razer_led));
	if (!logo) {
		free(scroll);
		return -ENOMEM;
	}

	scroll->name = "Scrollwheel";
	scroll->id = DEATHADDER_LED_SCROLL;
	scroll->state = RAZER_LED_UNKNOWN;
	scroll->toggle_state = deathadder_led_toggle;
	scroll->u.mouse = m;

	logo->name = "GlowingLogo";
	logo->id = DEATHADDER_LED_LOGO;
	logo->state = RAZER_LED_UNKNOWN;
	logo->toggle_state = deathadder_led_toggle;
	logo->u.mouse = m;

	/* Link the list */
	*leds_list = scroll;
	scroll->next = logo;
	logo->next = NULL;

	return DEATHADDER_NR_LEDS;
}

static int deathadder_supported_freqs(struct razer_mouse *m,
				      enum razer_mouse_freq **freq_list)
{
	enum razer_mouse_freq *list;
	const int count = 3;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	list[0] = RAZER_MOUSE_FREQ_125HZ;
	list[1] = RAZER_MOUSE_FREQ_500HZ;
	list[2] = RAZER_MOUSE_FREQ_1000HZ;

	*freq_list = list;

	return count;
}

static enum razer_mouse_freq deathadder_get_freq(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;

	return priv->frequency;
}

static int deathadder_set_freq(struct razer_mouse *m,
			       enum razer_mouse_freq freq)
{
	struct deathadder_private *priv = m->internal;
	char value;
	int err;

	switch (freq) {
	case RAZER_MOUSE_FREQ_125HZ:
		value = 3;
		break;
	case RAZER_MOUSE_FREQ_500HZ:
		value = 2;
		break;
	case RAZER_MOUSE_FREQ_1000HZ:
		value = 1;
		break;
	default:
		return -EINVAL;
	}
	if (!priv->claimed)
		return -EBUSY;

	err = deathadder_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				   0x07, &value, sizeof(value));
	if (!err)
		priv->frequency = freq;

	return err;
}

static int deathadder_supported_resolutions(struct razer_mouse *m,
					    enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	const int count = 3;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	list[0] = RAZER_MOUSE_RES_450DPI;
	list[1] = RAZER_MOUSE_RES_900DPI;
	list[2] = RAZER_MOUSE_RES_1800DPI;

	*res_list = list;

	return count;
}

static enum razer_mouse_res deathadder_get_resolution(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;

	return priv->resolution;
}

static int deathadder_set_resolution(struct razer_mouse *m,
				     enum razer_mouse_res res)
{
	struct deathadder_private *priv = m->internal;
	char value;
	int err;

	switch (res) {
	case RAZER_MOUSE_RES_450DPI:
		value = 3;
		break;
	case RAZER_MOUSE_RES_900DPI:
		value = 2;
		break;
	case RAZER_MOUSE_RES_1800DPI:
		value = 1;
		break;
	default:
		return -EINVAL;
	}
	if (!priv->claimed)
		return -EBUSY;

	err = deathadder_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				   0x09, &value, sizeof(value));
	if (!err)
		priv->resolution = res;

	return err;
}

int razer_deathadder_init_struct(struct razer_mouse *m,
				 struct usb_device *usbdev)
{
	struct deathadder_private *priv;

	priv = malloc(sizeof(struct deathadder_private));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));

	priv->usbdev = usbdev;
	priv->frequency = RAZER_MOUSE_FREQ_UNKNOWN;
	priv->resolution = RAZER_MOUSE_RES_UNKNOWN;

	m->internal = priv;
	m->type = RAZER_MOUSETYPE_DEATHADDER;
	snprintf(m->busid, sizeof(m->busid), "usb:%s-%s",
		 usbdev->bus->dirname,
		 usbdev->filename);

	m->claim = deathadder_claim;
	m->release = deathadder_release;
	m->get_fw_version = deathadder_get_fw_version;
	m->get_leds = deathadder_get_leds;
	m->supported_freqs = deathadder_supported_freqs;
	m->get_freq = deathadder_get_freq;
	m->set_freq = deathadder_set_freq;
	m->supported_resolutions = deathadder_supported_resolutions;
	m->get_resolution = deathadder_get_resolution;
	m->set_resolution = deathadder_set_resolution;

	return 0;
}

void razer_deathadder_release(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;

	deathadder_release(m);
	free(priv);
}
