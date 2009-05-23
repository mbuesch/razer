/*
 *   Lowlevel hardware access for the
 *   Razer Deathadder mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering and
 *   hardware documentation provided under NDA.
 *
 *   Copyright (C) 2007-2008 Michael Buesch <mb@bu3sch.de>
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

/* This is the device configuration structure sent to the device
 * for firmware version >= 1.25 */
struct deathadder_125_cfg {
	uint8_t freq;
	uint8_t res;
	uint8_t profile;
	uint8_t leds;
} __attribute__((packed));

struct deathadder_private {
	bool claimed;
	/* Firmware version number. */
	uint16_t fw_version;
	struct razer_usb_context usb;
	/* The currently set LED states. */
	bool led_states[DEATHADDER_NR_LEDS];
	/* The currently set frequency. */
	enum razer_mouse_freq frequency;
	/* Previous freq. For predicting reconnect events only. */
	enum razer_mouse_freq old_frequency;
	/* The currently set resolution. */
	enum razer_mouse_res resolution;
};

#define DEATHADDER_USB_TIMEOUT		3000
#define DADD_FW(major, minor)		(((major) << 8) | (minor))
#define DEATHADDER_FW_IMAGE_SIZE	0x200000//FIXME

static int deathadder_usb_write(struct deathadder_private *priv,
				int request, int command,
				const void *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      (char *)buf, size,
			      DEATHADDER_USB_TIMEOUT);
	if (err != size) {
		fprintf(stderr, "razer-deathadder: "
			"USB write 0x%02X 0x%02X failed: %d\n",
			request, command, err);
		return err;
	}
	return 0;
}

static int deathadder_usb_read(struct deathadder_private *priv,
			       int request, int command,
			       void *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      DEATHADDER_USB_TIMEOUT);
	if (err != size) {
		fprintf(stderr, "razer-deathadder: "
			"USB read 0x%02X 0x%02X failed: %d\n",
			request, command, err);
		return err;
	}
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

static int deathadder_commit(struct deathadder_private *priv)
{
	struct razer_usb_reconnect_guard guard;
	int i, err;

	err = razer_usb_reconnect_guard_init(&guard, &priv->usb);
	if (err)
		return err;

	if (priv->fw_version < DADD_FW(1,25)) {
		char value, freq_value, res_value;

		/* Translate frequency setting. */
		switch (priv->frequency) {
		case RAZER_MOUSE_FREQ_125HZ:
			freq_value = 3;
			break;
		case RAZER_MOUSE_FREQ_500HZ:
			freq_value = 2;
			break;
		case RAZER_MOUSE_FREQ_1000HZ:
		case RAZER_MOUSE_FREQ_UNKNOWN:
			freq_value = 1;
			break;
		default:
			return -EINVAL;
		}

		/* Translate resolution setting. */
		switch (priv->resolution) {
		case RAZER_MOUSE_RES_450DPI:
			res_value = 3;
			break;
		case RAZER_MOUSE_RES_900DPI:
			res_value = 2;
			break;
		case RAZER_MOUSE_RES_1800DPI:
		case RAZER_MOUSE_RES_UNKNOWN:
			res_value = 1;
			break;
		default:
			return -EINVAL;
		}

		if (priv->old_frequency != priv->frequency) {
			/* Commit frequency setting. */
			err = deathadder_usb_write(priv, USB_REQ_SET_CONFIGURATION,
						   0x07, &freq_value, sizeof(freq_value));
			if (err)
				return err;

			/* The frequency setting changed. The device firmware
			 * will reboot the mouse now. This will cause a reconnect
			 * on the USB bus. Call the guard... */
			err = razer_usb_reconnect_guard_wait(&guard, 0);
			if (err)
				return err;
			/* The device needs a bit of punching in the face after reconnect. */
			for (i = 0; i < 5; i++) {
				int ver = deathadder_read_fw_ver(priv);
				if ((ver > 0) && ((ver & 0xFFFF) == priv->fw_version))
					break;
				razer_msleep(100);
			}
			if (i >= 5) {
				fprintf(stderr, "razer-deathadder: The device didn't wake up "
					"after a frequency change. Try to replug it.\n");
			}
		}

		/* Commit LED states. */
		value = 0;
		if (priv->led_states[DEATHADDER_LED_LOGO])
			value |= 0x01;
		if (priv->led_states[DEATHADDER_LED_SCROLL])
			value |= 0x02;
		err = deathadder_usb_write(priv, USB_REQ_SET_CONFIGURATION,
					   0x06, &value, sizeof(value));
		if (err)
			return err;

		/* Commit resolution setting. */
		err = deathadder_usb_write(priv, USB_REQ_SET_CONFIGURATION,
					   0x09, &res_value, sizeof(res_value));
		if (err)
			return err;
	} else {
		struct deathadder_125_cfg config = { 0, };

		/* Translate frequency setting. */
		switch (priv->frequency) {
		case RAZER_MOUSE_FREQ_125HZ:
			config.freq = 3;
			break;
		case RAZER_MOUSE_FREQ_500HZ:
			config.freq = 2;
			break;
		case RAZER_MOUSE_FREQ_1000HZ:
		case RAZER_MOUSE_FREQ_UNKNOWN:
			config.freq = 1;
			break;
		default:
			return -EINVAL;
		}

		/* Translate resolution setting. */
		switch (priv->resolution) {
		case RAZER_MOUSE_RES_450DPI:
			config.res = 3;
			break;
		case RAZER_MOUSE_RES_900DPI:
			config.res = 2;
			break;
		case RAZER_MOUSE_RES_1800DPI:
		case RAZER_MOUSE_RES_UNKNOWN:
			config.res = 1;
			break;
		default:
			return -EINVAL;
		}

		/* Translate the profile ID. */
		//TODO
		config.profile = 1;

		/* Translate the LED states. */
		if (priv->led_states[DEATHADDER_LED_LOGO])
			config.leds |= 0x01;
		if (priv->led_states[DEATHADDER_LED_SCROLL])
			config.leds |= 0x02;


		/* Commit the settings. */
		err = deathadder_usb_write(priv, USB_REQ_SET_CONFIGURATION,
					   0x10, &config, sizeof(config));
		if (err)
			return err;

		if (priv->frequency != priv->old_frequency) {
			/* The frequency setting changed. The device firmware
			 * will reboot the mouse now. This will cause a reconnect
			 * on the USB bus. Call the guard... */
			err = razer_usb_reconnect_guard_wait(&guard, 0);
			if (err)
				return err;
			/* The device has reconnected, so write the config
			 * another time to ensure all settings are active.
			 */
			err = deathadder_usb_write(priv, USB_REQ_SET_CONFIGURATION,
						   0x10, &config, sizeof(config));
			if (err)
				return err;
		}

		/* The device needs a bit of punching in the face.
		 * Ensure it properly responds to read accesses. */
		for (i = 0; i < 5; i++) {
			int ver = deathadder_read_fw_ver(priv);
			if ((ver > 0) && ((ver & 0xFFFF) == priv->fw_version))
				break;
			razer_msleep(100);
		}
		if (i >= 5) {
			fprintf(stderr, "razer-deathadder: The device didn't wake up "
				"after a config change. Try to replug it.\n");
		}
	}

	return 0;
}

