/*
 *   Lowlevel hardware access for the
 *   Razer Taipan mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering, only.
 *
 *   Copyright (C) 2007-2010 Michael Buesch <m@bues.ch>
 *   Driver modified for Taipan by Tibor Peluch <messani@gmail.com>
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

#include "hw_taipan.h"
#include "razer_private.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


enum {
	TAIPAN_LED_SCROLL = 0,
	TAIPAN_LED_LOGO,
	TAIPAN_NR_LEDS,
};

enum { /* Misc constants */
	TAIPAN_NR_DPIMAPPINGS	= 82,
	TAIPAN_NR_AXES		= 3,
};

struct taipan_command {
	uint8_t status;
	uint8_t padding0[4];
	be16_t command;
	be16_t request;
	be16_t value0;
	be16_t value1;
	uint8_t padding1[75];
	uint8_t checksum;
	uint8_t padding2;
} _packed;

struct taipan_private {
	struct razer_mouse *m;

	/* Firmware version number. */
	uint16_t fw_version;
	/* The currently set LED states. */
	bool led_states[TAIPAN_NR_LEDS];
	/* The currently set frequency. */
	enum razer_mouse_freq frequency;
	/* The currently set resolution. */
	struct razer_mouse_dpimapping *cur_dpimapping_X;
	struct razer_mouse_dpimapping *cur_dpimapping_Y;

	struct razer_mouse_profile profile;
	struct razer_mouse_dpimapping dpimapping[TAIPAN_NR_DPIMAPPINGS];
	struct razer_axis axes[TAIPAN_NR_AXES];

	bool commit_pending;
};


static void taipan_command_init(struct taipan_command *cmd)
{
	memset(cmd, 0, sizeof(*cmd));
}

static int taipan_usb_write(struct taipan_private *priv,
			    int request, int command,
			    const void *buf, size_t size)
{
	int err;

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, 0,
		(unsigned char *)buf, size,
		RAZER_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-taipan: "
			"USB write 0x%02X 0x%02X failed: %d\n",
			request, command, err);
		return err;
	}
	return 0;
}

static int taipan_usb_read(struct taipan_private *priv,
			   int request, int command,
			   void *buf, size_t size)
{
	int err;

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, 0,
		buf, size,
		RAZER_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-taipan: "
			"USB read 0x%02X 0x%02X failed: %d\n",
			request, command, err);
		return err;
	}
	return 0;
}

static int taipan_send_command(struct taipan_private *priv,
			       struct taipan_command *cmd)
{
	int err;

	cmd->checksum = razer_xor8_checksum((uint8_t *)cmd + 2, sizeof(*cmd) - 4);
	err = taipan_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION, 0x300,
			       cmd, sizeof(*cmd));
	if (err)
		return err;
	err = taipan_usb_read(priv, LIBUSB_REQUEST_CLEAR_FEATURE, 0x300,
			      cmd, sizeof(*cmd));
	if (err)
		return err;
	if (cmd->status != 2 &&
	    cmd->status != 1 &&
	    cmd->status != 0) {
		razer_error("razer-taipan: Command %04X/%04X failed with %02X\n",
			    be16_to_cpu(cmd->command),
			    be16_to_cpu(cmd->request),
			    cmd->status);
	}

	return 0;
}

static int taipan_read_fw_ver(struct taipan_private *priv)
{
	struct taipan_command cmd;
	uint16_t ver;
	int err;
	unsigned int i;

	/* Poke the device several times until it responds with a
	 * valid version number */
	for (i = 0; i < 5; i++) {
		taipan_command_init(&cmd);
		cmd.command = cpu_to_be16(0x0200);
		cmd.request = cpu_to_be16(0x8100);
		err = taipan_send_command(priv, &cmd);
		ver = be16_to_cpu(cmd.value0);
		if (!err && (ver & 0xFF00) != 0)
			return ver;
		razer_msleep(100);
	}
	razer_error("razer-taipan: Failed to read firmware version\n");

	/* FIXME: Ignore the error and return 0 until we find out
	 *        why some mice fail to return a valid version number.
	 */
	return 0;
}

