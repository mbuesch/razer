/*
 *   Lowlevel hardware access for the
 *   Razer Deathadder mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering only.
 *
 *   Copyright (C) 2009-2010 Michael Buesch <mb@bu3sch.de>
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


enum { /* Misc constants */
	COPPERHEAD_NR_PROFILES		= 1,
	COPPERHEAD_NR_DPIMAPPINGS	= 4,
};

/* The wire protocol data structures... */

enum copperhead_phys_button {
	/* Physical button IDs */
	COPPERHEAD_PHYSBUT_LEFT = 0x01,	/* Left button */
	COPPERHEAD_PHYSBUT_RIGHT,	/* Right button */
	COPPERHEAD_PHYSBUT_MIDDLE,	/* Middle button */
	COPPERHEAD_PHYSBUT_LFRONT,	/* Left side, front button */
	COPPERHEAD_PHYSBUT_LREAR,	/* Left side, rear button */
	COPPERHEAD_PHYSBUT_RFRONT,	/* Right side, front button */
	COPPERHEAD_PHYSBUT_RREAR,	/* Right side, rear button */

	NR_COPPERHEAD_PHYSBUT = 7,	/* Number of physical buttons */
};
#define copperhead_for_each_physbut(iterator) \
	for (iterator = 0x01; iterator <= NR_COPPERHEAD_PHYSBUT; iterator++)

enum copperhead_button_function {
	/* Logical button function IDs */
	COPPERHEAD_BUTFUNC_LEFT		= 0x01, /* Left button */
	COPPERHEAD_BUTFUNC_RIGHT	= 0x02, /* Right button */
	COPPERHEAD_BUTFUNC_MIDDLE	= 0x03, /* Middle button */
	COPPERHEAD_BUTFUNC_DPIUP	= 0x0C, /* DPI down */
	COPPERHEAD_BUTFUNC_DPIDOWN	= 0x0D, /* DPI down */
	COPPERHEAD_BUTFUNC_WIN5		= 0x0A, /* Windows button 5 */
	COPPERHEAD_BUTFUNC_WIN4		= 0x0B, /* Windows button 4 */
};

struct copperhead_one_buttonmapping {
	uint8_t physical;
	uint8_t logical;
} _packed;

struct copperhead_buttonmappings {
	struct copperhead_one_buttonmapping left;
	uint8_t _padding0[46];
	struct copperhead_one_buttonmapping right;
	uint8_t _padding1[46];
	struct copperhead_one_buttonmapping middle;
	uint8_t _padding2[46];
	struct copperhead_one_buttonmapping lfront;
	uint8_t _padding3[46];
	struct copperhead_one_buttonmapping lrear;
	uint8_t _padding4[46];
	struct copperhead_one_buttonmapping rfront;
	uint8_t _padding5[46];
	struct copperhead_one_buttonmapping rrear;
	uint8_t _padding6[42];
} _packed;

struct copperhead_profcfg_cmd {
	le16_t packetlength;
	le32_t magic0;
	le32_t _padding;
	le16_t magic1;
	uint8_t dpisel;
	uint8_t freq;
	struct copperhead_buttonmappings buttons;
	le16_t checksum;
} _packed;
#define COPPERHEAD_PROFCFG_MAGIC0	cpu_to_le32(0x00010002)
#define COPPERHEAD_PROFCFG_MAGIC1	cpu_to_le16(0x0001)

struct copperhead_private {
	struct razer_mouse *m;

	uint16_t fw_version;

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

	/* The active button mapping; per profile. */
	struct copperhead_buttonmappings buttons[COPPERHEAD_NR_PROFILES];
};

/* A list of physical buttons on the device. */
static struct razer_button copperhead_physical_buttons[] = {
	{ .id = COPPERHEAD_PHYSBUT_LEFT,	.name = "Leftclick",		},
	{ .id = COPPERHEAD_PHYSBUT_RIGHT,	.name = "Rightclick",		},
	{ .id = COPPERHEAD_PHYSBUT_MIDDLE,	.name = "Middleclick",		},
	{ .id = COPPERHEAD_PHYSBUT_LFRONT,	.name = "Leftside front",	},
	{ .id = COPPERHEAD_PHYSBUT_LREAR,	.name = "Leftside rear",	},
	{ .id = COPPERHEAD_PHYSBUT_RFRONT,	.name = "Rightside front",	},
	{ .id = COPPERHEAD_PHYSBUT_RREAR,	.name = "Rightside rear",	},
};