static int deathadder_claim(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;
	int err, fwver;

	err = razer_generic_usb_claim(&priv->usb);
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

	razer_generic_usb_release(&priv->usb);
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
	int err;
	enum razer_led_state old_state;

	if (led->id >= DEATHADDER_NR_LEDS)
		return -EINVAL;
	if ((new_state != RAZER_LED_OFF) &&
	    (new_state != RAZER_LED_ON))
		return -EINVAL;

	if (!priv->claimed)
		return -EBUSY;

	old_state = priv->led_states[led->id];
	priv->led_states[led->id] = new_state;

	err = deathadder_commit(priv);
	if (err) {
		priv->led_states[led->id] = old_state;
		return err;
	}

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
	scroll->state = priv->led_states[DEATHADDER_LED_SCROLL];
	scroll->toggle_state = deathadder_led_toggle;
	scroll->u.mouse = m;

	logo->name = "GlowingLogo";
	logo->id = DEATHADDER_LED_LOGO;
	logo->state = priv->led_states[DEATHADDER_LED_LOGO];
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
	enum razer_mouse_freq old_freq;
	int err;

	if (!priv->claimed)
		return -EBUSY;

	old_freq = priv->frequency;
	priv->old_frequency = old_freq;
	priv->frequency = freq;

	err = deathadder_commit(priv);
	if (err) {
		priv->frequency = old_freq;
		return err;
	}
	priv->old_frequency = freq;

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
	enum razer_mouse_res old_res;
	int err;

	if (!priv->claimed)
		return -EBUSY;

	old_res = priv->resolution;
	priv->resolution = res;

	err = deathadder_commit(priv);
	if (err) {
		priv->resolution = res;
		return err;
	}

	return err;
}

#if 0
static struct usb_device * wait_for_usbdev(uint16_t vendor_id,
					   uint16_t product_id)
{
	struct usb_bus *bus, *buslist;
	struct usb_device *dev;
	unsigned int i;

	for (i = 0; i < 100; i++) {
		usb_find_busses();
		usb_find_devices();

		buslist = usb_get_busses();
		for_each_usbbus(bus, buslist) {
			for_each_usbdev(dev, bus->devices) {
				if (dev->descriptor.idVendor == vendor_id &&
				    dev->descriptor.idProduct == product_id)
					return dev;
			}
		}
		razer_msleep(100);
	}

	return NULL;
}
#endif

