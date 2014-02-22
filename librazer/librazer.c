/*
 *   Razer device access library
 *
 *   Copyright (C) 2007-2011 Michael Buesch <m@bues.ch>
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

#include "librazer.h"
#include "razer_private.h"
#include "config.h"
#include "profile_emulation.h"

#include "hw_deathadder.h"
#include "hw_deathadder2013.h"
#include "hw_naga.h"
#include "hw_krait.h"
#include "hw_lachesis.h"
#include "hw_lachesis5k6.h"
#include "hw_copperhead.h"
#include "hw_boomslangce.h"
#include "hw_imperator.h"
#include "hw_taipan.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>


enum razer_devtype {
	RAZER_DEVTYPE_MOUSE,
};

/** struct razer_mouse_base_ops - Basic device-init operations
 *
 * @type: The type ID.
 *
 * @init: Initialize the device and its private data structures.
 *
 * @release: Release device and data structures.
 */
struct razer_mouse_base_ops {
	enum razer_mouse_type type;
	int (*init)(struct razer_mouse *m, struct libusb_device *udev);
	void (*release)(struct razer_mouse *m);
};

struct razer_usb_device {
	uint16_t vendor;	/* Vendor ID */
	uint16_t product;	/* Product ID */
	enum razer_devtype type;
	union {
		const struct razer_mouse_base_ops *mouse_ops;
	} u;
};

static const struct razer_mouse_base_ops razer_deathadder_base_ops = {
	.type			= RAZER_MOUSETYPE_DEATHADDER,
	.init			= razer_deathadder_init,
	.release		= razer_deathadder_release,
};

static const struct razer_mouse_base_ops razer_deathadder2013_base_ops = {
	.type			= RAZER_MOUSETYPE_DEATHADDER,
	.init			= razer_deathadder2013_init,
	.release		= razer_deathadder2013_release,
};

static const struct razer_mouse_base_ops razer_naga_base_ops = {
	.type			= RAZER_MOUSETYPE_NAGA,
	.init			= razer_naga_init,
	.release		= razer_naga_release,
};

static const struct razer_mouse_base_ops razer_krait_base_ops = {
	.type			= RAZER_MOUSETYPE_KRAIT,
	.init			= razer_krait_init,
	.release		= razer_krait_release,
};

static const struct razer_mouse_base_ops razer_lachesis_base_ops = {
	.type			= RAZER_MOUSETYPE_LACHESIS,
	.init			= razer_lachesis_init,
	.release		= razer_lachesis_release,
};

static const struct razer_mouse_base_ops razer_lachesis5k6_base_ops = {
	.type			= RAZER_MOUSETYPE_LACHESIS,
	.init			= razer_lachesis5k6_init,
	.release		= razer_lachesis5k6_release,
};

static const struct razer_mouse_base_ops razer_copperhead_base_ops = {
	.type			= RAZER_MOUSETYPE_COPPERHEAD,
	.init			= razer_copperhead_init,
	.release		= razer_copperhead_release,
};

static const struct razer_mouse_base_ops razer_boomslangce_base_ops = {
	.type			= RAZER_MOUSETYPE_BOOMSLANGCE,
	.init			= razer_boomslangce_init,
	.release		= razer_boomslangce_release,
};

static const struct razer_mouse_base_ops razer_imperator_base_ops = {
	.type			= RAZER_MOUSETYPE_IMPERATOR,
	.init			= razer_imperator_init,
	.release		= razer_imperator_release,
};

static const struct razer_mouse_base_ops razer_taipan_base_ops = {
	.type			= RAZER_MOUSETYPE_TAIPAN,
	.init			= razer_taipan_init,
	.release		= razer_taipan_release,
};

#define USBVENDOR_ANY	0xFFFF
#define USBPRODUCT_ANY	0xFFFF

#define USB_MOUSE(_vendor, _product, _mouse_ops)	\
	{ .vendor = _vendor, .product = _product,	\
	  .type = RAZER_DEVTYPE_MOUSE,			\
	  .u = { .mouse_ops = _mouse_ops, }, }

/* Table of supported USB devices. */
static const struct razer_usb_device razer_usbdev_table[] = {
	USB_MOUSE(0x1532, 0x0007, &razer_deathadder_base_ops), /* classic */
	USB_MOUSE(0x1532, 0x0016, &razer_deathadder_base_ops), /* 3500 DPI */
	USB_MOUSE(0x1532, 0x0029, &razer_deathadder_base_ops), /* black edition */
	USB_MOUSE(0x1532, 0x0037, &razer_deathadder2013_base_ops), /* 2013 edition */
//	USB_MOUSE(0x04B4, 0xE006, &razer_deathadder_base_ops), /* cypress bootloader */
	USB_MOUSE(0x1532, 0x0003, &razer_krait_base_ops),
	USB_MOUSE(0x1532, 0x000C, &razer_lachesis_base_ops), /* classic */
	USB_MOUSE(0x1532, 0x001E, &razer_lachesis5k6_base_ops), /* 5600 DPI */
	USB_MOUSE(0x1532, 0x0015, &razer_naga_base_ops), /* classic */
	USB_MOUSE(0x1532, 0x002e, &razer_naga_base_ops), /* 2012 */
	USB_MOUSE(0x1532, 0x0036, &razer_naga_base_ops), /* Hex */
	USB_MOUSE(0x1532, 0x0040, &razer_naga_base_ops), /* 2014 */
	USB_MOUSE(0x1532, 0x0101, &razer_copperhead_base_ops),
	USB_MOUSE(0x1532, 0x0005, &razer_boomslangce_base_ops),
	USB_MOUSE(0x1532, 0x0017, &razer_imperator_base_ops),
	USB_MOUSE(0x1532, 0x0034, &razer_taipan_base_ops),
	{ 0, }, /* List end */
};
#undef USB_MOUSE



