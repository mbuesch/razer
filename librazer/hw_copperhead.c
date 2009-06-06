/*
 *   Lowlevel hardware access for the
 *   Razer Deathadder mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering only.
 *
 *   Copyright (C) 2009 Michael Buesch <mb@bu3sch.de>
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

#include "hw_copperhead.h"
#include "razer_private.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>


enum { /* Misc constants */
	COPPERHEAD_NR_PROFILES		= 5,
	COPPERHEAD_NR_DPIMAPPINGS	= 4,
};

struct copperhead_private {
	unsigned int claimed;
	uint16_t fw_version;
	struct razer_usb_context usb;

	/* The active profile. */
	struct razer_mouse_profile *cur_profile;
	/* Profile configuration (one per profile). */
	struct razer_mouse_profile profiles[COPPERHEAD_NR_PROFILES];

	/* The active DPI mapping; per profile. */
	struct razer_mouse_dpimapping *cur_dpimapping[COPPERHEAD_NR_PROFILES];
	/* The possible DPI mappings. */
	struct razer_mouse_dpimapping dpimappings[COPPERHEAD_NR_DPIMAPPINGS];

	/* The active scan frequency; per profile. */
	enum razer_mouse_freq cur_freq[COPPERHEAD_NR_PROFILES];
};

#define COPPERHEAD_USB_TIMEOUT		3000

static int copperhead_usb_write(struct copperhead_private *priv,
				int request, int command,
				const void *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      (char *)buf, size,
			      COPPERHEAD_USB_TIMEOUT);
	if (err != size) {
		fprintf(stderr, "razer-copperhead: "
			"USB write 0x%02X 0x%02X failed: %d\n",
			request, command, err);
		return err;
	}
	return 0;
}

static int copperhead_usb_read(struct copperhead_private *priv,
			       int request, int command,
			       void *buf, size_t size)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      COPPERHEAD_USB_TIMEOUT);
	if (err != size) {
		fprintf(stderr, "razer-copperhead: "
			"USB read 0x%02X 0x%02X failed: %d\n",
			request, command, err);
		return err;
	}
	return 0;
}

static int copperhead_read_fw_ver(struct copperhead_private *priv)
{
	char buf[2];
	uint16_t ver;
	int err;

//FIXME this is wrong
	err = copperhead_usb_read(priv, USB_REQ_CLEAR_FEATURE,
				  0x05, buf, sizeof(buf));
	if (err)
		return err;
	ver = buf[0];
	ver <<= 8;
	ver |= buf[1];

	return ver;
}

static int copperhead_commit(struct copperhead_private *priv)
{
	//TODO
	return 0;
}

static int copperhead_claim(struct razer_mouse *m)
{
	struct copperhead_private *priv = m->internal;
	int err, fwver;

	if (!priv->claimed) {
		err = razer_generic_usb_claim(&priv->usb);
		if (err)
			return err;
		fwver = copperhead_read_fw_ver(priv);
		if (fwver < 0)
			return fwver;
		priv->fw_version = fwver;
	}
	priv->claimed++;

	return 0;
}

static void copperhead_release(struct razer_mouse *m)
{
	struct copperhead_private *priv = m->internal;

	priv->claimed--;
	if (!priv->claimed)
		razer_generic_usb_release(&priv->usb);
}

static int copperhead_get_fw_version(struct razer_mouse *m)
{
	struct copperhead_private *priv = m->internal;

	/* Version is read on claim. */
	if (!priv->claimed)
		return -EBUSY;
	return priv->fw_version;
}

void razer_copperhead_gen_idstr(struct usb_device *udev, char *buf)
{
	char devid[64];
	char buspos[1024];

	snprintf(devid, sizeof(devid), "%04X-%04X-%02X",
		 udev->descriptor.idVendor,
		 udev->descriptor.idProduct,
		 udev->descriptor.iSerialNumber);
	snprintf(buspos, sizeof(buspos), "%s-%s",
		 udev->bus->dirname, udev->filename);
	razer_create_idstr(buf, BUSTYPESTR_USB, buspos,
			   DEVTYPESTR_MOUSE, "Copperhead", devid);
}

void razer_copperhead_assign_usb_device(struct razer_mouse *m,
					struct usb_device *usbdev)
{
	struct copperhead_private *priv = m->internal;

	priv->usb.dev = usbdev;
}

static struct razer_mouse_profile * copperhead_get_profiles(struct razer_mouse *m)
{
	struct copperhead_private *priv = m->internal;

	return &priv->profiles[0];
}

static struct razer_mouse_profile * copperhead_get_active_profile(struct razer_mouse *m)
{
	struct copperhead_private *priv = m->internal;

	return priv->cur_profile;
}

static int copperhead_supported_resolutions(struct razer_mouse *m,
					    enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	const int count = 4;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	list[0] = RAZER_MOUSE_RES_400DPI;
	list[1] = RAZER_MOUSE_RES_800DPI;
	list[2] = RAZER_MOUSE_RES_1600DPI;
	list[3] = RAZER_MOUSE_RES_2000DPI;

	*res_list = list;

	return count;
}

