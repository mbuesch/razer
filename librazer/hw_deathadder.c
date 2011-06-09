/*
 *   Lowlevel hardware access for the
 *   Razer Deathadder mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering and
 *   hardware documentation provided under NDA.
 *
 *   Copyright (C) 2007-2011 Michael Buesch <mb@bu3sch.de>
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
#include "cypress_bootloader.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


enum deathadder_type {
	DEATHADDER_CLASSIC,	/* DeathAdder Classic */
	DEATHADDER_3500,	/* DeathAdder 3500DPI */
	DEATHADDER_BLACK,	/* DeathAdder Black Edition */
};

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
} _packed;

struct deathadder_private {
	struct razer_mouse *m;

	/* The deathadder hardware revision type */
	enum deathadder_type type;

	bool in_bootloader;

	/* Firmware version number. */
	uint16_t fw_version;
	/* The currently set LED states. */
	bool led_states[DEATHADDER_NR_LEDS];
	/* The currently set frequency. */
	enum razer_mouse_freq frequency;
	/* Previous freq. For predicting reconnect events only. */
	enum razer_mouse_freq old_frequency;
	/* The currently set resolution. */
	struct razer_mouse_dpimapping *cur_dpimapping;

	struct razer_mouse_profile profile;
	struct razer_mouse_dpimapping dpimapping[4];

	struct razer_event_spacing commit_spacing;
};

#define DADD_FW(major, minor)		(((major) << 8) | (minor))
#define DEATHADDER_FW_IMAGE_SIZE	0x4000

static int deathadder_usb_write(struct deathadder_private *priv,
				int request, int command,
				const void *buf, size_t size)
{
	int err;

	if (priv->in_bootloader) {
		/* Deathadder firmware is down and we're in the bootloader. */
		return 0;
	}

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, 0,
		(unsigned char *)buf, size,
		RAZER_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-deathadder: "
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

	if (priv->in_bootloader) {
		/* Deathadder firmware is down and we're in the bootloader. */
		memset(buf, 0, size);
		return 0;
	}

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, 0,
		buf, size,
		RAZER_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-deathadder: "
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

	err = deathadder_usb_read(priv, LIBUSB_REQUEST_CLEAR_FEATURE,
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

	if (priv->in_bootloader)
		return 0;

	razer_event_spacing_enter(&priv->commit_spacing);

	err = razer_usb_reconnect_guard_init(&guard, priv->m->usb_ctx);
	if (err)
		goto out;

	if (priv->type == DEATHADDER_CLASSIC &&
	    priv->fw_version < DADD_FW(1,25)) {
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
			err = -EINVAL;
			goto out;
		}

		/* Translate resolution setting. */
		switch (priv->cur_dpimapping->res) {
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
			err = -EINVAL;
			goto out;
		}

		if (priv->old_frequency != priv->frequency) {
			/* Commit frequency setting. */
			err = deathadder_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
						   0x07, &freq_value, sizeof(freq_value));
			if (err)
				goto out;

			/* The frequency setting changed. The device firmware
			 * will reboot the mouse now. This will cause a reconnect
			 * on the USB bus. Call the guard... */
			err = razer_usb_reconnect_guard_wait(&guard, 0);
			if (err)
				goto out;
			/* The device needs a bit of punching in the face after reconnect. */
			for (i = 0; i < 5; i++) {
				int ver = deathadder_read_fw_ver(priv);
				if ((ver > 0) && ((ver & 0xFFFF) == priv->fw_version))
					break;
				razer_msleep(100);
			}
			if (i >= 5) {
				razer_error("razer-deathadder: The device didn't wake up "
					"after a frequency change. Try to replug it.\n");
			}
		}

		/* Commit LED states. */
		value = 0;
		if (priv->led_states[DEATHADDER_LED_LOGO])
			value |= 0x01;
		if (priv->led_states[DEATHADDER_LED_SCROLL])
			value |= 0x02;
		err = deathadder_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
					   0x06, &value, sizeof(value));
		if (err)
			goto out;

		/* Commit resolution setting. */
		err = deathadder_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
					   0x09, &res_value, sizeof(res_value));
		if (err)
			goto out;
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
			err = -EINVAL;
			goto out;
		}

		/* Translate resolution setting. */
		if (priv->type == DEATHADDER_CLASSIC) {
			switch (priv->cur_dpimapping->res) {
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
				err = -EINVAL;
				goto out;
			}
		} else {
			switch (priv->cur_dpimapping->res) {
			case RAZER_MOUSE_RES_450DPI:
				config.res = 4;
				break;
			case RAZER_MOUSE_RES_900DPI:
				config.res = 3;
				break;
			case RAZER_MOUSE_RES_1800DPI:
				config.res = 2;
				break;
			case RAZER_MOUSE_RES_3500DPI:
			case RAZER_MOUSE_RES_UNKNOWN:
				config.res = 1;
				break;
			default:
				err = -EINVAL;
				goto out;
			}
		}

		/* The profile ID. */
		config.profile = 1;

		/* Translate the LED states. */
		if (priv->type == DEATHADDER_BLACK) {
			/* There are no LEDs.
			 * Bit 0 and 1 are always set, though. */
			config.leds = 0x03;
		} else {
			if (priv->led_states[DEATHADDER_LED_LOGO])
				config.leds |= 0x01;
			if (priv->led_states[DEATHADDER_LED_SCROLL])
				config.leds |= 0x02;
		}


		/* Commit the settings. */
		err = deathadder_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
					   0x10, &config, sizeof(config));
		if (err)
			goto out;

		if ((priv->type == DEATHADDER_CLASSIC ||
		     priv->type == DEATHADDER_3500) &&
		    priv->frequency != priv->old_frequency) {
			/* The frequency setting changed. The device firmware
			 * will reboot the mouse now. This will cause a reconnect
			 * on the USB bus. Call the guard... */
			err = razer_usb_reconnect_guard_wait(&guard, 0);
			if (err)
				goto out;
			/* The device has reconnected, so write the config
			 * another time to ensure all settings are active.
			 */
			err = deathadder_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
						   0x10, &config, sizeof(config));
			if (err)
				goto out;
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
			razer_error("razer-deathadder: The device didn't wake up "
				"after a config change. Try to replug it.\n");
		}
	}
	err = 0;