static struct libusb_context *libusb_ctx;
static struct razer_mouse *mice_list = NULL;
/* We currently only have one handler. */
static razer_event_handler_t event_handler;
static struct config_file *razer_config_file = NULL;
static bool profile_emu_enabled;

razer_logfunc_t razer_logfunc_info;
razer_logfunc_t razer_logfunc_error;
razer_logfunc_t razer_logfunc_debug;


static inline bool razer_initialized(void)
{
	return !!libusb_ctx;
}

int razer_register_event_handler(razer_event_handler_t handler)
{
	if (event_handler)
		return -EEXIST;
	event_handler = handler;
	return 0;
}

void razer_unregister_event_handler(razer_event_handler_t handler)
{
	event_handler = NULL;
}

static void razer_notify_event(enum razer_event type,
			       const struct razer_event_data *data)
{
	if (event_handler)
		event_handler(type, data);
}

static int match_usbdev(const struct libusb_device_descriptor *desc,
			const struct razer_usb_device *id)
{
	if ((desc->idVendor != id->vendor) &&
	    (id->vendor != USBVENDOR_ANY))
		return 0;
	if ((desc->idProduct != id->product) &&
	    (id->product != USBPRODUCT_ANY))
		return 0;
	return 1;
}

static const struct razer_usb_device * usbdev_lookup(const struct libusb_device_descriptor *desc)
{
	const struct razer_usb_device *id = &(razer_usbdev_table[0]);

	while (id->vendor || id->product) {
		if (match_usbdev(desc, id))
			return id;
		id++;
	}
	return NULL;
}

static void mouse_list_add(struct razer_mouse **base, struct razer_mouse *new_entry)
{
	struct razer_mouse *i;

	new_entry->next = NULL;
	if (!(*base)) {
		*base = new_entry;
		return;
	}
	for (i = *base; i->next; i = i->next)
		;
	i->next = new_entry;
}

static void mouse_list_del(struct razer_mouse **base, struct razer_mouse *del_entry)
{
	struct razer_mouse *i;

	if (del_entry == *base) {
		*base = (*base)->next;
		return;
	}
	for (i = *base; i && (i->next != del_entry); i = i->next)
		;
	if (i)
		i->next = del_entry->next;
}

static struct razer_mouse * mouse_list_find(struct razer_mouse *base,
					    struct libusb_device *udev)
{
	struct razer_mouse *m, *next;
	uint8_t busnr = libusb_get_bus_number(udev);
	uint8_t devaddr = libusb_get_device_address(udev);

	razer_for_each_mouse(m, next, base) {
		if (m->usb_ctx) {
			if (libusb_get_bus_number(m->usb_ctx->dev) == busnr &&
			    libusb_get_device_address(m->usb_ctx->dev) == devaddr)
				return m;
		}
	}

	return NULL;
}

static int parse_idstr(char *idstr, char **devtype, char **devname,
				    char **buspos, char **devid)
{
	*devtype = idstr;
	*devname = razer_strsplit(*devtype, ':');
	*buspos = razer_strsplit(*devname, ':');
	*devid = razer_strsplit(*buspos, ':');
	if (!*devtype || !*devname || !*buspos || !*devid)
		return -EINVAL;
	return 0;
}

static bool simple_globcmp(const char *string,
			   const char *template)
{
	char s, t, tnext;

	while (1) {
		s = string[0];
		t = template[0];

		if (s == '\0' && t == '\0')
			break;

		if (t == '*') {
			tnext = template[1];
			if (s == '\0') {
				if (tnext == '\0')
					break;
				return 0;
			}
			if (s == tnext) {
				template++;
				continue;
			}
		} else {
			if (s == '\0' || t == '\0')
				return 0;
			if (s != t)
				return 0;
			template++;
		}
		string++;
	}

	return 1; /* Match */
}

static bool mouse_idstr_glob_match(struct config_file *f,
				   void *context, void *data,
				   const char *section)
{
	struct razer_mouse *m = context;
	const char **matched_section = data;
	char idstr[RAZER_IDSTR_MAX_SIZE + 1] = { 0, };
	char *idstr_devtype, *idstr_devname, *idstr_buspos, *idstr_devid;
	char globstr[RAZER_IDSTR_MAX_SIZE + 1] = { 0, };
	char *globstr_devtype, *globstr_devname, *globstr_buspos, *globstr_devid;

	if (strlen(section) > RAZER_IDSTR_MAX_SIZE) {
		razer_error("globbed idstr \"%s\" in config too long\n",
			section);
		return 1;
	}
	strcpy(globstr, section);
	strcpy(idstr, m->idstr);
	if (parse_idstr(globstr, &globstr_devtype, &globstr_devname,
				 &globstr_buspos, &globstr_devid))
		return 1;
	if (parse_idstr(idstr, &idstr_devtype, &idstr_devname,
			       &idstr_buspos, &idstr_devid)) {
		razer_error("INTERNAL-ERROR: Failed to parse idstr \"%s\"\n",
			idstr);
		return 1;
	}

	if (!simple_globcmp(idstr_devtype, globstr_devtype))
		return 1;
	if (!simple_globcmp(idstr_devname, globstr_devname))
		return 1;
	if (!simple_globcmp(idstr_buspos, globstr_buspos))
		return 1;
	if (!simple_globcmp(idstr_devid, globstr_devid))
		return 1;

	*matched_section = section;

	return 0; /* Match */
}