/* A list of possible button functions. */
static struct razer_button_function copperhead_button_functions[] = {
	{ .id = COPPERHEAD_BUTFUNC_LEFT,	.name = "Leftclick",		},
	{ .id = COPPERHEAD_BUTFUNC_RIGHT,	.name = "Rightclick",		},
	{ .id = COPPERHEAD_BUTFUNC_MIDDLE,	.name = "Middleclick",		},
	{ .id = COPPERHEAD_BUTFUNC_DPIUP,	.name = "DPI switch up",	},
	{ .id = COPPERHEAD_BUTFUNC_DPIDOWN,	.name = "DPI switch down",	},
	{ .id = COPPERHEAD_BUTFUNC_WIN5,	.name = "Windows Button 5",	},
	{ .id = COPPERHEAD_BUTFUNC_WIN4,	.name = "Windows Button 4",	},
};
/* TODO: There are more functions */

#define DEFINE_DEF_BUTMAP(mappingptr, phys, func)			\
	.mappingptr = { .physical = COPPERHEAD_PHYSBUT_##phys,		\
			.logical = COPPERHEAD_BUTFUNC_##func,		\
	}
static const struct copperhead_buttonmappings copperhead_default_buttonmap = {
	DEFINE_DEF_BUTMAP(left, LEFT, LEFT),
	DEFINE_DEF_BUTMAP(right, RIGHT, RIGHT),
	DEFINE_DEF_BUTMAP(middle, MIDDLE, MIDDLE),
	DEFINE_DEF_BUTMAP(lfront, LFRONT, WIN5),
	DEFINE_DEF_BUTMAP(lrear, LREAR, WIN4),
	DEFINE_DEF_BUTMAP(rfront, RFRONT, DPIUP),
	DEFINE_DEF_BUTMAP(rrear, RREAR, DPIDOWN),
};


#define COPPERHEAD_USB_TIMEOUT		3000

static struct copperhead_one_buttonmapping *
	copperhead_buttonid_to_mapping(struct copperhead_buttonmappings *mappings,
				       enum copperhead_phys_button id)
{
	switch (id) {
	case COPPERHEAD_PHYSBUT_LEFT:
		return &mappings->left;
	case COPPERHEAD_PHYSBUT_RIGHT:
		return &mappings->right;
	case COPPERHEAD_PHYSBUT_MIDDLE:
		return &mappings->middle;
	case COPPERHEAD_PHYSBUT_LFRONT:
		return &mappings->lfront;
	case COPPERHEAD_PHYSBUT_LREAR:
		return &mappings->lrear;
	case COPPERHEAD_PHYSBUT_RFRONT:
		return &mappings->rfront;
	case COPPERHEAD_PHYSBUT_RREAR:
		return &mappings->rrear;
	}
	return NULL;
}

static int copperhead_usb_write(struct copperhead_private *priv,
				int request, int command, int index,
				const void *buf, size_t size)
{
	int err;

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, index,
		(unsigned char *)buf, size, COPPERHEAD_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-copperhead: "
			"USB write 0x%02X 0x%02X 0x%02X failed: %d\n",
			request, command, index, err);
		return -EIO;
	}

	return 0;
}

static int copperhead_usb_read(struct copperhead_private *priv,
			       int request, int command, int index,
			       void *buf, size_t size)
{
	int err;

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, index,
		(unsigned char *)buf, size, COPPERHEAD_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-copperhead: "
			"USB read 0x%02X 0x%02X 0x%02X failed: %d\n",
			request, command, index, err);
		return -EIO;
	}

	return 0;
}

static int copperhead_read_fw_ver(struct copperhead_private *priv)
{
	char buf[2];
	uint16_t ver;
//	int err;

//FIXME this is wrong
//	err = copperhead_usb_read(priv, LIBUSB_REQUEST_CLEAR_FEATURE,
//				  0x05, 0, buf, sizeof(buf));
buf[0]=0;
buf[1]=0;
//	if (err)
//		return err;
	ver = buf[0];
	ver <<= 8;
	ver |= buf[1];

	return ver;
}