out:
	razer_event_spacing_leave(&priv->commit_spacing);

	return err;
}

static int deathadder_get_fw_version(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->drv_data;

	return priv->fw_version;
}

static int deathadder_led_toggle(struct razer_led *led,
				 enum razer_led_state new_state)
{
	struct razer_mouse *m = led->u.mouse;
	struct deathadder_private *priv = m->drv_data;
	int err;
	enum razer_led_state old_state;

	if (led->id >= DEATHADDER_NR_LEDS)
		return -EINVAL;
	if ((new_state != RAZER_LED_OFF) &&
	    (new_state != RAZER_LED_ON))
		return -EINVAL;

	if (priv->type == DEATHADDER_BLACK)
		return -ENODEV;
	if (!m->claim_count)
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
	struct deathadder_private *priv = m->drv_data;
	struct razer_led *scroll, *logo;

	if (priv->type == DEATHADDER_BLACK)
		return 0; /* No LEDs */

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

static enum razer_mouse_freq deathadder_get_freq(struct razer_mouse_profile *p)
{
	struct deathadder_private *priv = p->mouse->drv_data;

	return priv->frequency;
}

static int deathadder_set_freq(struct razer_mouse_profile *p,
			       enum razer_mouse_freq freq)
{
	struct deathadder_private *priv = p->mouse->drv_data;
	enum razer_mouse_freq old_freq;
	int err;

	if (!priv->m->claim_count)
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
	struct deathadder_private *priv = m->drv_data;
	enum razer_mouse_res *list;
	const int count = (priv->type == DEATHADDER_CLASSIC) ? 3 : 4;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	list[0] = RAZER_MOUSE_RES_450DPI;
	list[1] = RAZER_MOUSE_RES_900DPI;
	list[2] = RAZER_MOUSE_RES_1800DPI;
	if (priv->type != DEATHADDER_CLASSIC)
		list[3] = RAZER_MOUSE_RES_3500DPI;

	*res_list = list;