static struct razer_mouse_profile * find_prof(struct razer_mouse *m, unsigned int nr)
{
	struct razer_mouse_profile *list;
	unsigned int i;

	if (!m->get_profiles)
		return NULL;
	list = m->get_profiles(m);
	if (!list)
		return NULL;
	for (i = 0; i < m->nr_profiles; i++) {
		if (list[i].nr == nr)
			return &list[i];
	}
	return NULL;
}

static int parse_int_int_pair(const char *str, int *val0, int *val1)
{
	char a[64] = { 0, }, b[64] = { 0, };
	int err;

	*val0 = *val1 = -1;
	err = razer_split_tuple(str, ':', min(sizeof(a), sizeof(b)),
				a, b, NULL);
	if (err) {
		/* It's not a pair. Interpret it as one value. */
		razer_strlcpy(a, str, sizeof(a));
		err = razer_string_to_int(razer_string_strip(a), val1);
		if (err)
			return -EINVAL;
		return 1;
	}
	err = razer_string_to_int(razer_string_strip(a), val0);
	err |= razer_string_to_int(razer_string_strip(b), val1);
	if (err)
		return -EINVAL;

	return 0;
}

static bool mouse_apply_one_config(struct config_file *f,
				   void *context, void *data,
				   const char *section,
				   const char *item,
				   const char *value)
{
	struct razer_mouse *m = context;
	struct razer_mouse_profile *prof;
	bool *error = data;
	int err, nr;
	static const size_t tmplen = 128;
	char a[tmplen], b[tmplen], c[tmplen];

//FIXME fixes for glob/prof configs
	if (strcasecmp(item, "profile") == 0) {
		int profile;

		err = razer_string_to_int(value, &profile);
		if (err || profile < 1 || profile > m->nr_profiles)
			goto error;
		if (m->set_active_profile) {
			prof = find_prof(m, profile - 1);
			if (!prof)
				goto error;
			err = m->set_active_profile(m, prof);
			if (err)
				goto error;
		}
	} else if (strcasecmp(item, "res") == 0) {
		int profile, resolution, i;
		struct razer_mouse_dpimapping *mappings;

		err = parse_int_int_pair(value, &profile, &resolution);
		if (err == 1) {
			prof = m->get_active_profile(m);
			profile = prof->nr + 1;
		} else if (err)
			goto error;
		if (profile < 1 || resolution < 1)
			goto error;
		prof = find_prof(m, profile - 1);
		if (!prof)
			goto error;
		nr = m->supported_dpimappings(m, &mappings);
		if (nr <= 0)
			goto error;
		//FIXME dims
		for (i = 0; i < nr; i++) {
			if (resolution >= 100) {
				if ((int)(mappings[i].res[RAZER_DIM_0]) != resolution)
					continue;
			} else {
				if (mappings[i].nr != resolution)
					continue;
			}
			err = prof->set_dpimapping(prof, NULL, &mappings[i]);
			if (err)
				goto error;
			goto ok;
		}
		goto error;
	} else if (strcasecmp(item, "freq") == 0) {
		int profile, freq, i;
		enum razer_mouse_freq *freqs;

		err = parse_int_int_pair(value, &profile, &freq);
		if (err == 1) {
			prof = m->get_active_profile(m);
			profile = prof->nr + 1;
		} else if (err)
			goto error;
		if (profile < 1 || freq < 1)
			goto error;
		prof = find_prof(m, profile - 1);
		if (!prof)
			goto error;
		nr = m->supported_freqs(m, &freqs);
		if (nr <= 0)
			goto error;
		for (i = 0; i < nr; i++) {
			if (freqs[i] != freq)
				continue;
			err = prof->set_freq(prof, freqs[i]);
			razer_free_freq_list(freqs, nr);
			if (err)
				goto error;
			goto ok;
		}
		razer_free_freq_list(freqs, nr);
		goto error;
	} else if (strcasecmp(item, "led") == 0) {
		bool on;
		struct razer_led *leds, *led;
		const char *ledname, *ledstate;
		int profile;

		err = razer_split_tuple(value, ':', tmplen, a, b, c, NULL);
		if (err && err != -ENODATA)
			goto error;
		if (!strlen(a) || !strlen(b))
			goto error;
		if (strlen(c)) {
			/* A profile was specified */
			err = razer_string_to_int(razer_string_strip(a), &profile);
			if (err || profile < 1)
				goto error;
			prof = find_prof(m, profile - 1);
			if (!prof)
				goto error;
			ledname = razer_string_strip(b);
			ledstate = razer_string_strip(c);
		} else {
			/* Modify global LEDs */
			prof = NULL;
			ledname = razer_string_strip(a);
			ledstate = razer_string_strip(b);
		}
		err = razer_string_to_bool(ledstate, &on);
		if (err)
			goto error;
		if (prof) {
			if (prof->get_leds) {
				err = prof->get_leds(prof, &leds);
			} else {
				/* Try to fall back to global */
				if (!m->global_get_leds)
					goto ok; /* No LEDs. Ignore config. */
				err = m->global_get_leds(m, &leds);
			}
		} else {
			if (!m->global_get_leds)
				goto ok; /* No LEDs. Ignore config. */
			err = m->global_get_leds(m, &leds);
		}
		if (err < 0)
			goto error;
		if (err == 0)
			goto ok; /* No LEDs. Ignore config. */
		for (led = leds; led; led = led->next) {
			if (strcasecmp(led->name, ledname) != 0)
				continue;
			if (!led->toggle_state) {
				razer_free_leds(leds);
				goto invalid;
			}
			err = led->toggle_state(led,
				on ? RAZER_LED_ON : RAZER_LED_OFF);
			razer_free_leds(leds);
			if (err)
				goto error;
			goto ok;
		}
		razer_free_leds(leds);
		goto error;
	} else if (strcasecmp(item, "disabled") == 0) {
		goto ok;
	} else
		goto invalid;
ok:
	return 1;
error:
	*error = 1;
invalid:
	razer_error("Config section \"%s\" item \"%s\" "
		"invalid.\n", section, item);
	return *error ? 0 : 1;
}

