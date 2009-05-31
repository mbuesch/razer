/*
 *   Lowlevel hardware access for the
 *   Razer Lachesis mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering, only.
 *
 *   Copyright (C) 2008-2009 Michael Buesch <mb@bu3sch.de>
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


enum {
	LACHESIS_LED_SCROLL = 0,
	LACHESIS_LED_LOGO,
	LACHESIS_NR_LEDS,
};

struct lachesis_private {
	bool claimed;
	struct razer_usb_context usb;

	/* The currently set LED states. */
	enum razer_led_state led_states[LACHESIS_NR_LEDS];

	/* The active profile. */
	struct razer_mouse_profile *cur_profile;
	/* Profile configuration (one per profile). */
	struct razer_mouse_profile profiles[5];

	/* The active DPI mapping; per profile. */
	struct razer_mouse_dpimapping *cur_dpimapping[5];
	/* The possible DPI mappings. */
	struct razer_mouse_dpimapping dpimappings[5];

	/* The active scan frequency; per profile. */
	enum razer_mouse_freq cur_freq[5];
};

/* The wire protocol data structures... */

struct lachesis_buttonmapping {
	uint8_t physical;
	uint8_t logical;
	uint8_t _padding[33];
} __attribute__((packed));

struct lachesis_profcfg_cmd {
	be32_t magic;
	uint8_t profile;
	uint8_t _padding0;
	uint8_t dpisel;
	uint8_t freq;
	uint8_t _padding1;
	struct lachesis_buttonmapping buttons[11];
	le16_t checksum;
} __attribute__((packed));
#define LACHESIS_PROFCFG_MAGIC		cpu_to_be32(0x8C010200)

struct lachesis_one_dpimapping {
	uint8_t magic;
	uint8_t dpival0;
	uint8_t dpival1;
} __attribute__((packed));
#define LACHESIS_DPIMAPPING_MAGIC	0x01

struct lachesis_dpimap_cmd {
	struct lachesis_one_dpimapping mappings[5];
	uint8_t _padding[81];
};


#define LACHESIS_USB_TIMEOUT	3000

static int lachesis_usb_write(struct lachesis_private *priv,
			      int request, int command,
			      void *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      LACHESIS_USB_TIMEOUT);
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

static int lachesis_commit(struct lachesis_private *priv)
{
	unsigned int i;
	int err;
	char value;
	struct lachesis_profcfg_cmd profcfg;
	struct lachesis_dpimap_cmd dpimap;

	/* Commit the profile configuration. */
	for (i = 0; i < 5; i++) {
		memset(&profcfg, 0, sizeof(profcfg));
		profcfg.magic = LACHESIS_PROFCFG_MAGIC;
		profcfg.profile = i + 1;
		profcfg.dpisel = priv->cur_dpimapping[i]->nr + 1;
		switch (priv->cur_freq[i]) {
		default:
		case RAZER_MOUSE_FREQ_1000HZ:
			profcfg.freq = 1;
			break;
		case RAZER_MOUSE_FREQ_500HZ:
			profcfg.freq = 2;
			break;
		case RAZER_MOUSE_FREQ_125HZ:
			profcfg.freq = 3;
			break;
		}
		//TODO buttons
		profcfg.checksum = razer_xor16_checksum(&profcfg,
				sizeof(profcfg) - sizeof(profcfg.checksum));
		err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
					 0x01, &profcfg, sizeof(profcfg));
		if (err)
			return err;
	}

	/* Commit LED states. */
	value = 0;
	if (priv->led_states[LACHESIS_LED_LOGO])
		value |= 0x01;
	if (priv->led_states[LACHESIS_LED_SCROLL])
		value |= 0x02;
	err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				 0x04, &value, sizeof(value));
	if (err)
		return err;

	/* Commit the active profile selection. */
	value = priv->cur_profile->nr;
	err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				 0x08, &value, sizeof(value));
	if (err)
		return err;

	/* Commit the DPI map. */
	for (i = 0; i < 5; i++) {
		memset(&dpimap, 0, sizeof(dpimap));
		dpimap.mappings[i].magic = LACHESIS_DPIMAPPING_MAGIC;
		dpimap.mappings[i].dpival0 = (priv->dpimappings[i].res / 125) - 1;
		dpimap.mappings[i].dpival1 = dpimap.mappings[i].dpival0;
	}
	err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				 0x12, &dpimap, sizeof(dpimap));
	if (err)
		return err;

	return 0;
}

static int lachesis_claim(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;
	int err;

	err = razer_generic_usb_claim(&priv->usb);
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

	razer_generic_usb_release(&priv->usb);
	priv->claimed = 0;
}

static int lachesis_get_fw_version(struct razer_mouse *m)
{
	return -EOPNOTSUPP;
}

static int lachesis_led_toggle(struct razer_led *led,
			       enum razer_led_state new_state)
{
	struct razer_mouse *m = led->u.mouse;
	struct lachesis_private *priv = m->internal;
	int err;
	enum razer_led_state old_state;