	return count;
}

//TODO
#if 0
static struct usb_device * wait_for_usbdev(struct usb_device *dev,
					   uint16_t vendor_id,
					   uint16_t product_id)
{
	struct usb_bus *bus, *buslist;
	unsigned int i;

	if (dev->descriptor.idVendor == vendor_id &&
	    dev->descriptor.idProduct == product_id)
		return dev;

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
	struct deathadder_private *priv = m->drv_data;
	int err;
	char value;
	struct libusb_device *cydev;
	struct cypress cy;

	if (magic_number != RAZER_FW_FLASH_MAGIC)
		return -EINVAL;
	if (!m->claim_count)
		return -EBUSY;

	if (len != DEATHADDER_FW_IMAGE_SIZE) {
		razer_error("razer-deathadder: "
			"Firmware image has wrong size %u (expected %u).\n",
			(unsigned int)len,
			(unsigned int)DEATHADDER_FW_IMAGE_SIZE);
		return -EINVAL;
	}

	razer_msleep(50);
	if (priv->in_bootloader) {
		/* We're already inside of the bootloader */
		cydev = m->usb_ctx->dev;
	} else {
		/* Enter bootloader mode */
		value = 0;
		err = deathadder_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
					   0x08, &value, sizeof(value));
		if (err) {
			razer_error("razer-deathadder: Failed to enter the bootloader.\n");
			return err;
		}
		/* Wait for the cypress device to appear. */
//TODO	cydev = wait_for_usbdev(priv->usb.dev, CYPRESS_BOOT_VENDORID, CYPRESS_BOOT_PRODUCTID);
cydev=NULL;
		if (!cydev) {
			razer_error("razer-deathadder: Cypress device didn't appear.\n");
			return -1;
		}
	}
	razer_msleep(100);

	err = cypress_open(&cy, cydev, NULL);
	if (err)
		return err;
	err = cypress_upload_image(&cy, data, len);
	cypress_close(&cy);
	if (err)
		return err;

	//FIXME need to reconnect the device?

	return 0;
}

static struct razer_mouse_profile * deathadder_get_profiles(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->drv_data;

	return &priv->profile;
}

static int deathadder_supported_dpimappings(struct razer_mouse *m,
					    struct razer_mouse_dpimapping **res_ptr)
{
	struct deathadder_private *priv = m->drv_data;

	*res_ptr = &priv->dpimapping[0];

	if (priv->type == DEATHADDER_CLASSIC)
		return ARRAY_SIZE(priv->dpimapping) - 1;
	return ARRAY_SIZE(priv->dpimapping);
}

static struct razer_mouse_dpimapping * deathadder_get_dpimapping(struct razer_mouse_profile *p,
								 struct razer_axis *axis)
{
	struct deathadder_private *priv = p->mouse->drv_data;

	return priv->cur_dpimapping;
}

static int deathadder_set_dpimapping(struct razer_mouse_profile *p,
				     struct razer_axis *axis,
				     struct razer_mouse_dpimapping *d)
{
	struct deathadder_private *priv = p->mouse->drv_data;
	struct razer_mouse_dpimapping *oldmapping;
	int err;

	if (!priv->m->claim_count)
		return -EBUSY;

	oldmapping = priv->cur_dpimapping;
	priv->cur_dpimapping = d;

	err = deathadder_commit(priv);
	if (err) {
		priv->cur_dpimapping = oldmapping;
		return err;
	}

	return err;
}

int razer_deathadder_init(struct razer_mouse *m,
			  struct libusb_device *usbdev)
{
	struct deathadder_private *priv;
	struct libusb_device_descriptor desc;
	unsigned int i;
	int err, fwver;
	const char *devname = "";

	err = libusb_get_device_descriptor(usbdev, &desc);
	if (err) {
		razer_error("hw_deathadder: Failed to get device descriptor\n");
		return -EIO;
	}

	priv = zalloc(sizeof(struct deathadder_private));
	if (!priv)
		return -ENOMEM;
	priv->m = m;
	m->drv_data = priv;

	priv->in_bootloader = is_cypress_bootloader(&desc);