static int taipan_do_commit(struct taipan_private *priv)
{
	struct taipan_command cmd;
	unsigned int xres, yres;
	uint16_t freq;
	int err;

	/* Set the resolution. */
	taipan_command_init(&cmd);
	cmd.command = cpu_to_be16(0x0704);
	cmd.request = cpu_to_be16(0x0500);
	xres = (unsigned int)priv->cur_dpimapping_X->res[RAZER_DIM_0];
	yres = (unsigned int)priv->cur_dpimapping_Y->res[RAZER_DIM_0];
	cmd.value0 = cpu_to_be16(xres);
	cmd.value1 = cpu_to_be16(yres);
	err = taipan_send_command(priv, &cmd);
	if (err)
		return err;

	/* Set the scroll wheel and buttons LEDs. */
	taipan_command_init(&cmd);
	cmd.command = cpu_to_be16(0x0303);
	cmd.request = cpu_to_be16(0x0001);
	cmd.value0 = cpu_to_be16(0x0100);
	if (priv->led_states[TAIPAN_LED_SCROLL])
		cmd.value0 |= cpu_to_be16(0x0001);
	err = taipan_send_command(priv, &cmd);
	if (err)
		return err;

	/* Set the logo LED. */
	taipan_command_init(&cmd);
	cmd.command = cpu_to_be16(0x0303);
	cmd.request = cpu_to_be16(0x0001);
	cmd.value0 = cpu_to_be16(0x0400);
	if (priv->led_states[TAIPAN_LED_LOGO])
		cmd.value0 |= cpu_to_be16(0x0001);
	err = taipan_send_command(priv, &cmd);
	if (err)
		return err;

	/* Set scan frequency. */
	switch (priv->frequency) {
	case RAZER_MOUSE_FREQ_125HZ:
		freq = 0x0008;
		break;
	case RAZER_MOUSE_FREQ_500HZ:
		freq = 0x0002;
		break;
	case RAZER_MOUSE_FREQ_1000HZ:
	case RAZER_MOUSE_FREQ_UNKNOWN:
		freq = 0x0001;
		break;
	default:
		return -EINVAL;
	}
	taipan_command_init(&cmd);
	cmd.command = cpu_to_be16(0x0100);
	cmd.request = cpu_to_be16(0x0500);
	cmd.request |= cpu_to_be16(freq);
	err = taipan_send_command(priv, &cmd);
	if (err)
		return err;

	return 0;
}

static int taipan_get_fw_version(struct razer_mouse *m)
{
	struct taipan_private *priv = m->drv_data;

	return priv->fw_version;
}

static int taipan_commit(struct razer_mouse *m, int force)
{
	struct taipan_private *priv = m->drv_data;
	int err = 0;

	if (!m->claim_count)
		return -EBUSY;
	if (priv->commit_pending || force) {
		err = taipan_do_commit(priv);
		if (!err)
			priv->commit_pending = 0;
	}

	return err;
}

static int taipan_led_toggle(struct razer_led *led,
			     enum razer_led_state new_state)
{
	struct razer_mouse *m = led->u.mouse;
	struct taipan_private *priv = m->drv_data;

	if (led->id >= TAIPAN_NR_LEDS)
		return -EINVAL;
	if ((new_state != RAZER_LED_OFF) &&
	    (new_state != RAZER_LED_ON))
		return -EINVAL;

	if (!priv->m->claim_count)
		return -EBUSY;

	priv->led_states[led->id] = new_state;
	priv->commit_pending = 1;

	return 0;
}

static int taipan_get_leds(struct razer_mouse *m,
			   struct razer_led **leds_list)
{
	struct taipan_private *priv = m->drv_data;
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
	scroll->id = TAIPAN_LED_SCROLL;
	scroll->state = priv->led_states[TAIPAN_LED_SCROLL];
	scroll->toggle_state = taipan_led_toggle;
	scroll->u.mouse = m;

	logo->name = "GlowingLogo";
	logo->id = TAIPAN_LED_LOGO;
	logo->state = priv->led_states[TAIPAN_LED_LOGO];
	logo->toggle_state = taipan_led_toggle;
	logo->u.mouse = m;

	/* Link the list */
	*leds_list = scroll;
	scroll->next = logo;
	logo->next = NULL;

	return TAIPAN_NR_LEDS;
}

