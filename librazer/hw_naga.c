/*
 *   Lowlevel hardware access for the
 *   Razer Naga mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering, only.
 *
 *   Copyright (C) 2007-2010 Michael Buesch <mb@bu3sch.de>
 *   Copyright (C) 2010 Bernd Michael Helm <naga@rw23.de>
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

#include "hw_naga.h"
#include "razer_private.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>


enum {
	NAGA_LED_SCROLL = 0,
	NAGA_LED_LOGO,
	NAGA_NR_LEDS,
};

enum { /* Misc constants */
	NAGA_NR_DPIMAPPINGS	= 56,
	NAGA_NR_AXES		= 3,
};

struct naga_command {
	le16_t status;
	uint8_t padding0[2];
	le16_t command;
	le16_t request;
	le16_t value0;
	le16_t value1;
	uint8_t padding1[76];
	uint8_t checksum;
	uint8_t padding2;
} _packed;

struct naga_private {
	unsigned int claimed;
	/* Firmware version number. */
	uint16_t fw_version;
	/* USB context */
	struct razer_usb_context usb;
	/* The currently set LED states. */
	bool led_states[NAGA_NR_LEDS];
	/* The currently set frequency. */
	enum razer_mouse_freq frequency;
	/* The currently set resolution. */
	struct razer_mouse_dpimapping *cur_dpimapping_X;
	struct razer_mouse_dpimapping *cur_dpimapping_Y;

	struct razer_mouse_profile profile;
	struct razer_mouse_dpimapping dpimapping[NAGA_NR_DPIMAPPINGS];
	struct razer_axis axes[NAGA_NR_AXES];
};

#define NAGA_USB_TIMEOUT	3000

static int naga_usb_write(struct naga_private *priv,
				int request, int command,
				const void *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      (char *)buf, size,
			      NAGA_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-naga: "
			"USB write 0x%02X 0x%02X failed: %d\n",
			request, command, err);
		return err;
	}
	return 0;
}

static int naga_usb_read(struct naga_private *priv,
			       int request, int command,
			       void *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      NAGA_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-naga: "
			"USB read 0x%02X 0x%02X failed: %d\n",
			request, command, err);
		return err;
	}
	return 0;
}

static int naga_send_command(struct naga_private *priv,
			     struct naga_command *cmd)
{
	int err;

	cmd->checksum = razer_xor8_checksum(cmd, sizeof(*cmd) - 2);
	err = naga_usb_write(priv, USB_REQ_SET_CONFIGURATION, 0x300,
			     cmd, sizeof(*cmd));
	if (err)
		return err;
	err = naga_usb_read(priv, USB_REQ_CLEAR_FEATURE, 0x300,
			    cmd, sizeof(*cmd));
	if (err)
		return err;
	if (cmd->status != cpu_to_le16(2) &&
	    cmd->status != cpu_to_le16(1)) {
		razer_error("razer-naga: Command %04X/%04X failed with %04X\n",
			    le16_to_cpu(cmd->command),
			    le16_to_cpu(cmd->request),
			    le16_to_cpu(cmd->status));
//		return -EBUSY;
	}

	return 0;
}

static int naga_read_fw_ver(struct naga_private *priv)
{
	struct naga_command cmd;
	uint16_t ver;
	int err;
	unsigned int i;

	/* Poke the device several times until it responds with a
	 * valid version number */
	for (i = 0; i < 5; i++) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.command = cpu_to_le16(0x0200);
		cmd.request = cpu_to_le16(0x8100);
		err = naga_send_command(priv, &cmd);
		ver = be16_to_cpu((be16_t)cmd.value0);
		if (!err && (ver & 0xFF00) != 0)
			return ver;
		razer_msleep(100);
	}
	razer_error("razer-naga: Failed to read firmware version\n");

	return -ENODEV;
}

