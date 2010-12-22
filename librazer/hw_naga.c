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
	/* Previous freq. For predicting reconnect events only. */
	enum razer_mouse_freq old_frequency;
	/* The currently set resolution. */
	struct razer_mouse_dpimapping *cur_dpimapping;

	struct razer_mouse_profile profile;
	struct razer_mouse_dpimapping dpimapping[56];
};

#define NAGA_USB_TIMEOUT		3000
#define NAGA_FW_IMAGE_SIZE	0x4000

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

static int naga_read_fw_ver(struct naga_private *priv)
{
	char buf[90];
	uint16_t ver;
	int err;
	
	// something is wrong, this workaround retries until a valid firmware version is returned
	do
	{
		for(int i=0;i<90;i++)
		{
			buf[i] = 0;
		}
		buf[5] = 0x02;
		buf[7] = 0x81;
		buf[88] = 0x83;
		err = naga_usb_write(priv, 0x09, 0x0300, buf, sizeof(buf));

		//err = usb_control_msg(priv->usb.h, USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0x01, 0x0300, 0x0000, buf, sizeof(buf), DEATHADDER_USB_TIMEOUT);
		err = naga_usb_read(priv, 0x01, 0x0300, buf, sizeof(buf));

//		if(err<0) 
//			printf("error %d\n",err);
/*
		for(int i=0;i<90;i++)
		{
			printf("%02X ",buf[i]);
			if(i%16 == 0 && i != 0)
				printf("\n");
		}
		printf("\n");
*/
	}while(buf[8] == 0 && 0/*FIXME*/);
	
	ver = buf[8];
	ver <<= 8;
	ver |= buf[9];
	
//	printf("Firmware: (%d.%d)\n", buf[8], buf[9]);
	
	return ver;
}

static int naga_commit(struct naga_private *priv)
{
	int err;
	char buf[90];

	/* Translate frequency setting. */
	switch (priv->frequency) {
	case RAZER_MOUSE_FREQ_125HZ:
		
		break;
	case RAZER_MOUSE_FREQ_500HZ:
		
		break;
	case RAZER_MOUSE_FREQ_1000HZ:
	case RAZER_MOUSE_FREQ_UNKNOWN:
		
		break;
	default:
		return -EINVAL;
	}

	/* set the scroll wheel and buttons */
	for(int i=0;i<90;i++)
	{
		buf[i] = 0;
	}
	
	buf[5] = 0x03;
	buf[6] = 0x03;
	buf[8] = 0x01;
	buf[9] = 0x01;
	if (priv->led_states[NAGA_LED_SCROLL])
	{
		buf[10] = 0x01;
		buf[88] = 0x01;
	}
	
	err = naga_usb_write(priv, 0x09, 0x0300, buf, sizeof(buf));
	err = naga_usb_read(priv, 0x01, 0x0300, buf, sizeof(buf));
	
	/* now the logo */
	for(int i=0;i<90;i++)
		{
			buf[i] = 0;
		}
	buf[5] = 0x03;
	buf[6] = 0x03;
	buf[8] = 0x01;
	buf[9] = 0x04;
	if(priv->led_states[NAGA_LED_LOGO])
	{
		buf[10] = 0x01;
		buf[88] = 0x04;
	}else
	{
		buf[10] = 0x00;
		buf[88] = 0x05;
	}
	err = naga_usb_write(priv, 0x09, 0x0300, buf, sizeof(buf));
	err = naga_usb_read(priv, 0x01, 0x0300, buf, sizeof(buf));
	/*printf("logo led:\n");
	for(int i=0;i<90;i++)
	{
		printf("%02X ",buf[i]);
		if(i%16 == 0 && i != 0)
			printf("\n");
	}
	printf("\n");
	*/
	
	if (err)
		return err;
	
	/* set the resolution */
	int res = (priv->cur_dpimapping->res/100)-1;
	res <<=2;
	
	//printf("selected res is 0x%02X\n",res);

	for(int i=0;i<90;i++)
	{
		buf[i] = 0;
	}
	buf[5] = 0x03;
	buf[6] = 0x04;
	buf[7] = 0x01;
	buf[8] = res;
	buf[9] = res;
	buf[88]=0x06;
	err = naga_usb_write(priv, 0x09, 0x0300, buf, sizeof(buf));
	err = naga_usb_read(priv, 0x01, 0x0300, buf, sizeof(buf));

	return 0;
}

static int naga_claim(struct razer_mouse *m)
{
	struct naga_private *priv = m->internal;
	int err;

	if (!priv->claimed) {
		err = razer_generic_usb_claim(&priv->usb);
		if (err)
			return err;
	}
	priv->claimed++;

	return 0;
}