static int copperhead_commit(struct copperhead_private *priv)
{
	union {
		struct copperhead_profcfg_cmd profcfg;
		uint8_t chunks[64 * 6];
	} _packed u;
	uint8_t *chunk;
	unsigned int i;
	int err;
	unsigned char value;

	BUILD_BUG_ON(sizeof(u) != 0x180);

	/* Select the profile */
	value = 1;
	err = copperhead_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
				   0x02, 1, &value, sizeof(value));
	if (err)
		return err;

	/* Upload the profile config */
	memset(&u, 0, sizeof(u));
	u.profcfg.packetlength = cpu_to_le16(sizeof(u.profcfg));
	u.profcfg.magic0 = COPPERHEAD_PROFCFG_MAGIC0;
	u.profcfg.magic1 = COPPERHEAD_PROFCFG_MAGIC1;
	switch (priv->cur_dpimapping[0]->res) {
	default:
	case RAZER_MOUSE_RES_400DPI:
		u.profcfg.dpisel = 4;
		break;
	case RAZER_MOUSE_RES_800DPI:
		u.profcfg.dpisel = 3;
		break;
	case RAZER_MOUSE_RES_1600DPI:
		u.profcfg.dpisel = 2;
		break;
	case RAZER_MOUSE_RES_2000DPI:
		u.profcfg.dpisel = 1;
		break;
	}
	switch (priv->cur_freq[0]) {
	default:
	case RAZER_MOUSE_FREQ_125HZ:
		u.profcfg.freq = 3;
		break;
	case RAZER_MOUSE_FREQ_500HZ:
		u.profcfg.freq = 2;
		break;
	case RAZER_MOUSE_FREQ_1000HZ:
		u.profcfg.freq = 1;
		break;
	}
	u.profcfg.buttons = priv->buttons[0];
	u.profcfg.checksum = razer_xor16_checksum(&u.profcfg,
		sizeof(u.profcfg) - 2);
razer_dump("profcfg", &u.profcfg, sizeof(u.profcfg));
	/* The profile config is committed in 64byte chunks */
	chunk = &u.chunks[0];
	for (i = 0; i < 6; i++, chunk += 64) {
		err = copperhead_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
					   i + 1, 0, chunk, 64);
//razer_dump("chunk", chunk, 64);
		if (err)
			return err;
	}
#if 0
	/* 2109 0200 0300 0100 01 */
	copperhead_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
			     0x02, 3, buf, 1);
#endif
	memset(&u, 0, sizeof(u));
	err = copperhead_usb_read(priv, LIBUSB_REQUEST_CLEAR_FEATURE,
				  0x01, 0, &u.chunks[0], 0x156);
	if (err)
		return err;
	razer_dump("reply", &u.chunks[0], 0x156);

	return 0;
}

static int copperhead_read_config_from_hw(struct copperhead_private *priv)
{
	unsigned int i;
	unsigned char value;
	int err;

	/* Assign sane defaults. */
	for (i = 0; i < COPPERHEAD_NR_PROFILES; i++) {
		priv->buttons[i] = copperhead_default_buttonmap;
		priv->cur_freq[i] = RAZER_MOUSE_FREQ_1000HZ;
		priv->cur_dpimapping[i] = &priv->dpimappings[0];
	}
	priv->cur_profile = &priv->profiles[0];

	/* Read the current profile number. It's currently unused, though. */
	err = copperhead_usb_read(priv, LIBUSB_REQUEST_CLEAR_FEATURE,
				  0x01, 0, &value, sizeof(value));
	if (err)
		return err;

	for (i = 0; i < 64; i++) {
		char buf[0x156];

		value = 1;
#if 0
		err = copperhead_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
					   0x02, 3, &value, sizeof(value));
//		if (err)
//			return err;
#endif
		err = copperhead_usb_read(priv, LIBUSB_REQUEST_CLEAR_FEATURE,
					  0x01, 0, buf, sizeof(buf));
		if (err)
			return err;
	}

	return 0;
}