static int taipan_supported_freqs(struct razer_mouse *m,
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

static enum razer_mouse_freq taipan_get_freq(struct razer_mouse_profile *p)
{
	struct taipan_private *priv = p->mouse->drv_data;

	return priv->frequency;
}

static int taipan_set_freq(struct razer_mouse_profile *p,
			   enum razer_mouse_freq freq)
{
	struct taipan_private *priv = p->mouse->drv_data;

	if (!priv->m->claim_count)
		return -EBUSY;

	priv->frequency = freq;
	priv->commit_pending = 1;

	return 0;
}

static int taipan_supported_axes(struct razer_mouse *m,
				 struct razer_axis **axes_list)
{
	struct taipan_private *priv = m->drv_data;

	*axes_list = priv->axes;

	return ARRAY_SIZE(priv->axes);
}

static int taipan_supported_resolutions(struct razer_mouse *m,
					enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	unsigned int i;
	const unsigned int count = TAIPAN_NR_DPIMAPPINGS;

	list = zalloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;
	for (i = 0; i < count; i++)
		list[i] = (i + 1) * 100;
	*res_list = list;

	return count;
}

static struct razer_mouse_profile * taipan_get_profiles(struct razer_mouse *m)
{
	struct taipan_private *priv = m->drv_data;

	return &priv->profile;
}

static int taipan_supported_dpimappings(struct razer_mouse *m,
					struct razer_mouse_dpimapping **res_ptr)
{
	struct taipan_private *priv = m->drv_data;

	*res_ptr = &priv->dpimapping[0];

	return ARRAY_SIZE(priv->dpimapping);
}

static struct razer_mouse_dpimapping * taipan_get_dpimapping(struct razer_mouse_profile *p,
							     struct razer_axis *axis)
{
	struct taipan_private *priv = p->mouse->drv_data;

	if (!axis)
		axis = &priv->axes[0];
	if (axis->id == 0)
		return priv->cur_dpimapping_X;
	if (axis->id == 1)
		return priv->cur_dpimapping_Y;

	return NULL;
}

static int taipan_set_dpimapping(struct razer_mouse_profile *p,
				 struct razer_axis *axis,
				 struct razer_mouse_dpimapping *d)
{
	struct taipan_private *priv = p->mouse->drv_data;

	if (!priv->m->claim_count)
		return -EBUSY;
	if (axis && axis->id >= ARRAY_SIZE(priv->axes))
		return -EINVAL;

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
	priv->commit_pending = 1;

	return 0;
}

int razer_taipan_init(struct razer_mouse *m,
		      struct libusb_device *usbdev)
{
	struct taipan_private *priv;
	unsigned int i;
	int fwver, err;

	BUILD_BUG_ON(sizeof(struct taipan_command) != 90);

	priv = zalloc(sizeof(struct taipan_private));
	if (!priv)
		return -ENOMEM;
	priv->m = m;
	m->drv_data = priv;

	err = razer_usb_add_used_interface(m->usb_ctx, 0, 0);
	if (err)
		goto err_free;

	err = m->claim(m);
	if (err) {
		razer_error("hw_taipan: Failed to claim device\n");
		goto err_free;
	}

	/* Fetch firmware version */
	fwver = taipan_read_fw_ver(priv);
	if (fwver < 0) {
		err = fwver;
		goto err_release;
	}
	priv->fw_version = fwver;

	priv->frequency = RAZER_MOUSE_FREQ_1000HZ;
	for (i = 0; i < TAIPAN_NR_LEDS; i++)
		priv->led_states[i] = RAZER_LED_ON;

	priv->profile.nr = 0;
	priv->profile.get_freq = taipan_get_freq;
	priv->profile.set_freq = taipan_set_freq;
	priv->profile.get_dpimapping = taipan_get_dpimapping;
	priv->profile.set_dpimapping = taipan_set_dpimapping;
	priv->profile.mouse = m;

	for (i = 0; i < TAIPAN_NR_DPIMAPPINGS; i++) {
		priv->dpimapping[i].nr = i;
		priv->dpimapping[i].res[RAZER_DIM_0] = (i + 1) * 100;
		if (priv->dpimapping[i].res[RAZER_DIM_0] == 1000) {
			priv->cur_dpimapping_X = &priv->dpimapping[i];
			priv->cur_dpimapping_Y = &priv->dpimapping[i];
		}
		priv->dpimapping[i].dimension_mask = (1 << RAZER_DIM_0);
		priv->dpimapping[i].change = NULL;
		priv->dpimapping[i].mouse = m;
	}
	razer_init_axes(&priv->axes[0],
			"X", RAZER_AXIS_INDEPENDENT_DPIMAPPING,
			"Y", RAZER_AXIS_INDEPENDENT_DPIMAPPING,
			"Scroll", 0);

	m->type = RAZER_MOUSETYPE_TAIPAN;
	razer_generic_usb_gen_idstr(usbdev, m->usb_ctx->h, "Taipan", 1,
				    NULL, m->idstr);

	m->get_fw_version = taipan_get_fw_version;
	m->commit = taipan_commit;
	m->global_get_leds = taipan_get_leds;
	m->get_profiles = taipan_get_profiles;
	m->supported_axes = taipan_supported_axes;
	m->supported_resolutions = taipan_supported_resolutions;
	m->supported_freqs = taipan_supported_freqs;
	m->supported_dpimappings = taipan_supported_dpimappings;

	err = taipan_do_commit(priv);
	if (err) {
		razer_error("hw_taipan: Failed to commit initial settings\n");
		goto err_release;
	}

	m->release(m);

	return 0;

err_release:
	m->release(m);
err_free:
	free(priv);
	return err;
}

void razer_taipan_release(struct razer_mouse *m)
{
	struct taipan_private *priv = m->drv_data;

	free(priv);
}