static void mouse_apply_initial_config(struct razer_mouse *m)
{
	const char *section = NULL;
	int err;
	bool error = 0;

	config_for_each_section(razer_config_file,
				m, &section,
				mouse_idstr_glob_match);
	if (!section)
		return;
	if (config_get_bool(razer_config_file, section,
			    "disabled", 0, CONF_NOCASE)) {
		razer_debug("Initial config for \"%s\" is disabled. Not applying.\n",
			    m->idstr);
		return;
	}
	razer_debug("Applying config section \"%s\" to \"%s\"\n",
		section, m->idstr);
	err = m->claim(m);
	if (err) {
		razer_error("Failed to claim \"%s\"\n", m->idstr);
		return;
	}
	config_for_each_item(razer_config_file,
			     m, &error,
			     section,
			     mouse_apply_one_config);
	m->release(m);
	if (error) {
		razer_error("Failed to apply initial config "
			"to \"%s\"\n", m->idstr);
	}
}

static struct razer_usb_context * razer_create_usb_ctx(struct libusb_device *dev)
{
	struct razer_usb_context *ctx;

	ctx = zalloc(sizeof(*ctx));
	if (!ctx)
		return NULL;
	ctx->dev = dev;
	ctx->bConfigurationValue = 1;

	return ctx;
}

static int mouse_default_claim(struct razer_mouse *m)
{
	return razer_generic_usb_claim_refcount(m->usb_ctx, &m->claim_count);
}

static int mouse_default_release(struct razer_mouse *m)
{
	int err = 0;

	if (m->claim_count == 1) {
		if (m->commit)
			err = m->commit(m, 0);
	}
	razer_generic_usb_release_refcount(m->usb_ctx, &m->claim_count);

	return err;
}

static struct razer_mouse * mouse_new(const struct razer_usb_device *id,
				      struct libusb_device *udev)
{
	struct razer_event_data ev;
	struct razer_mouse *m;
	int err;

	libusb_ref_device(udev);

	m = zalloc(sizeof(*m));
	if (!m)
		return NULL;
	m->usb_ctx = razer_create_usb_ctx(udev);
	if (!m->usb_ctx)
		goto err_free_mouse;

	/* Set default values and callbacks */
	m->nr_profiles = 1;
	m->claim = mouse_default_claim;
	m->release = mouse_default_release;

	/* Call the driver init */
	m->flags |= RAZER_MOUSEFLG_NEW;
	m->base_ops = id->u.mouse_ops;
	err = m->base_ops->init(m, udev);
	if (err)
		goto err_free_ctx;
	udev = m->usb_ctx->dev;

	if (WARN_ON(m->nr_profiles <= 0))
		goto err_release;
	if (m->nr_profiles == 1 && !m->get_active_profile)
		m->get_active_profile = m->get_profiles;
	if (profile_emu_enabled && m->nr_profiles == 1) {
		err = razer_mouse_init_profile_emulation(m);
		if (err)
			goto err_release;
	}

	mouse_apply_initial_config(m);

	razer_debug("Allocated and initialized new mouse \"%s\"\n",
		m->idstr);

	ev.u.mouse = m;
	razer_notify_event(RAZER_EV_MOUSE_ADD, &ev);

	return m;

err_release:
	m->base_ops->release(m);
err_free_ctx:
	razer_free(m->usb_ctx, sizeof(*(m->usb_ctx)));
err_free_mouse:
	razer_free(m, sizeof(*m));
	libusb_unref_device(udev);

	return NULL;
}