static int copperhead_get_fw_version(struct razer_mouse *m)
{
	struct copperhead_private *priv = m->internal;

	return priv->fw_version;
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

	if (!priv->m->claim_count)
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

static struct razer_mouse_dpimapping * copperhead_get_dpimapping(struct razer_mouse_profile *p,
								 struct razer_axis *axis)
{
	struct copperhead_private *priv = p->mouse->internal;

	if (p->nr >= ARRAY_SIZE(priv->cur_dpimapping))
		return NULL;

	return priv->cur_dpimapping[p->nr];
}

static int copperhead_set_dpimapping(struct razer_mouse_profile *p,
				     struct razer_axis *axis,
				     struct razer_mouse_dpimapping *d)
{
	struct copperhead_private *priv = p->mouse->internal;
	struct razer_mouse_dpimapping *oldmapping;
	int err;

	if (!priv->m->claim_count)
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

static int copperhead_supported_buttons(struct razer_mouse *m,
					struct razer_button **res_ptr)
{
	*res_ptr = copperhead_physical_buttons;
	return ARRAY_SIZE(copperhead_physical_buttons);
}

static int copperhead_supported_button_functions(struct razer_mouse *m,
						 struct razer_button_function **res_ptr)
{
	*res_ptr = copperhead_button_functions;
	return ARRAY_SIZE(copperhead_button_functions);
}

static struct razer_button_function * copperhead_get_button_function(struct razer_mouse_profile *p,
								     struct razer_button *b)
{
	struct copperhead_private *priv = p->mouse->internal;
	struct copperhead_buttonmappings *m;
	struct copperhead_one_buttonmapping *one;
	unsigned int i;

	if (p->nr > ARRAY_SIZE(priv->buttons))
		return NULL;
	m = &priv->buttons[p->nr];

	one = copperhead_buttonid_to_mapping(m, b->id);
	if (!one)
		return NULL;
	for (i = 0; i < ARRAY_SIZE(copperhead_button_functions); i++) {
		if (copperhead_button_functions[i].id == one->logical)
			return &copperhead_button_functions[i];
	}

	return NULL;
}

static int copperhead_set_button_function(struct razer_mouse_profile *p,
					  struct razer_button *b,
					  struct razer_button_function *f)
{
	struct copperhead_private *priv = p->mouse->internal;
	struct copperhead_buttonmappings *m;
	struct copperhead_one_buttonmapping *one;
	uint8_t oldlogical;
	int err;

	if (!priv->m->claim_count)
		return -EBUSY;
	if (p->nr > ARRAY_SIZE(priv->buttons))
		return -EINVAL;
	m = &priv->buttons[p->nr];

	one = copperhead_buttonid_to_mapping(m, b->id);
	if (!one)
		return -ENODEV;
	oldlogical = one->logical;
	one->logical = f->id;
	err = copperhead_commit(priv);
	if (err) {
		one->logical = oldlogical;
		return err;
	}

	return 0;
}

int razer_copperhead_init(struct razer_mouse *m,
			  struct libusb_device *usbdev)
{
	struct copperhead_private *priv;
	unsigned int i;
	int err;

	BUILD_BUG_ON(sizeof(struct copperhead_profcfg_cmd) != 0x15C);

	priv = zalloc(sizeof(struct copperhead_private));
	if (!priv)
		return -ENOMEM;
	priv->m = m;
	m->internal = priv;

	err = razer_usb_add_used_interface(m->usb_ctx, 0, 0);
	err |= razer_usb_add_used_interface(m->usb_ctx, 1, 0);
	if (err) {
		err = -EIO;
		goto err_free;
	}

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
		priv->profiles[i].get_button_function = copperhead_get_button_function;
		priv->profiles[i].set_button_function = copperhead_set_button_function;
		priv->profiles[i].mouse = m;
	}

	err = m->claim(m);
	if (err) {
		razer_error("hw_copperhead: "
			"Failed to initially claim the device\n");
		goto err_free;
	}
	err = copperhead_read_config_from_hw(priv);
	if (!err)
		err = copperhead_commit(priv);
	m->release(m);
	if (err) {
		razer_error("hw_copperhead: "
			"Failed to read the configuration from hardware\n");
		goto err_free;
	}

	m->type = RAZER_MOUSETYPE_COPPERHEAD;
	razer_generic_usb_gen_idstr(usbdev, NULL, "Copperhead", 1, m->idstr);

	m->get_fw_version = copperhead_get_fw_version;
	m->nr_profiles = COPPERHEAD_NR_PROFILES;
	m->get_profiles = copperhead_get_profiles;
	m->get_active_profile = copperhead_get_active_profile;
	m->supported_resolutions = copperhead_supported_resolutions;
	m->supported_freqs = copperhead_supported_freqs;
	m->supported_dpimappings = copperhead_supported_dpimappings;
	m->supported_buttons = copperhead_supported_buttons;
	m->supported_button_functions = copperhead_supported_button_functions;

	return 0;

err_free:
	free(priv);

	return err;
}

void razer_copperhead_release(struct razer_mouse *m)
{
	struct copperhead_private *priv = m->internal;

	free(priv);
}