	if (led->id >= LACHESIS_NR_LEDS)
		return -EINVAL;
	if ((new_state != RAZER_LED_OFF) &&
	    (new_state != RAZER_LED_ON))
		return -EINVAL;

	if (!priv->claimed)
		return -EBUSY;

	old_state = priv->led_states[led->id];
	priv->led_states[led->id] = new_state;

	err = lachesis_commit(priv);
	if (err) {
		priv->led_states[led->id] = old_state;
		return err;
	}

	return err;
}

static int lachesis_get_leds(struct razer_mouse *m,
			     struct razer_led **leds_list)
{
	struct lachesis_private *priv = m->internal;
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
	scroll->id = LACHESIS_LED_SCROLL;
	scroll->state = priv->led_states[LACHESIS_LED_SCROLL];
	scroll->toggle_state = lachesis_led_toggle;
	scroll->u.mouse = m;

	logo->name = "GlowingLogo";
	logo->id = LACHESIS_LED_LOGO;
	logo->state = priv->led_states[LACHESIS_LED_LOGO];
	logo->toggle_state = lachesis_led_toggle;
	logo->u.mouse = m;

	/* Link the list */
	*leds_list = scroll;
	scroll->next = logo;
	logo->next = NULL;

	return LACHESIS_NR_LEDS;
}

static int lachesis_supported_freqs(struct razer_mouse *m,
				    enum razer_mouse_freq **freq_list)
{
	enum razer_mouse_freq *list;
	const int count = 3;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	list[0] = RAZER_MOUSE_FREQ_1000HZ;
	list[1] = RAZER_MOUSE_FREQ_500HZ;
	list[2] = RAZER_MOUSE_FREQ_125HZ;

	*freq_list = list;

	return count;
}

static enum razer_mouse_freq lachesis_get_freq(struct razer_mouse_profile *p)
{
	struct lachesis_private *priv = p->mouse->internal;

	if (p->nr >= ARRAY_SIZE(priv->cur_freq))
		return -EINVAL;

	return priv->cur_freq[p->nr];
}

static int lachesis_set_freq(struct razer_mouse_profile *p,
			     enum razer_mouse_freq freq)
{
	struct lachesis_private *priv = p->mouse->internal;
	enum razer_mouse_freq oldfreq;
	int err;

	if (!priv->claimed)
		return -EBUSY;
	if (p->nr >= ARRAY_SIZE(priv->cur_freq))
		return -EINVAL;

	oldfreq = priv->cur_freq[p->nr];
	priv->cur_freq[p->nr] = freq;

	err = lachesis_commit(priv);
	if (err) {
		priv->cur_freq[p->nr] = oldfreq;
		return err;
	}

	return 0;
}

static int lachesis_supported_resolutions(struct razer_mouse *m,
					  enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	const int count = 32;
	unsigned int i, res;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	res = RAZER_MOUSE_RES_125DPI;
	for (i = 0; i < count; i++) {
		list[i] = res;
		res += RAZER_MOUSE_RES_125DPI;
	}

	*res_list = list;

	return count;
}

static struct razer_mouse_profile * lachesis_get_profiles(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	return &priv->profiles[0];
}

static struct razer_mouse_profile * lachesis_get_active_profile(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	return priv->cur_profile;
}

static int lachesis_set_active_profile(struct razer_mouse *m,
				       struct razer_mouse_profile *p)
{
	struct lachesis_private *priv = m->internal;
	struct razer_mouse_profile *oldprof;
	int err;

	if (!priv->claimed)
		return -EBUSY;

	oldprof = priv->cur_profile;
	priv->cur_profile = p;

	err = lachesis_commit(priv);
	if (err) {
		priv->cur_profile = oldprof;
		return err;
	}

	return err;
}

static int lachesis_supported_dpimappings(struct razer_mouse *m,
					  struct razer_mouse_dpimapping **res_ptr)
{
	struct lachesis_private *priv = m->internal;

	*res_ptr = &priv->dpimappings[0];

	return ARRAY_SIZE(priv->dpimappings);
}

static struct razer_mouse_dpimapping * lachesis_get_dpimapping(struct razer_mouse_profile *p)
{
	struct lachesis_private *priv = p->mouse->internal;

	if (p->nr >= ARRAY_SIZE(priv->cur_dpimapping))
		return NULL;

	return priv->cur_dpimapping[p->nr];
}

static int lachesis_set_dpimapping(struct razer_mouse_profile *p,
				   struct razer_mouse_dpimapping *d)
{
	struct lachesis_private *priv = p->mouse->internal;
	struct razer_mouse_dpimapping *oldmapping;
	int err;

	if (!priv->claimed)
		return -EBUSY;
	if (p->nr >= ARRAY_SIZE(priv->cur_dpimapping))
		return -EINVAL;

	oldmapping = priv->cur_dpimapping[p->nr];
	priv->cur_dpimapping[p->nr] = d;