static void razer_free_mouse(struct razer_mouse *m)
{
	struct razer_event_data ev;

	razer_debug("Freeing mouse (type=%d)\n",
		m->base_ops->type);

	ev.u.mouse = m;
	razer_notify_event(RAZER_EV_MOUSE_REMOVE, &ev);

	if (m->release == mouse_default_release) {
		while (m->claim_count)
			m->release(m);
	}
	razer_mouse_exit_profile_emulation(m);
	m->base_ops->release(m);

	libusb_unref_device(m->usb_ctx->dev);

	razer_free(m->usb_ctx, sizeof(*(m->usb_ctx)));
	razer_free(m, sizeof(*m));
}

static void razer_free_mice(struct razer_mouse *mouse_list)
{
	struct razer_mouse *mouse, *next;

	for (mouse = mouse_list; mouse; ) {
		next = mouse->next;
		razer_free_mouse(mouse);
		mouse = next;
	}
}

struct new_razer_usb_device {
	const struct razer_usb_device *id;
	struct usb_device *udev;
};

struct razer_mouse * razer_rescan_mice(void)
{
	struct libusb_device **devlist, *dev;
	ssize_t nr_devices;
	unsigned int i;
	int err;
	struct libusb_device_descriptor desc;
	const struct razer_usb_device *id;
	struct razer_mouse *m, *next;

	nr_devices = libusb_get_device_list(libusb_ctx, &devlist);
	if (nr_devices < 0) {
		razer_error("razer_rescan_mice: Failed to get USB device list\n");
		return NULL;
	}

	for (i = 0; i < nr_devices; i++) {
		dev = devlist[i];
		err = libusb_get_device_descriptor(dev, &desc);
		if (err) {
			razer_error("razer_rescan_mice: Failed to get descriptor\n");
			continue;
		}
		id = usbdev_lookup(&desc);
		if (!id || id->type != RAZER_DEVTYPE_MOUSE)
			continue;
		m = mouse_list_find(mice_list, dev);
		if (m) {
			/* We already had this mouse */
			m->flags |= RAZER_MOUSEFLG_PRESENT;
		} else {
			/* We don't have this mouse, yet. Create a new one */
			m = mouse_new(id, dev);
			if (m) {
				m->flags |= RAZER_MOUSEFLG_PRESENT;
				mouse_list_add(&mice_list, m);
			}
		}
	}
	/* Remove mice that are not connected anymore. */
	razer_for_each_mouse(m, next, mice_list) {
		if (m->flags & RAZER_MOUSEFLG_PRESENT) {
			m->flags &= ~RAZER_MOUSEFLG_PRESENT;
			continue;
		}
		mouse_list_del(&mice_list, m);
		razer_free_mouse(m);
	}

	libusb_free_device_list(devlist, 1);

	return mice_list;
}

int razer_reconfig_mice(void)
{
	struct razer_mouse *m, *next;
	int err;

	razer_for_each_mouse(m, next, mice_list) {
		err = m->claim(m);
		if (err)
			return err;
		if (m->commit)
			err = m->commit(m, 1);
		m->release(m);
		if (err)
			return err;
	}

	return 0;
}

void razer_free_freq_list(enum razer_mouse_freq *freq_list, int count)
{
	if (freq_list)
		free(freq_list);
}

void razer_free_resolution_list(enum razer_mouse_res *res_list, int count)
{
	if (res_list)
		free(res_list);
}

void razer_free_leds(struct razer_led *led_list)
{
	struct razer_led *led, *next;

	for (led = led_list; led; ) {
		next = led->next;
		free(led);
		led = next;
	}
}

int razer_init(int enable_profile_emu)
{
	int err = 0;

	if (!razer_initialized())
		err = libusb_init(&libusb_ctx);
	if (!err)
		profile_emu_enabled = enable_profile_emu;

	return err ? -EINVAL : 0;
}

void razer_exit(void)
{
	if (!razer_initialized())
		return;
	razer_free_mice(mice_list);
	mice_list = NULL;
	config_file_free(razer_config_file);
	razer_config_file = NULL;

	libusb_exit(libusb_ctx);
	libusb_ctx = NULL;
}

int razer_usb_add_used_interface(struct razer_usb_context *ctx,
				 int bInterfaceNumber,
				 int bAlternateSetting)
{
	struct razer_usb_interface *interf;

	if (ctx->nr_interfaces >= ARRAY_SIZE(ctx->interfaces)) {
		razer_error("USB context interface array overflow\n");
		return -ENOSPC;
	}

	interf = &ctx->interfaces[ctx->nr_interfaces];
	interf->bInterfaceNumber = bInterfaceNumber;
	interf->bAlternateSetting = bAlternateSetting;
	ctx->nr_interfaces++;

	return 0;
}

static void razer_reattach_usb_kdrv(struct razer_usb_context *ctx,
				    int bInterfaceNumber)
{
	int res;

	res = libusb_kernel_driver_active(ctx->h, bInterfaceNumber);
	if (res == 1)
		return;
	if (res) {
		razer_error("Failed to get kernel driver state\n");
		return;
	}

	res = libusb_attach_kernel_driver(ctx->h, bInterfaceNumber);
	if (res) {
		razer_error("Failed to reconnect the kernel driver (%d).\n"
			"The device most likely won't work now. Try to replug it.\n", res);
		return;
	}
}

static void razer_usb_release(struct razer_usb_context *ctx,
			      int bInterfaceNumber)
{
	libusb_release_interface(ctx->h, bInterfaceNumber);
	razer_reattach_usb_kdrv(ctx, bInterfaceNumber);
}