static int copperhead_supported_freqs(struct razer_mouse *m,
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

static enum razer_mouse_freq copperhead_get_freq(struct razer_mouse_profile *p)
{
	struct copperhead_private *priv = p->mouse->internal;

	if (p->nr >= ARRAY_SIZE(priv->cur_freq))
		return -EINVAL;

	return priv->cur_freq[p->nr];
}

static int copperhead_set_freq(struct razer_mouse_profile *p,
			       enum razer_mouse_freq freq)
{
	struct copperhead_private *priv = p->mouse->internal;
	enum razer_mouse_freq oldfreq;
	int err;

	if (!priv->claimed)
		return -EBUSY;
	if (p->nr >= ARRAY_SIZE(priv->cur_freq))
		return -EINVAL;

	oldfreq = priv->cur_freq[p->nr];
	priv->cur_freq[p->nr] = freq;

	err = copperhead_commit(priv);
	if (err) {
		priv->cur_freq[p->nr] = oldfreq;
		return err;
	}

	return 0;
}

static int copperhead_supported_dpimappings(struct razer_mouse *m,
					    struct razer_mouse_dpimapping **res_ptr)
{
	struct copperhead_private *priv = m->internal;

	*res_ptr = &priv->dpimappings[0];

	return ARRAY_SIZE(priv->dpimappings);
}

static struct razer_mouse_dpimapping * copperhead_get_dpimapping(struct razer_mouse_profile *p)
{
	struct copperhead_private *priv = p->mouse->internal;

	if (p->nr >= ARRAY_SIZE(priv->cur_dpimapping))
		return NULL;

	return priv->cur_dpimapping[p->nr];
}

static int copperhead_set_dpimapping(struct razer_mouse_profile *p,
				     struct razer_mouse_dpimapping *d)
{
	struct copperhead_private *priv = p->mouse->internal;
	struct razer_mouse_dpimapping *oldmapping;
	int err;

	if (!priv->claimed)
		return -EBUSY;
	if (p->nr >= ARRAY_SIZE(priv->cur_dpimapping))
		return -EINVAL;

	oldmapping = priv->cur_dpimapping[p->nr];
	priv->cur_dpimapping[p->nr] = d;

	err = copperhead_commit(priv);
	if (err) {
		priv->cur_dpimapping[p->nr] = oldmapping;
		return err;
	}

	return err;
}

int razer_copperhead_init_struct(struct razer_mouse *m,
				 struct usb_device *usbdev)
{
	struct copperhead_private *priv;
	unsigned int i;

	priv = malloc(sizeof(struct copperhead_private));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));
	m->internal = priv;

	razer_copperhead_assign_usb_device(m, usbdev);

	priv->dpimappings[0].nr = 0;
	priv->dpimappings[0].res = RAZER_MOUSE_RES_400DPI;
	priv->dpimappings[0].mouse = m;

	priv->dpimappings[1].nr = 1;
	priv->dpimappings[1].res = RAZER_MOUSE_RES_800DPI;
	priv->dpimappings[1].mouse = m;

	priv->dpimappings[2].nr = 2;
	priv->dpimappings[2].res = RAZER_MOUSE_RES_1600DPI;
	priv->dpimappings[2].mouse = m;

	priv->dpimappings[3].nr = 3;
	priv->dpimappings[3].res = RAZER_MOUSE_RES_2000DPI;
	priv->dpimappings[3].mouse = m;

	for (i = 0; i < COPPERHEAD_NR_PROFILES; i++) {
		priv->profiles[i].nr = i;
		priv->profiles[i].get_freq = copperhead_get_freq;
		priv->profiles[i].set_freq = copperhead_set_freq;
		priv->profiles[i].get_dpimapping = copperhead_get_dpimapping;
		priv->profiles[i].set_dpimapping = copperhead_set_dpimapping;
		priv->profiles[i].mouse = m;
	}

	for (i = 0; i < COPPERHEAD_NR_PROFILES; i++)
		priv->cur_freq[i] = RAZER_MOUSE_FREQ_1000HZ; //FIXME

	m->type = RAZER_MOUSETYPE_COPPERHEAD;
	razer_copperhead_gen_idstr(usbdev, m->idstr);

	m->claim = copperhead_claim;
	m->release = copperhead_release;
	m->get_fw_version = copperhead_get_fw_version;
	m->nr_profiles = 5;
	m->get_profiles = copperhead_get_profiles;
	m->get_active_profile = copperhead_get_active_profile;
	m->supported_resolutions = copperhead_supported_resolutions;
	m->supported_freqs = copperhead_supported_freqs;
	m->supported_dpimappings = copperhead_supported_dpimappings;

	return 0;
}

void razer_copperhead_release(struct razer_mouse *m)
{
	struct copperhead_private *priv = m->internal;

	copperhead_release(m);
	free(priv);
}
