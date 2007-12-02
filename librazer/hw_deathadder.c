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
#include <string.h>
#include <usb.h>


enum {
	DEATHADDER_LED_SCROLL = 0,
	DEATHADDER_LED_LOGO,
	DEATHADDER_NR_LEDS,
};


struct deathadder_private {
	bool claimed;
	struct usb_device *usbdev;
	struct razer_usb_context usb;
	bool led_states[DEATHADDER_NR_LEDS];
};


static int deathadder_claim(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;
	int err;

	err = razer_generic_usb_claim(priv->usbdev, &priv->usb);
	if (err)
		return err;
	priv->claimed = 1;

	return 0;
}

static void deathadder_release(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;

	razer_generic_usb_release(priv->usbdev, &priv->usb);
	priv->claimed = 0;
}

#define DADD_CFG_GLOW			0x6
#define  DADD_CFG_GLOW_LOGO		(1 << 0)
#define  DADD_CFG_GLOW_WHEEL		(1 << 1)
static int x(struct deathadder_private *p, int on)
{
	int err;
	char buf[1];

	buf[0] = 0;
	if (on)
		buf[0] |= DADD_CFG_GLOW_LOGO;
	if (on)
		buf[0] |= DADD_CFG_GLOW_WHEEL;
	err = usb_control_msg(p->usb.h, USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      USB_REQ_SET_CONFIGURATION,
			      DADD_CFG_GLOW, 0, buf, sizeof(buf),
			      1000);
	if (err == 1)
		return 0;
	return -EINVAL;
}

static int deathadder_led_toggle(struct razer_led *led,
				 enum razer_led_state new_state)
{
	struct razer_mouse *m = led->u.mouse;
	struct deathadder_private *priv = m->internal;

	if (!priv->claimed)
		return -EBUSY;

return x(priv, new_state);
	//TODO

	return 0;
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
	//TODO
	return RAZER_MOUSE_FREQ_UNKNOWN;
}

static int deathadder_set_freq(struct razer_mouse *m,
			       enum razer_mouse_freq freq)
{
	//TODO
	return 0;
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
	//TODO
	return RAZER_MOUSE_RES_UNKNOWN;
}

static int deathadder_set_resolution(struct razer_mouse *m,
				     enum razer_mouse_res res)
{
	//TODO
	return 0;
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

	m->internal = priv;
	m->type = RAZER_MOUSETYPE_DEATHADDER;
	snprintf(m->busid, sizeof(m->busid), "usb:%s-%s",
		 usbdev->bus->dirname,
		 usbdev->filename);

	m->claim = deathadder_claim;
	m->release = deathadder_release;
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

	if (priv->claimed)
		deathadder_release(m);
	free(priv);
}