int razer_generic_usb_claim(struct razer_usb_context *ctx)
{
	unsigned int tries;
	int err, i, config;
	struct razer_usb_interface *interf;

	err = libusb_open(ctx->dev, &ctx->h);
	if (err) {
		razer_error("razer_generic_usb_claim: Failed to open USB device\n");
		return -ENODEV;
	}

	/* Detach kernel drivers for all interfaces. */
	for (i = 0; i < ctx->nr_interfaces; i++) {
		interf = &ctx->interfaces[i];
		err = libusb_kernel_driver_active(ctx->h, interf->bInterfaceNumber);
		if (err == 1) {
			err = libusb_detach_kernel_driver(ctx->h, interf->bInterfaceNumber);
			if (err) {
				err = -EBUSY;
				razer_error("Failed to detach kernel driver\n");
				goto err_close;
			}
		} else if (err) {
			err = -ENODEV;
			razer_error("Failed to get kernel driver state\n");
			goto err_close;
		}
	}

	tries = 0;
	while (1) {
		if (tries >= 10) {
			razer_error("razer_generic_usb_claim: Failed to claim config\n");
			goto err_close;
		}

		/* Select the correct configuration */
		err = libusb_get_configuration(ctx->h, &config);
		if (err) {
			err = -EBUSY;
			razer_error("razer_generic_usb_claim: Failed to get configuration\n");
			goto err_close;
		}
		if (config != ctx->bConfigurationValue) {
			err = libusb_set_configuration(ctx->h, ctx->bConfigurationValue);
			if (err) {
				err = -EBUSY;
				razer_error("razer_generic_usb_claim: Failed to set configuration\n");
				goto err_close;
			}
		}

		/* And finally claim all interfaces. */
		for (i = 0; i < ctx->nr_interfaces; i++) {
			interf = &ctx->interfaces[i];
			err = libusb_claim_interface(ctx->h, interf->bInterfaceNumber);
			if (err) {
				err = -EIO;
				razer_error("Failed to claim USB interface\n");
				goto err_close;
			}
			err = libusb_set_interface_alt_setting(ctx->h, interf->bInterfaceNumber,
							       interf->bAlternateSetting);
			if (err) {
				err = -EIO;
				goto err_close;
			}
		}

		/* To make sure there was no race, check config value again */
		err = libusb_get_configuration(ctx->h, &config);
		if (err) {
			err = -EBUSY;
			razer_error("razer_generic_usb_claim: Failed to get configuration\n");
			goto err_close;
		}
		if (config == ctx->bConfigurationValue)
			break;
		razer_msleep(100);
		tries++;
	}

	return 0;

err_close:
	libusb_close(ctx->h);

	return err;
}

int razer_generic_usb_claim_refcount(struct razer_usb_context *ctx,
				     unsigned int *refcount)
{
	int err;

	if (!(*refcount)) {
		err = razer_generic_usb_claim(ctx);
		if (err)
			return err;
	}
	(*refcount)++;

	return 0;
}

void razer_generic_usb_release(struct razer_usb_context *ctx)
{
	int i;

	for (i = ctx->nr_interfaces - 1; i >= 0; i--)
		razer_usb_release(ctx, ctx->interfaces[i].bInterfaceNumber);
	libusb_close(ctx->h);
}

void razer_generic_usb_release_refcount(struct razer_usb_context *ctx,
					unsigned int *refcount)
{
	if (*refcount) {
		(*refcount)--;
		if (!(*refcount))
			razer_generic_usb_release(ctx);
	}
}

void razer_generic_usb_gen_idstr(struct libusb_device *udev,
				 struct libusb_device_handle *h,
				 const char *devname,
				 bool include_devicenr,
				 const char *serial,
				 char *idstr_buf)
{
	char devid[64];
	char serial_buf[64];
	char buspos[512];
	unsigned int serial_index = 0;
	int err;
	struct libusb_device_descriptor devdesc;
	struct razer_usb_context usbctx = {
		.dev = udev,
		.h = h,
	};

	err = libusb_get_device_descriptor(udev, &devdesc);
	if (err) {
		razer_error("razer_generic_usb_gen_idstr: Failed to get "
			"device descriptor (%d)\n", err);
		return;
	}

	if (!serial || !strlen(serial)) {
		serial_index = devdesc.iSerialNumber;
		err = -EINVAL;
		if (serial_index) {
			err = 0;
			if (!h)
				err = razer_generic_usb_claim(&usbctx);
			if (err) {
				razer_error("Failed to claim device for serial fetching.\n");
			} else {
				err = libusb_get_string_descriptor_ascii(
					usbctx.h, serial_index,
					(unsigned char *)serial_buf, sizeof(serial_buf));
				if (!h)
					razer_generic_usb_release(&usbctx);
			}
		}
		if (err <= 0)
			strcpy(serial_buf, "0");
		serial = serial_buf;
	}

	snprintf(devid, sizeof(devid), "%04X-%04X-%s",
		 devdesc.idVendor,
		 devdesc.idProduct, serial);
	if (include_devicenr) {
		snprintf(buspos, sizeof(buspos), "%03d-%03d",
			 libusb_get_bus_number(udev),
			 libusb_get_device_address(udev));
	} else {
		snprintf(buspos, sizeof(buspos), "%03d",
			 libusb_get_bus_number(udev));
	}

	razer_create_idstr(idstr_buf, BUSTYPESTR_USB, buspos,
			   DEVTYPESTR_MOUSE, devname, devid);
}