static int naga_commit(struct naga_private *priv)
{
	struct naga_command cmd;
	unsigned int xres, yres, freq;
	int err;

	/* Set the resolution. */
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = cpu_to_le16(0x0300);
	cmd.request = cpu_to_le16(0x0104);
	xres = (((unsigned int)priv->cur_dpimapping_X->res / 100) - 1) * 4;
	yres = (((unsigned int)priv->cur_dpimapping_Y->res / 100) - 1) * 4;
	cmd.value0 = cpu_to_le16(xres | (yres << 8));
	err = naga_send_command(priv, &cmd);
	if (err)
		return err;

	/* Set the scroll wheel and buttons LEDs. */
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = cpu_to_le16(0x0300);
	cmd.request = cpu_to_le16(0x0003);
	cmd.value0 = cpu_to_le16(0x0101);
	if (priv->led_states[NAGA_LED_SCROLL])
		cmd.value1 = cpu_to_le16(1);
	err = naga_send_command(priv, &cmd);
	if (err)
		return err;

	/* Set the logo LED. */
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = cpu_to_le16(0x0300);
	cmd.request = cpu_to_le16(0x0003);
	cmd.value0 = cpu_to_le16(0x0401);
	if (priv->led_states[NAGA_LED_LOGO])
		cmd.value1 = cpu_to_le16(1);
	err = naga_send_command(priv, &cmd);
	if (err)
		return err;

	/* Set scan frequency. */
	switch (priv->frequency) {
	case RAZER_MOUSE_FREQ_125HZ:
		freq = 8;
		break;
	case RAZER_MOUSE_FREQ_500HZ:
		freq = 2;
		break;
	case RAZER_MOUSE_FREQ_1000HZ:
	case RAZER_MOUSE_FREQ_UNKNOWN:
		freq = 1;
		break;
	default:
		return -EINVAL;
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = cpu_to_le16(0x0100);
	cmd.request = cpu_to_le16(0x0500);
	cmd.value0 = cpu_to_le16(freq);
	err = naga_send_command(priv, &cmd);
	if (err)
		return err;

	return 0;
}

static int naga_claim(struct razer_mouse *m)
{
	struct naga_private *priv = m->internal;

	return razer_generic_usb_claim_refcount(&priv->usb, &priv->claimed);
}

static void naga_release(struct razer_mouse *m)
{
	struct naga_private *priv = m->internal;

	razer_generic_usb_release_refcount(&priv->usb, &priv->claimed);
}

static int naga_get_fw_version(struct razer_mouse *m)
{
	struct naga_private *priv = m->internal;

	return priv->fw_version;
}

static int naga_led_toggle(struct razer_led *led,
				 enum razer_led_state new_state)
{
	struct razer_mouse *m = led->u.mouse;
	struct naga_private *priv = m->internal;
	int err;
	enum razer_led_state old_state;

	if (led->id >= NAGA_NR_LEDS)
		return -EINVAL;
	if ((new_state != RAZER_LED_OFF) &&
	    (new_state != RAZER_LED_ON))
		return -EINVAL;

	if (!priv->claimed)
		return -EBUSY;

	old_state = priv->led_states[led->id];
	priv->led_states[led->id] = new_state;

	err = naga_commit(priv);
	if (err) {
		priv->led_states[led->id] = old_state;
		return err;
	}

	return err;
}

static int naga_get_leds(struct razer_mouse *m,
			       struct razer_led **leds_list)
{
	struct naga_private *priv = m->internal;
	struct razer_led *scroll, *logo;

	scroll = zalloc(sizeof(struct razer_led));
	if (!scroll)
		return -ENOMEM;
	logo = zalloc(sizeof(struct razer_led));
	if (!logo) {
		free(scroll);
		return -ENOMEM;
	}

	scroll->name = "Scrollwheel";
	scroll->id = NAGA_LED_SCROLL;
	scroll->state = priv->led_states[NAGA_LED_SCROLL];
	scroll->toggle_state = naga_led_toggle;
	scroll->u.mouse = m;

	logo->name = "GlowingLogo";
	logo->id = NAGA_LED_LOGO;
	logo->state = priv->led_states[NAGA_LED_LOGO];
	logo->toggle_state = naga_led_toggle;
	logo->u.mouse = m;

	/* Link the list */
	*leds_list = scroll;
	scroll->next = logo;
	logo->next = NULL;

	return NAGA_NR_LEDS;
}

static int naga_supported_freqs(struct razer_mouse *m,
				      enum razer_mouse_freq **freq_list)
{
	enum razer_mouse_freq *list;
	const int count = 3;

	list = zalloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	list[0] = RAZER_MOUSE_FREQ_125HZ;
	list[1] = RAZER_MOUSE_FREQ_500HZ;
	list[2] = RAZER_MOUSE_FREQ_1000HZ;

	*freq_list = list;

	return count;
}

static enum razer_mouse_freq naga_get_freq(struct razer_mouse_profile *p)
{
	struct naga_private *priv = p->mouse->internal;

	return priv->frequency;
}

static int naga_set_freq(struct razer_mouse_profile *p,
			       enum razer_mouse_freq freq)
{
	struct naga_private *priv = p->mouse->internal;
	enum razer_mouse_freq old_freq;
	int err;

	if (!priv->claimed)
		return -EBUSY;

	old_freq = priv->frequency;
	priv->frequency = freq;

	err = naga_commit(priv);
	if (err) {
		priv->frequency = old_freq;
		return err;
	}

	return err;
}

static int naga_supported_axes(struct razer_mouse *m,
			       struct razer_axis **axes_list)
{
	struct naga_private *priv = m->internal;

	*axes_list = priv->axes;

	return ARRAY_SIZE(priv->axes);
}

static int naga_supported_resolutions(struct razer_mouse *m,
					    enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	unsigned int i;
	const unsigned int count = NAGA_NR_DPIMAPPINGS;

	list = zalloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;
	for (i = 0; i < count; i++)
		list[i] = (i + 1) * 100;
	*res_list = list;

	return count;
}

static struct razer_mouse_profile * naga_get_profiles(struct razer_mouse *m)
{
	struct naga_private *priv = m->internal;

	return &priv->profile;
}

static struct razer_mouse_profile * naga_get_active_profile(struct razer_mouse *m)
{
	struct naga_private *priv = m->internal;

	return &priv->profile;
}

static int naga_supported_dpimappings(struct razer_mouse *m,
				      struct razer_mouse_dpimapping **res_ptr)
{
	struct naga_private *priv = m->internal;

	*res_ptr = &priv->dpimapping[0];

	return ARRAY_SIZE(priv->dpimapping);
}

static struct razer_mouse_dpimapping * naga_get_dpimapping(struct razer_mouse_profile *p,
							   struct razer_axis *axis)
{
	struct naga_private *priv = p->mouse->internal;

	if (!axis)
		axis = &priv->axes[0];
	if (axis->id == 0)
		return priv->cur_dpimapping_X;
	if (axis->id == 1)
		return priv->cur_dpimapping_Y;

	return NULL;
}

static int naga_set_dpimapping(struct razer_mouse_profile *p,
			       struct razer_axis *axis,
			       struct razer_mouse_dpimapping *d)
{
	struct naga_private *priv = p->mouse->internal;
	struct razer_mouse_dpimapping *oldmapping_X, *oldmapping_Y;
	int err;

	if (!priv->claimed)
		return -EBUSY;
	if (axis && axis->id >= ARRAY_SIZE(priv->axes))
		return -EINVAL;

	oldmapping_X = priv->cur_dpimapping_X;
	oldmapping_Y = priv->cur_dpimapping_Y;

	if (axis) {
		if (axis->id == 0)
			priv->cur_dpimapping_X = d;
		else if (axis->id == 1)
			priv->cur_dpimapping_Y = d;
		else
			return -EINVAL;
	} else {
		priv->cur_dpimapping_X = d;
		priv->cur_dpimapping_Y = d;
	}

	err = naga_commit(priv);
	if (err) {
		priv->cur_dpimapping_X = oldmapping_X;
		priv->cur_dpimapping_Y = oldmapping_Y;
		return err;
	}

	return err;
}

void razer_naga_gen_idstr(struct usb_device *udev, char *buf)
{
	razer_generic_usb_gen_idstr(udev, NULL, "Naga", 1, buf);
}

void razer_naga_assign_usb_device(struct razer_mouse *m,
					struct usb_device *usbdev)
{
	struct naga_private *priv = m->internal;

	priv->usb.dev = usbdev;
}

int razer_naga_init(struct razer_mouse *m,
		    struct usb_device *usbdev)
{
	struct naga_private *priv;
	unsigned int i;
	int fwver, err;

	BUILD_BUG_ON(sizeof(struct naga_command) != 90);

	priv = zalloc(sizeof(struct naga_private));
	if (!priv)
		return -ENOMEM;
	m->internal = priv;

	razer_naga_assign_usb_device(m, usbdev);

	err = naga_claim(m);
	if (err) {
		razer_error("hw_naga: Failed to claim device\n");
		goto err_free;
	}

	/* Fetch firmware version */
	fwver = naga_read_fw_ver(priv);
	if (fwver < 0) {
		err = fwver;
		goto err_release;
	}
	priv->fw_version = fwver;

	priv->frequency = RAZER_MOUSE_FREQ_1000HZ;
	for (i = 0; i < NAGA_NR_LEDS; i++)
		priv->led_states[i] = RAZER_LED_ON;

	priv->profile.nr = 0;
	priv->profile.get_freq = naga_get_freq;
	priv->profile.set_freq = naga_set_freq;
	priv->profile.get_dpimapping = naga_get_dpimapping;
	priv->profile.set_dpimapping = naga_set_dpimapping;
	priv->profile.mouse = m;

	for (i = 0; i < NAGA_NR_DPIMAPPINGS; i++) {
		priv->dpimapping[i].nr = i;
		priv->dpimapping[i].res = (i + 1) * 100;
		if (priv->dpimapping[i].res == 1000) {
			priv->cur_dpimapping_X = &priv->dpimapping[i];
			priv->cur_dpimapping_Y = &priv->dpimapping[i];
		}
		priv->dpimapping[i].change = NULL;
		priv->dpimapping[i].mouse = m;
	}
	for (i = 0; i < NAGA_NR_AXES; i++) {
		priv->axes[i].id = i;
		switch (i) {
		case 0: /* X */
			priv->axes[i].flags |= RAZER_AXIS_INDEPENDENT_DPIMAPPING;
			priv->axes[i].name = "X";
			break;
		case 1: /* Y */
			priv->axes[i].flags |= RAZER_AXIS_INDEPENDENT_DPIMAPPING;
			priv->axes[i].name = "Y";
			break;
		case 2: /* Scrollwheel */
			priv->axes[i].name = "Scroll";
			break;
		}
	}

	m->type = RAZER_MOUSETYPE_NAGA;
	razer_generic_usb_gen_idstr(usbdev, priv->usb.h, "Naga", 1, m->idstr);

	m->claim = naga_claim;
	m->release = naga_release;
	m->get_fw_version = naga_get_fw_version;
	m->get_leds = naga_get_leds;
	m->nr_profiles = 1;
	m->get_profiles = naga_get_profiles;
	m->get_active_profile = naga_get_active_profile;
	m->supported_axes = naga_supported_axes;
	m->supported_resolutions = naga_supported_resolutions;
	m->supported_freqs = naga_supported_freqs;
	m->supported_dpimappings = naga_supported_dpimappings;

	err = naga_commit(priv);
	if (err) {
		razer_error("hw_naga: Failed to commit initial settings\n");
		goto err_release;
	}

	naga_release(m);

	return 0;

err_release:
	naga_release(m);
err_free:
	free(priv);
	return err;
}

void razer_naga_release(struct razer_mouse *m)
{
	struct naga_private *priv = m->internal;

	while (priv->claimed)
		naga_release(m);
	free(priv);
}