static void naga_release(struct razer_mouse *m)
{
	struct naga_private *priv = m->internal;

	priv->claimed--;
	if (!priv->claimed)
		razer_generic_usb_release(&priv->usb);
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

	scroll = malloc(sizeof(struct razer_led));
	if (!scroll)
		return -ENOMEM;
	logo = malloc(sizeof(struct razer_led));
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

	list = malloc(sizeof(*list) * count);
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
	priv->old_frequency = old_freq;
	priv->frequency = freq;

	err = naga_commit(priv);
	if (err) {
		priv->frequency = old_freq;
		return err;
	}
	priv->old_frequency = freq;

	return err;
}

static int naga_supported_resolutions(struct razer_mouse *m,
					    enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	const int count = 55;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	for(int i = 1;i<56;i++)
	{
		list[i-1] = i*100;
	}
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

static struct razer_mouse_dpimapping * naga_get_dpimapping(struct razer_mouse_profile *p)
{
	struct naga_private *priv = p->mouse->internal;

	return priv->cur_dpimapping;
}

static int naga_set_dpimapping(struct razer_mouse_profile *p,
				     struct razer_mouse_dpimapping *d)
{
	struct naga_private *priv = p->mouse->internal;
	struct razer_mouse_dpimapping *oldmapping;
	int err;

	if (!priv->claimed)
		return -EBUSY;

	oldmapping = priv->cur_dpimapping;
	priv->cur_dpimapping = d;

	err = naga_commit(priv);
	if (err) {
		priv->cur_dpimapping = oldmapping;
		return err;
	}

	return err;
}

void razer_naga_gen_idstr(struct usb_device *udev, char *buf)
{
	char devid[64];
	char serial[64];
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

	/* We can't include the USB device number, because that changes on the
	 * automatic reconnects the device firmware does.
	 * The serial number is zero, so that's not very useful, too.
	 * Basically, that means we have a pretty bad ID string due to
	 * major design faults in the hardware. :(
	 */
	snprintf(devid, sizeof(devid), "%04X-%04X-%s",
		 udev->descriptor.idVendor,
		 udev->descriptor.idProduct, serial);
	razer_create_idstr(buf, BUSTYPESTR_USB, udev->bus->dirname,
			   DEVTYPESTR_MOUSE, "Naga", devid);
}

void razer_naga_assign_usb_device(struct razer_mouse *m,
					struct usb_device *usbdev)
{
	struct naga_private *priv = m->internal;

	priv->usb.dev = usbdev;
}

int razer_naga_init_struct(struct razer_mouse *m,
				 struct usb_device *usbdev)
{
	struct naga_private *priv;
	unsigned int i;
	int fwver;

	priv = malloc(sizeof(struct naga_private));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));
	m->internal = priv;

	razer_naga_assign_usb_device(m, usbdev);

	/* Fetch firmware version */
	naga_claim(m);
	fwver = naga_read_fw_ver(priv);
	if (fwver < 0) {
		razer_error("hw_naga: Failed to get firmware version\n");
		naga_release(m);
		free(priv);
		return fwver;
	}
	priv->fw_version = fwver;
	naga_release(m);

	priv->frequency = RAZER_MOUSE_FREQ_1000HZ;
	priv->old_frequency = priv->frequency;
	for (i = 0; i < NAGA_NR_LEDS; i++)
		priv->led_states[i] = RAZER_LED_ON;

	priv->profile.nr = 0;
	priv->profile.get_freq = naga_get_freq;
	priv->profile.set_freq = naga_set_freq;
	priv->profile.get_dpimapping = naga_get_dpimapping;
	priv->profile.set_dpimapping = naga_set_dpimapping;
	priv->profile.mouse = m;

	for(int i = 1;i<57;i++)
	{
		priv->dpimapping[i-1].nr = i-1;
		priv->dpimapping[i-1].res = i*100;
		priv->dpimapping[i-1].change = NULL;
		priv->dpimapping[i-1].mouse = m;
	}
	
	priv->cur_dpimapping = &priv->dpimapping[54];
	
	m->type = RAZER_MOUSETYPE_NAGA;
	razer_naga_gen_idstr(usbdev, m->idstr);

	m->claim = naga_claim;
	m->release = naga_release;
	m->get_fw_version = naga_get_fw_version;
	m->get_leds = naga_get_leds;
	m->nr_profiles = 1;
	m->get_profiles = naga_get_profiles;
	m->get_active_profile = naga_get_active_profile;
	m->supported_resolutions = naga_supported_resolutions;
	m->supported_freqs = naga_supported_freqs;
	m->supported_dpimappings = naga_supported_dpimappings;

	return 0;
}

void razer_naga_release(struct razer_mouse *m)
{
	struct naga_private *priv = m->internal;

	naga_release(m);
	free(priv);
}