/** razer_usb_force_hub_reset
 * Force reset of the hub the specified device is on
 */
int razer_usb_force_hub_reset(struct razer_usb_context *device_ctx)
{
	struct libusb_device_handle *h;
	struct libusb_device *hub = NULL, *dev;
	uint8_t hub_bus_number, hub_device_address;
	struct razer_usb_reconnect_guard rg;
	int err;
	struct libusb_device **devlist;
	ssize_t devlist_size, i;

	razer_debug("Forcing hub reset for device %03u:%03u\n",
		libusb_get_bus_number(device_ctx->dev),
		libusb_get_device_address(device_ctx->dev));

	razer_usb_reconnect_guard_init(&rg, device_ctx);

	hub_bus_number = libusb_get_bus_number(device_ctx->dev);
	hub_device_address = 1; /* Constant */

	devlist_size = libusb_get_device_list(libusb_ctx, &devlist);
	for (i = 0; i < devlist_size; i++) {
		dev = devlist[i];
		if (libusb_get_bus_number(dev) == hub_bus_number &&
		    libusb_get_device_address(dev) == hub_device_address) {
			hub = dev;
			break;
		}
	}
	if (!hub) {
		razer_error("razer_usb_force_reinit: Failed to find hub\n");
		err = -ENODEV;
		goto error;
	}
	razer_debug("Resetting root hub %03u:%03u\n",
		hub_bus_number, hub_device_address);

	err = libusb_open(hub, &h);
	if (err) {
		razer_error("razer_usb_force_reinit: Failed to open hub device\n");
		err = -ENODEV;
		goto error;
	}
	libusb_reset_device(h);
	libusb_close(h);

	err = razer_usb_reconnect_guard_wait(&rg, 1);
	if (err) {
		razer_error("razer_usb_force_reinit: "
			"Failed to discover the reconnected device\n");
		goto error;
	}
	razer_debug("Hub reset completed. Device rediscovered as %03u:%03u\n",
		libusb_get_bus_number(device_ctx->dev),
		libusb_get_device_address(device_ctx->dev));

	err = 0;
error:
	libusb_free_device_list(devlist, 1);

	return err;
}

/** razer_usb_reconnect_guard_init - Init the reconnect-guard context
 *
 * Call this _before_ triggering any device operations that might
 * reset the device.
 */
int razer_usb_reconnect_guard_init(struct razer_usb_reconnect_guard *guard,
				   struct razer_usb_context *ctx)
{
	int err;

	guard->ctx = ctx;
	err = libusb_get_device_descriptor(ctx->dev, &guard->old_desc);
	if (err) {
		razer_error("razer_usb_reconnect_guard_init: Failed to "
			"get device descriptor\n");
		return err;
	}
	guard->old_busnr = libusb_get_bus_number(ctx->dev);
	guard->old_devaddr = libusb_get_device_address(ctx->dev);

	return 0;
}

static struct libusb_device * guard_find_usb_dev(const struct libusb_device_descriptor *expected_desc,
						 uint8_t expected_bus_number,
						 uint8_t expected_dev_addr,
						 bool exact_match)
{
	struct libusb_device **devlist, *dev;
	struct libusb_device_descriptor desc;
	uint8_t dev_addr;
	ssize_t nr_devices, i, j;
	int err;

	nr_devices = libusb_get_device_list(libusb_ctx, &devlist);
	if (nr_devices < 0) {
		razer_error("guard_find_usb_dev: Failed to get device list\n");
		return NULL;
	}

	for (i = 0; i < nr_devices; i++) {
		dev = devlist[i];
		if (libusb_get_bus_number(dev) != expected_bus_number)
			continue;
		err = libusb_get_device_descriptor(dev, &desc);
		if (err)
			continue;
		if (memcmp(&desc, expected_desc, sizeof(desc)) != 0)
			continue;
		dev_addr = libusb_get_device_address(dev);
		if (exact_match) {
			if (dev_addr == expected_dev_addr)
				goto found_dev;
		} else {
			for (j = 0; j < 64; j++) {
				if (dev_addr == ((expected_dev_addr + j) & 0x7F))
					goto found_dev;
			}
		}
	}
	libusb_free_device_list(devlist, 1);

	return NULL;

found_dev:
	libusb_ref_device(dev);
	libusb_free_device_list(devlist, 1);

	return dev;
}

/** razer_usb_reconnect_guard_wait - Protect against a firmware reconnect.
 *
 * If the firmware does a reconnect of the device on the USB bus, this
 * function tries to keep track of the device and it will update the
 * usb context information.
 * Of course, this is not completely race-free, but we try to do our best.
 *
 * hub_reset is true, if the device reconnects due to a HUB reset event.
 * Otherwise it's assumed that the device reconnects on behalf of itself.
 * If hub_reset is false, the device is expected to be claimed.
 */