	err = lachesis_commit(priv);
	if (err) {
		priv->cur_dpimapping[p->nr] = oldmapping;
		return err;
	}

	return 0;
}

static int lachesis_dpimapping_modify(struct razer_mouse_dpimapping *d,
				      enum razer_mouse_res res)
{
	struct lachesis_private *priv = d->mouse->internal;
	enum razer_mouse_res oldres;
	int err;

	if (!priv->claimed)
		return -EBUSY;

	oldres = d->res;
	d->res = res;

	err = lachesis_commit(priv);
	if (err) {
		d->res = oldres;
		return err;
	}

	return 0;
}

void razer_lachesis_gen_idstr(struct usb_device *udev, char *buf)
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
			   DEVTYPESTR_MOUSE, "Lachesis", devid);
}

void razer_lachesis_assign_usb_device(struct razer_mouse *m,
				      struct usb_device *usbdev)
{
	struct lachesis_private *priv = m->internal;

	priv->usb.dev = usbdev;
}

static void lachesis_init_profile_struct(struct razer_mouse_profile *p,
					 struct razer_mouse *m,
					 unsigned int nr)
{
	p->nr = nr;
	p->get_freq = lachesis_get_freq;
	p->set_freq = lachesis_set_freq;
	p->get_dpimapping = lachesis_get_dpimapping;
	p->set_dpimapping = lachesis_set_dpimapping;
	p->mouse = m;
}

static void lachesis_init_dpimapping_struct(struct razer_mouse_dpimapping *d,
					    struct razer_mouse *m,
					    unsigned int nr,
					    enum razer_mouse_res res)
{
	d->nr = nr;
	d->res = res;
	d->change = lachesis_dpimapping_modify;
	d->mouse = m;
}

int razer_lachesis_init_struct(struct razer_mouse *m,
			       struct usb_device *usbdev)
{
	struct lachesis_private *priv;
	unsigned int i;

	if (sizeof(struct lachesis_profcfg_cmd) != 0x18C) {
		fprintf(stderr, "librazer: hw_lachesis: "
			"Invalid struct lachesis_profcfg_cmd size (0x%X).\n",
			(unsigned int)sizeof(struct lachesis_profcfg_cmd));
		return -EINVAL;
	}
	if (sizeof(struct lachesis_dpimap_cmd) != 0x60) {
		fprintf(stderr, "librazer: hw_lachesis: "
			"Invalid struct lachesis_dpimap_cmd size (0x%X).\n",
			(unsigned int)sizeof(struct lachesis_dpimap_cmd));
		return -EINVAL;
	}

	priv = malloc(sizeof(struct lachesis_private));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));
	m->internal = priv;

	for (i = 0; i < ARRAY_SIZE(priv->profiles); i++)
		lachesis_init_profile_struct(&priv->profiles[i], m, i);
	priv->cur_profile = &priv->profiles[0];

	//FIXME what are the default mappings?
	lachesis_init_dpimapping_struct(&priv->dpimappings[0], m, 0, RAZER_MOUSE_RES_UNKNOWN);
	lachesis_init_dpimapping_struct(&priv->dpimappings[1], m, 1, RAZER_MOUSE_RES_UNKNOWN);
	lachesis_init_dpimapping_struct(&priv->dpimappings[2], m, 2, RAZER_MOUSE_RES_UNKNOWN);
	lachesis_init_dpimapping_struct(&priv->dpimappings[3], m, 3, RAZER_MOUSE_RES_UNKNOWN);
	lachesis_init_dpimapping_struct(&priv->dpimappings[4], m, 4, RAZER_MOUSE_RES_UNKNOWN);
	for (i = 0; i < ARRAY_SIZE(priv->cur_dpimapping); i++)
		priv->cur_dpimapping[i] = &priv->dpimappings[0];//FIXME

	for (i = 0; i < ARRAY_SIZE(priv->cur_freq); i++)
		priv->cur_freq[i] = RAZER_MOUSE_FREQ_1000HZ;//FIXME?

	razer_lachesis_assign_usb_device(m, usbdev);
	for (i = 0; i < LACHESIS_NR_LEDS; i++)
		priv->led_states[i] = RAZER_LED_ON;

	m->type = RAZER_MOUSETYPE_LACHESIS;
	razer_lachesis_gen_idstr(usbdev, m->idstr);

	m->claim = lachesis_claim;
	m->release = lachesis_release;
	m->get_fw_version = lachesis_get_fw_version;
	m->get_leds = lachesis_get_leds;
	m->nr_profiles = ARRAY_SIZE(priv->profiles);
	m->get_profiles = lachesis_get_profiles;
	m->get_active_profile = lachesis_get_active_profile;
	m->set_active_profile = lachesis_set_active_profile;
	m->supported_resolutions = lachesis_supported_resolutions;
	m->supported_freqs = lachesis_supported_freqs;
	m->supported_dpimappings = lachesis_supported_dpimappings;

	return 0;
}

void razer_lachesis_release(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	lachesis_release(m);
	free(priv);
}