	/* We need to wait some time between commits */
	razer_event_spacing_init(&priv->commit_spacing, 250);

	err = razer_usb_add_used_interface(m->usb_ctx, 0, 0);
	if (err)
		goto err_free;

	if (!priv->in_bootloader && desc.idProduct == 0x0007) {
		err = razer_usb_force_hub_reset(m->usb_ctx);
		if (err) {
			razer_error("hw_deathadder: Failed to reinit USB device\n");
			goto err_free;
		}
		usbdev = m->usb_ctx->dev;
	}

	err = m->claim(m);
	if (err) {
		razer_error("hw_deathadder: Failed to claim device\n");
		goto err_free;
	}

	/* Fetch firmware version */
	fwver = deathadder_read_fw_ver(priv);
	if (fwver < 0) {
		razer_error("hw_deathadder: Failed to get firmware version\n");
		err = fwver;
		goto err_release;
	}
	priv->fw_version = fwver;

	/* Determine the hardware revision */
	priv->type = DEATHADDER_CLASSIC;
	if (desc.idVendor == 0x1532 && desc.idProduct == 0x0029) {
		priv->type = DEATHADDER_BLACK;
	} else {
		if (fwver >= DADD_FW(2,0))
			priv->type = DEATHADDER_3500;
	}

	priv->frequency = RAZER_MOUSE_FREQ_1000HZ;
	priv->old_frequency = priv->frequency;
	for (i = 0; i < DEATHADDER_NR_LEDS; i++)
		priv->led_states[i] = RAZER_LED_ON;

	priv->profile.nr = 0;
	priv->profile.get_freq = deathadder_get_freq;
	priv->profile.set_freq = deathadder_set_freq;
	priv->profile.get_dpimapping = deathadder_get_dpimapping;
	priv->profile.set_dpimapping = deathadder_set_dpimapping;
	priv->profile.mouse = m;

	priv->dpimapping[0].nr = 0;
	priv->dpimapping[0].res = RAZER_MOUSE_RES_450DPI;
	priv->dpimapping[0].change = NULL;
	priv->dpimapping[0].mouse = m;

	priv->dpimapping[1].nr = 1;
	priv->dpimapping[1].res = RAZER_MOUSE_RES_900DPI;
	priv->dpimapping[1].change = NULL;
	priv->dpimapping[1].mouse = m;

	priv->dpimapping[2].nr = 2;
	priv->dpimapping[2].res = RAZER_MOUSE_RES_1800DPI;
	priv->dpimapping[2].change = NULL;
	priv->dpimapping[2].mouse = m;

	if (priv->type == DEATHADDER_CLASSIC) {
		priv->cur_dpimapping = &priv->dpimapping[2];
	} else {
		priv->dpimapping[3].nr = 3;
		priv->dpimapping[3].res = RAZER_MOUSE_RES_3500DPI;
		priv->dpimapping[3].change = NULL;
		priv->dpimapping[3].mouse = m;

		priv->cur_dpimapping = &priv->dpimapping[3];
	}

	m->type = RAZER_MOUSETYPE_DEATHADDER;
	switch (priv->type) {
	case DEATHADDER_CLASSIC:
		devname = "DeathAdder Classic";
		break;
	case DEATHADDER_3500:
		devname = "DeathAdder 3500DPI";
		break;
	case DEATHADDER_BLACK:
		devname = "DeathAdder Black Edition";
		break;
	}
	razer_generic_usb_gen_idstr(usbdev, m->usb_ctx->h, devname, 0, m->idstr);

	m->get_fw_version = deathadder_get_fw_version;
	m->get_leds = deathadder_get_leds;
	m->flash_firmware = deathadder_flash_firmware;
	m->get_profiles = deathadder_get_profiles;
	m->supported_resolutions = deathadder_supported_resolutions;
	m->supported_freqs = deathadder_supported_freqs;
	m->supported_dpimappings = deathadder_supported_dpimappings;

	err = deathadder_commit(priv);
	if (err) {
		razer_error("hw_deathadder: Failed to commit initial settings\n");
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

void razer_deathadder_release(struct razer_mouse *m)
{
	struct deathadder_private *priv = m->drv_data;

	free(priv);
}