int razer_usb_reconnect_guard_wait(struct razer_usb_reconnect_guard *guard, bool hub_reset)
{
	uint8_t reconn_dev_addr;
	uint8_t old_dev_addr = guard->old_devaddr;
	uint8_t old_bus_number = guard->old_busnr;
	int res, errorcode = 0;
	struct libusb_device *dev;
	struct timeval now, timeout;

	if (!hub_reset) {
		/* Release the device, so the kernel can detect the bus reconnect. */
		razer_generic_usb_release(guard->ctx);
	}

	/* Wait for the device to disconnect. */
	gettimeofday(&timeout, NULL);
	razer_timeval_add_msec(&timeout, 3000);
	while (1) {
		dev = guard_find_usb_dev(&guard->old_desc,
				old_bus_number, old_dev_addr, 1);
		if (!dev)
			break;
		libusb_unref_device(dev);
		gettimeofday(&now, NULL);
		if (razer_timeval_after(&now, &timeout)) {
			/* Timeout. Hm. It seems the device won't reconnect.
			 * That's probably OK. Reclaim it. */
			razer_error("razer_usb_reconnect_guard: "
				"The device did not disconnect! If it "
				"does not work anymore, try to replug it.\n");
			goto reclaim;
		}
		razer_msleep(50);
	}

	/* Construct the device address it will reconnect on.
	 * On a device reset the new dev addr will be >= reconn_dev_addr.
	 */
	reconn_dev_addr = (old_dev_addr + 1) & 0x7F;

	/* Wait for the device to reconnect. */
	gettimeofday(&timeout, NULL);
	razer_timeval_add_msec(&timeout, 3000);
	while (1) {
		dev = guard_find_usb_dev(&guard->old_desc,
				old_bus_number, reconn_dev_addr, 0);
		if (dev)
			break;
		gettimeofday(&now, NULL);
		if (razer_timeval_after(&now, &timeout)) {
			razer_error("razer_usb_reconnect_guard: The device did not "
				"reconnect! It might not work anymore. Try to replug it.\n");
			razer_debug("Expected reconnect busid was: %02u:>=%03u\n",
				old_dev_addr, reconn_dev_addr);
			errorcode = -EBUSY;
			goto out;
		}
		razer_msleep(50);
	}

	/* Update the USB context. */
	libusb_unref_device(guard->ctx->dev);
	guard->ctx->dev = dev;

reclaim:
	if (!hub_reset) {
		/* Reclaim the new device. */
		res = razer_generic_usb_claim(guard->ctx);
		if (res) {
			razer_error("razer_usb_reconnect_guard: Reclaim failed.\n");
			return res;
		}
	}
out:
	return errorcode;
}

int razer_load_config(const char *path)
{
	struct config_file *conf = NULL;

	if (!razer_initialized())
		return -EINVAL;

	if (!path)
		path = RAZER_DEFAULT_CONFIG;
	if (strlen(path)) {
		conf = config_file_parse(path, 1);
		if (!conf)
			return -ENOENT;
	}
	config_file_free(razer_config_file);
	razer_config_file = conf;

	return 0;
}

void razer_set_logging(razer_logfunc_t info_callback,
		       razer_logfunc_t error_callback,
		       razer_logfunc_t debug_callback)
{
	razer_logfunc_info = info_callback;
	razer_logfunc_error = error_callback;
	razer_logfunc_debug = debug_callback;
}

static void do_init_axis(struct razer_axis *axis,
			 unsigned int id, const char *name, unsigned int flags)
{
	if (name) {
		axis->id = id;
		axis->name = name;
		axis->flags = flags;
	}
}

void razer_init_axes(struct razer_axis *axes,
		     const char *name0, unsigned int flags0,
		     const char *name1, unsigned int flags1,
		     const char *name2, unsigned int flags2)
{
	do_init_axis(&axes[0], 0, name0, flags0);
	do_init_axis(&axes[1], 1, name1, flags1);
	do_init_axis(&axes[2], 2, name2, flags2);
}

struct razer_mouse_dpimapping * razer_mouse_get_dpimapping_by_res(
		struct razer_mouse_dpimapping *mappings, size_t nr_mappings,
		enum razer_dimension dim, enum razer_mouse_res res)
{
	struct razer_mouse_dpimapping *mapping = NULL;
	size_t i;

	for (i = 0; i < nr_mappings; i++) {
		if (mappings[i].res[dim] == res) {
			mapping = &mappings[i];
			break;
		}
	}

	return mapping;
}

void razer_event_spacing_init(struct razer_event_spacing *es,
			      unsigned int msec)
{
	memset(es, 0, sizeof(*es));
	es->spacing_msec = msec;
}

void razer_event_spacing_enter(struct razer_event_spacing *es)
{
	struct timeval now, deadline;
	int wait_msec;

	gettimeofday(&now, NULL);
	deadline = es->last_event;
	razer_timeval_add_msec(&deadline, es->spacing_msec);

	if (razer_timeval_after(&deadline, &now)) {
		/* We have to sleep long enough to ensure we're
		 * after the deadline. */
		wait_msec = razer_timeval_msec_diff(&deadline, &now);
		WARN_ON(wait_msec < 0);
		razer_msleep(wait_msec + 1);
		gettimeofday(&now, NULL);
		razer_error_on(razer_timeval_after(&deadline, &now),
			       "Failed to maintain event spacing\n");
	}
}

void razer_event_spacing_leave(struct razer_event_spacing *es)
{
	gettimeofday(&es->last_event, NULL);
}