static int deathadder_flash_firmware(struct razer_mouse *m,
				     const char *data, size_t len,
				     unsigned int magic_number)
{
#if 0
	struct deathadder_private *priv = m->internal;
	int err;
	uint16_t checksum, expected_checksum;
	unsigned int i;
	char value;
	struct usb_device *cydev;
	usb_dev_handle *cyh;
	unsigned int interface;

	if (magic_number != RAZER_FW_FLASH_MAGIC)
		return -EINVAL;
	if (!priv->claimed)
		return -EBUSY;

	/* Firmware needs to be image plus 2 bytes checksum. */
//	if (len != DEATHADDER_FW_IMAGE_SIZE + 2) {
	if (len != 16*1024) {
		fprintf(stderr, "razer-deathadder: "
			"Firmware image has wrong size %u (expected %u).\n",
			(unsigned int)len,
			(unsigned int)(DEATHADDER_FW_IMAGE_SIZE + 2));
		return -EINVAL;
	}
#if 0
	/* Verify the checksum. */
	checksum = 0;
	for (i = 0; i < DEATHADDER_FW_IMAGE_SIZE; i += 2) {
		checksum ^= (((uint16_t)(data[i + 0])) << 8) |
			     ((uint16_t)(data[i + 1]));
	}
	expected_checksum = (((uint16_t)(data[DEATHADDER_FW_IMAGE_SIZE + 0])) << 8) |
			     ((uint16_t)(data[DEATHADDER_FW_IMAGE_SIZE + 1]));
	if (checksum != expected_checksum) {
		fprintf(stderr, "razer-deathadder: "
			"Firmware image has invalid checksum. "
			"(was: 0x%02X, expected: 0x%02X)\n",
			checksum, expected_checksum);
		return -EINVAL;
	}
#endif

	/* Enter bootloader mode */
	razer_msleep(50);
	value = 0;
	err = deathadder_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				   0x08, &value, sizeof(value));
	if (err) {
		fprintf(stderr, "razer-deathadder: Failed to enter the bootloader.\n");
		return err;
	}
	/* Wait for the cypress device to appear. */
	cydev = wait_for_usbdev(0x04B4, 0xE006);
	if (!cydev) {
		fprintf(stderr, "razer-deathadder: Cypress device didn't appear.\n");
		return -1;
	}
	razer_msleep(100);
printf("Found cypress dev\n");
	cyh = usb_open(cydev);
	if (!cyh) {
		fprintf(stderr, "razer-deathadder: Failed to open Cypress device.\n");
		return -1;
	}
	interface = cydev->config->interface->altsetting[0].bInterfaceNumber;
#if 0
	err = usb_detach_kernel_driver_np(cyh, interface);
	if (err) {
		fprintf(stderr, "razer-deathadder: "
			"failed to detach kernel driver for Cypress device.\n");
		//TODO abort?
	}
#endif
	err = usb_claim_interface(cyh, interface);
	if (err) {
		fprintf(stderr, "razer-deathadder: Failed to claim Cypress device.\n");
		usb_close(cyh);
		return -1;
	}
printf("cypress claimed\n");

	//TODO
	usb_close(cyh);
	razer_msleep(300);

	return err;
#endif
	return -ENOSYS;
}

void razer_deathadder_gen_idstr(struct usb_device *udev, char *buf)
{
	char devid[64];

	/* We can't include the USB device number, because that changes on the
	 * automatic reconnects the device firmware does.
	 * The serial number is zero, so that's not very useful, too.
	 * Basically, that means we have a pretty bad ID string due to
	 * major design faults in the hardware. :(
	 */
	snprintf(devid, sizeof(devid), "%04X-%04X-%02X",
		 udev->descriptor.idVendor,
		 udev->descriptor.idProduct,
		 udev->descriptor.iSerialNumber);
	razer_create_idstr(buf, BUSTYPESTR_USB, udev->bus->dirname,
			   DEVTYPESTR_MOUSE, "DeathAdder", devid);
}

void razer_deathadder_assign_usb_device(struct razer_mouse *m,
					struct usb_device *usbdev)
{
	struct deathadder_private *priv = m->internal;

	priv->usb.dev = usbdev;
}

int razer_deathadder_init_struct(struct razer_mouse *m,
				 struct usb_device *usbdev)
{
	struct deathadder_private *priv;
	unsigned int i;
	int err;

	priv = malloc(sizeof(struct deathadder_private));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));
	m->internal = priv;

	razer_deathadder_assign_usb_device(m, usbdev);

	err = razer_usb_force_reinit(&priv->usb);
	if (err) {
		fprintf(stderr, "hw_deathadder: Failed to reinit USB device\n");
		free(priv);
		return err;
	}

	priv->frequency = RAZER_MOUSE_FREQ_1000HZ;
	priv->old_frequency = priv->frequency;
	priv->resolution = RAZER_MOUSE_RES_1800DPI;
	for (i = 0; i < DEATHADDER_NR_LEDS; i++)
		priv->led_states[i] = RAZER_LED_ON;

	m->type = RAZER_MOUSETYPE_DEATHADDER;
	razer_deathadder_gen_idstr(usbdev, m->idstr);

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
	m->flash_firmware = deathadder_flash_firmware;

	return 0;
}

void razer_deathadder_release(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->internal;

	deathadder_release(m);
	free(priv);
}
