/*
 *   Lowlevel hardware access for the
 *   Razer Boomslang Collector's Edition mouse
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering only.
 *
 *   Copyright (C) 2010 Michael Buesch <mb@bu3sch.de>
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

#include "hw_boomslangce.h"
#include "razer_private.h"
#include "buttonmapping.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


enum {
	BOOMSLANGCE_LED_SCROLL = 0,
	BOOMSLANGCE_LED_GLOWPIPE,
	BOOMSLANGCE_NR_LEDS,
};

enum { /* Misc constants */
	BOOMSLANGCE_NR_PROFILES		= 5,
	BOOMSLANGCE_NR_DPIMAPPINGS	= 3,
};

/* The wire protocol data structures... */

enum boomslangce_phys_button {
	/* Physical button IDs */
	BOOMSLANGCE_PHYSBUT_LEFT = 0x01,	/* Left button */
	BOOMSLANGCE_PHYSBUT_RIGHT,		/* Right button */
	BOOMSLANGCE_PHYSBUT_MIDDLE,		/* Middle button */
	BOOMSLANGCE_PHYSBUT_LSIDE,		/* Left side */
	BOOMSLANGCE_PHYSBUT_RSIDE,		/* Right side */
	BOOMSLANGCE_PHYSBUT_SCROLLUP,		/* Scroll up */
	BOOMSLANGCE_PHYSBUT_SCROLLDOWN,		/* Scroll down */

	NR_BOOMSLANGCE_PHYSBUT = 7,		/* Number of physical buttons */
};

enum boomslangce_button_function {
	/* Logical button function IDs */
	BOOMSLANGCE_BUTFUNC_LEFT	= 0x01, /* Left button */
	BOOMSLANGCE_BUTFUNC_RIGHT	= 0x02, /* Right button */
	BOOMSLANGCE_BUTFUNC_MIDDLE	= 0x03, /* Middle button */
	BOOMSLANGCE_BUTFUNC_DPIUP	= 0x0C, /* DPI down */
	BOOMSLANGCE_BUTFUNC_DPIDOWN	= 0x0D, /* DPI down */
	BOOMSLANGCE_BUTFUNC_WIN5	= 0x0A, /* Windows button 5 */
	BOOMSLANGCE_BUTFUNC_WIN4	= 0x0B, /* Windows button 4 */
	BOOMSLANGCE_BUTFUNC_SCROLLUP	= 0x30, /* Scroll wheel up */
	BOOMSLANGCE_BUTFUNC_SCROLLDOWN	= 0x31, /* Scroll wheel down */
};

struct boomslangce_one_buttonmapping {
	uint8_t physical;
	uint8_t logical;
} _packed;

struct boomslangce_buttonmappings {
	struct boomslangce_one_buttonmapping left;
	uint8_t _padding0[46];
	struct boomslangce_one_buttonmapping right;
	uint8_t _padding1[46];
	struct boomslangce_one_buttonmapping middle;
	uint8_t _padding2[46];
	struct boomslangce_one_buttonmapping rside;
	uint8_t _padding3[46];
	struct boomslangce_one_buttonmapping lside;
	uint8_t _padding4[46];
	struct boomslangce_one_buttonmapping scrollup;
	uint8_t _padding5[46];
	struct boomslangce_one_buttonmapping scrolldown;
	uint8_t _padding6[42];
} _packed;

struct boomslangce_profcfg_cmd {
	le16_t packetlength;
	le16_t magic;
	le16_t profilenr;
	le16_t reply_packetlength;	/* Only valid for read data */
	le16_t reply_magic;		/* Only valid for read data */
	le16_t reply_profilenr;
	uint8_t dpisel;
	uint8_t freq;
	struct boomslangce_buttonmappings buttons;
	le16_t checksum;
} _packed;
#define BOOMSLANGCE_PROFCFG_MAGIC	cpu_to_le16(0x0002)

struct boomslangce_private {
	struct razer_mouse *m;

	uint16_t fw_version;

	/* The currently set LED states. */
	bool led_states[BOOMSLANGCE_NR_LEDS];

	/* The active profile. */
	struct razer_mouse_profile *cur_profile;
	/* Profile configuration (one per profile). */
	struct razer_mouse_profile profiles[BOOMSLANGCE_NR_PROFILES];

	/* The active DPI mapping; per profile. */
	struct razer_mouse_dpimapping *cur_dpimapping[BOOMSLANGCE_NR_PROFILES];
	/* The possible DPI mappings. */
	struct razer_mouse_dpimapping dpimappings[BOOMSLANGCE_NR_DPIMAPPINGS];

	/* The active scan frequency; per profile. */
	enum razer_mouse_freq cur_freq[BOOMSLANGCE_NR_PROFILES];

	/* The active button mapping; per profile. */
	struct boomslangce_buttonmappings buttons[BOOMSLANGCE_NR_PROFILES];
};

/* A list of physical buttons on the device. */
static struct razer_button boomslangce_physical_buttons[] = {
	{ .id = BOOMSLANGCE_PHYSBUT_LEFT,	.name = "Leftclick",		},
	{ .id = BOOMSLANGCE_PHYSBUT_RIGHT,	.name = "Rightclick",		},
	{ .id = BOOMSLANGCE_PHYSBUT_MIDDLE,	.name = "Middleclick",		},
	{ .id = BOOMSLANGCE_PHYSBUT_LSIDE,	.name = "Leftside button",	},
	{ .id = BOOMSLANGCE_PHYSBUT_RSIDE,	.name = "Rightside button",	},
	{ .id = BOOMSLANGCE_PHYSBUT_SCROLLUP,	.name = "Scroll up",		},
	{ .id = BOOMSLANGCE_PHYSBUT_SCROLLDOWN,	.name = "Scroll down",		},
};

/* A list of possible button functions. */
static struct razer_button_function boomslangce_button_functions[] = {
	{ .id = BOOMSLANGCE_BUTFUNC_LEFT,	.name = "Leftclick",		},
	{ .id = BOOMSLANGCE_BUTFUNC_RIGHT,	.name = "Rightclick",		},
	{ .id = BOOMSLANGCE_BUTFUNC_MIDDLE,	.name = "Middleclick",		},
	{ .id = BOOMSLANGCE_BUTFUNC_DPIUP,	.name = "DPI switch up",	},
	{ .id = BOOMSLANGCE_BUTFUNC_DPIDOWN,	.name = "DPI switch down",	},
	{ .id = BOOMSLANGCE_BUTFUNC_WIN5,	.name = "Windows Button 5",	},
	{ .id = BOOMSLANGCE_BUTFUNC_WIN4,	.name = "Windows Button 4",	},
	{ .id = BOOMSLANGCE_BUTFUNC_SCROLLUP,	.name = "Scroll up",		},
	{ .id = BOOMSLANGCE_BUTFUNC_SCROLLDOWN,	.name = "Scroll down",		},
};
/* TODO: There are more functions */

#define DEFINE_DEF_BUTMAP(mappingptr, phys, func)			\
	.mappingptr = { .physical = BOOMSLANGCE_PHYSBUT_##phys,		\
			.logical = BOOMSLANGCE_BUTFUNC_##func,		\
	}
static const struct boomslangce_buttonmappings boomslangce_default_buttonmap = {
	DEFINE_DEF_BUTMAP(left, LEFT, LEFT),
	DEFINE_DEF_BUTMAP(right, RIGHT, RIGHT),
	DEFINE_DEF_BUTMAP(middle, MIDDLE, MIDDLE),
	DEFINE_DEF_BUTMAP(lside, LSIDE, WIN5),
	DEFINE_DEF_BUTMAP(rside, RSIDE, WIN4),
	DEFINE_DEF_BUTMAP(scrollup, SCROLLUP, SCROLLUP),
	DEFINE_DEF_BUTMAP(scrolldown, SCROLLDOWN, SCROLLDOWN),
};


static struct boomslangce_one_buttonmapping *
	boomslangce_buttonid_to_mapping(struct boomslangce_buttonmappings *mappings,
				       enum boomslangce_phys_button id)
{
	switch (id) {
	case BOOMSLANGCE_PHYSBUT_LEFT:
		return &mappings->left;
	case BOOMSLANGCE_PHYSBUT_RIGHT:
		return &mappings->right;
	case BOOMSLANGCE_PHYSBUT_MIDDLE:
		return &mappings->middle;
	case BOOMSLANGCE_PHYSBUT_LSIDE:
		return &mappings->lside;
	case BOOMSLANGCE_PHYSBUT_RSIDE:
		return &mappings->rside;
	case BOOMSLANGCE_PHYSBUT_SCROLLUP:
		return &mappings->scrollup;
	case BOOMSLANGCE_PHYSBUT_SCROLLDOWN:
		return &mappings->scrolldown;
	}
	return NULL;
}

static bool verify_buttons(const struct boomslangce_buttonmappings *map)
{
	if (!razer_buffer_is_all_zero(map->_padding0, sizeof(map->_padding0)) ||
	    !razer_buffer_is_all_zero(map->_padding1, sizeof(map->_padding1)) ||
	    !razer_buffer_is_all_zero(map->_padding2, sizeof(map->_padding2)) ||
	    !razer_buffer_is_all_zero(map->_padding3, sizeof(map->_padding3)) ||
	    !razer_buffer_is_all_zero(map->_padding4, sizeof(map->_padding4)) ||
	    !razer_buffer_is_all_zero(map->_padding5, sizeof(map->_padding5)) ||
	    !razer_buffer_is_all_zero(map->_padding6, sizeof(map->_padding6)))
		return 0;

/*
	if (map->left.physical != BOOMSLANGCE_PHYSBUT_LEFT ||
	    map->right.physical != BOOMSLANGCE_PHYSBUT_RIGHT ||
	    map->middle.physical != BOOMSLANGCE_PHYSBUT_MIDDLE ||
	    map->lside.physical != BOOMSLANGCE_PHYSBUT_LSIDE ||
	    map->rside.physical != BOOMSLANGCE_PHYSBUT_RSIDE ||
	    map->scrollup.physical != BOOMSLANGCE_PHYSBUT_SCROLLUP ||
	    map->scrolldown.physical != BOOMSLANGCE_PHYSBUT_SCROLLDOWN)
		return 0;*/

	return 1;
}

static int boomslangce_usb_write(struct boomslangce_private *priv,
				int request, int command, int index,
				const void *buf, size_t size)
{
	int err;

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_OTHER,
		request, command, index,
		(unsigned char *)buf, size, RAZER_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-boomslangce: "
			"USB write 0x%02X 0x%02X 0x%02X failed: %d\n",
			request, command, index, err);
		return -EIO;
	}

	return 0;
}

static int boomslangce_usb_read(struct boomslangce_private *priv,
			       int request, int command, int index,
			       void *buf, size_t size)
{
	int err;

	err = libusb_control_transfer(
		priv->m->usb_ctx->h,
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_OTHER,
		request, command, index,
		(unsigned char *)buf, size, RAZER_USB_TIMEOUT);
	if (err != size) {
		razer_error("razer-boomslangce: "
			"USB read 0x%02X 0x%02X 0x%02X failed: %d\n",
			request, command, index, err);
		return -EIO;
	}

	return 0;
}

static int boomslangce_read_fw_ver(struct boomslangce_private *priv)
{
	char buf[2];
	uint16_t ver;

	buf[0] = 0;//TODO
	buf[1] = 0;

	ver = buf[0];
	ver <<= 8;
	ver |= buf[1];

	return ver;
}

static int boomslangce_commit(struct boomslangce_private *priv)
{
	union {
		struct boomslangce_profcfg_cmd profcfg;
		uint8_t chunks[64 * 6];
	} _packed u;
	uint8_t *chunk;
	unsigned int i, j;
	int err;
	unsigned char value;

	BUILD_BUG_ON(sizeof(u) != 0x180);

	/* Upload the profile config */
	for (i = 0; i < BOOMSLANGCE_NR_PROFILES; i++) {
		memset(&u, 0, sizeof(u));
		u.profcfg.packetlength = cpu_to_le16(sizeof(u.profcfg));
		u.profcfg.magic = BOOMSLANGCE_PROFCFG_MAGIC;
		u.profcfg.profilenr = cpu_to_le16(i + 1);
		u.profcfg.reply_profilenr = u.profcfg.profilenr;
		switch (priv->cur_dpimapping[i]->res) {
		default:
		case RAZER_MOUSE_RES_400DPI:
			u.profcfg.dpisel = 4;
			break;
		case RAZER_MOUSE_RES_800DPI:
			u.profcfg.dpisel = 3;
			break;
		case RAZER_MOUSE_RES_1800DPI:
			u.profcfg.dpisel = 2;
			break;
		}
		switch (priv->cur_freq[i]) {
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
		u.profcfg.buttons = priv->buttons[i];
		u.profcfg.checksum = razer_xor16_checksum(&u.profcfg,
					sizeof(u.profcfg) - 2);
		/* The profile config is committed in 64byte chunks */
		chunk = &u.chunks[0];
		for (j = 0; j < 6; j++, chunk += 64) {
			err = boomslangce_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
						   j + 1, 0, chunk, 64);
			if (err)
				return err;
		}
		/* Commit the profile */
		value = i + 1;
		boomslangce_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
				     0x02, 3, &value, sizeof(value));
		/* Read back the result */
		BUILD_BUG_ON(0x156 + 6 != sizeof(u.profcfg));
		memset(&u, 0, sizeof(u));
		err = boomslangce_usb_read(priv, LIBUSB_REQUEST_CLEAR_FEATURE,
					  0x01, 0, ((uint8_t *)&u.profcfg) + 6,
					  sizeof(u.profcfg) - 6);
		if (err)
			return err;
		if (razer_xor16_checksum(&u.profcfg, sizeof(u.profcfg))) {
			razer_error("hw_boomslangce: Profile commit checksum mismatch\n");
			return -EIO;
		}
	}

	/* Select the profile */
	value = priv->cur_profile->nr + 1;
	err = boomslangce_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
				   0x02, 1, &value, sizeof(value));
	if (err)
		return err;

	/* Switch LED states */
	value = 0;
	if (priv->led_states[BOOMSLANGCE_LED_SCROLL])
		value |= 0x01;
	if (priv->led_states[BOOMSLANGCE_LED_GLOWPIPE])
		value |= 0x02;
	err = boomslangce_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
				    0x02, 5, &value, sizeof(value));
	if (err)
		return err;

	return 0;
}

static int boomslangce_read_config_from_hw(struct boomslangce_private *priv)
{
	struct boomslangce_profcfg_cmd profcfg;
	unsigned int i;
	unsigned char value;
	int err;

	/* Assign sane defaults. */
	for (i = 0; i < BOOMSLANGCE_NR_PROFILES; i++) {
		priv->buttons[i] = boomslangce_default_buttonmap;
		priv->cur_freq[i] = RAZER_MOUSE_FREQ_1000HZ;
		priv->cur_dpimapping[i] = &priv->dpimappings[0];
	}

	/* Read the current profile number. */
	err = boomslangce_usb_read(priv, LIBUSB_REQUEST_CLEAR_FEATURE,
				  0x01, 0, &value, sizeof(value));
	if (err)
		return err;
	if (value < 1 || value > BOOMSLANGCE_NR_PROFILES) {
		razer_error("hw_boomslangce: Got invalid profile number\n");
		return -EIO;
	}
	priv->cur_profile = &priv->profiles[value - 1];

	/* Read the profiles config */
	for (i = 0; i < BOOMSLANGCE_NR_PROFILES; i++) {
		BUILD_BUG_ON(0x156 + 6 != sizeof(profcfg));

		/* Request profile config */
		value = i + 1;
		err = boomslangce_usb_write(priv, LIBUSB_REQUEST_SET_CONFIGURATION,
					   0x02, 3, &value, sizeof(value));
		if (err)
			return err;
		/* Read profile config */
		memset(&profcfg, 0, sizeof(profcfg));
		err = boomslangce_usb_read(priv, LIBUSB_REQUEST_CLEAR_FEATURE,
					  0x01, 0, ((uint8_t *)&profcfg) + 6,
					  sizeof(profcfg) - 6);
		if (err)
			return err;
		if (razer_xor16_checksum(&profcfg, sizeof(profcfg))) {
			razer_error("hw_boomslangce: Read profile data checksum mismatch\n");
			return -EIO;
		}
		if (le16_to_cpu(profcfg.reply_profilenr) != i + 1) {
			razer_error("hw_boomslangce: Got invalid profile nr in profile config\n");
			return -EIO;
		}
		switch (profcfg.dpisel) {
		case 4:
			priv->cur_dpimapping[i] = razer_mouse_get_dpimapping_by_res(
					priv->dpimappings, ARRAY_SIZE(priv->dpimappings),
					RAZER_MOUSE_RES_400DPI);
			break;
		case 3:
			priv->cur_dpimapping[i] = razer_mouse_get_dpimapping_by_res(
					priv->dpimappings, ARRAY_SIZE(priv->dpimappings),
					RAZER_MOUSE_RES_800DPI);
			break;
		case 2:
			priv->cur_dpimapping[i] = razer_mouse_get_dpimapping_by_res(
					priv->dpimappings, ARRAY_SIZE(priv->dpimappings),
					RAZER_MOUSE_RES_1800DPI);
			break;
		default:
			razer_error("hw_boomslangce: Got invalid DPI mapping selection\n");
			return -EIO;
		}
		if (!priv->cur_dpimapping[i]) {
			razer_error("hw_boomslangce: Internal error: Did not find dpimapping\n");
			return -ENODEV;
		}
		switch (profcfg.freq) {
		case 3:
			priv->cur_freq[i] = RAZER_MOUSE_FREQ_125HZ;
			break;
		case 2:
			priv->cur_freq[i] = RAZER_MOUSE_FREQ_500HZ;
			break;
		case 1:
			priv->cur_freq[i] = RAZER_MOUSE_FREQ_1000HZ;
			break;
		default:
			razer_error("hw_boomslangce: Got invalid frequency selection\n");
			return -EIO;
		}
		if (!verify_buttons(&profcfg.buttons)) {
			razer_error("hw_boomslangce: Got invalid buttons map\n");
			return -EIO;
		}
		priv->buttons[i] = profcfg.buttons;
	}

	/* Read the LED states */
//	err = boomslang_usb_read(priv, LIBUSB_REQUEST_
	//TODO

	return 0;
}

static int boomslangce_get_fw_version(struct razer_mouse *m)
{
	struct boomslangce_private *priv = m->drv_data;

	return priv->fw_version;
}

static struct razer_mouse_profile * boomslangce_get_profiles(struct razer_mouse *m)
{
	struct boomslangce_private *priv = m->drv_data;

	return &priv->profiles[0];
}

static struct razer_mouse_profile * boomslangce_get_active_profile(struct razer_mouse *m)
{
	struct boomslangce_private *priv = m->drv_data;

	return priv->cur_profile;
}

static int boomslangce_set_active_profile(struct razer_mouse *m,
					 struct razer_mouse_profile *p)
{
	struct boomslangce_private *priv = m->drv_data;
	struct razer_mouse_profile *oldprof;
	int err;

	if (!priv->m->claim_count)
		return -EBUSY;

	oldprof = priv->cur_profile;
	priv->cur_profile = p;

	err = boomslangce_commit(priv);
	if (err) {
		priv->cur_profile = oldprof;
		return err;
	}

	return err;
}

static int boomslangce_supported_resolutions(struct razer_mouse *m,
					    enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	const int count = 3;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	list[0] = RAZER_MOUSE_RES_400DPI;
	list[1] = RAZER_MOUSE_RES_800DPI;
	list[2] = RAZER_MOUSE_RES_1800DPI;

	*res_list = list;

	return count;
}

static int boomslangce_supported_freqs(struct razer_mouse *m,
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

static enum razer_mouse_freq boomslangce_get_freq(struct razer_mouse_profile *p)
{
	struct boomslangce_private *priv = p->mouse->drv_data;

	if (p->nr >= ARRAY_SIZE(priv->cur_freq))
		return -EINVAL;

	return priv->cur_freq[p->nr];
}

static int boomslangce_set_freq(struct razer_mouse_profile *p,
			       enum razer_mouse_freq freq)
{
	struct boomslangce_private *priv = p->mouse->drv_data;
	enum razer_mouse_freq oldfreq;
	int err;

	if (!priv->m->claim_count)
		return -EBUSY;
	if (p->nr >= ARRAY_SIZE(priv->cur_freq))
		return -EINVAL;

	oldfreq = priv->cur_freq[p->nr];
	priv->cur_freq[p->nr] = freq;

	err = boomslangce_commit(priv);
	if (err) {
		priv->cur_freq[p->nr] = oldfreq;
		return err;
	}

	return 0;
}

static int boomslangce_supported_dpimappings(struct razer_mouse *m,
					    struct razer_mouse_dpimapping **res_ptr)
{
	struct boomslangce_private *priv = m->drv_data;

	*res_ptr = &priv->dpimappings[0];

	return ARRAY_SIZE(priv->dpimappings);
}

static struct razer_mouse_dpimapping * boomslangce_get_dpimapping(struct razer_mouse_profile *p,
								 struct razer_axis *axis)
{
	struct boomslangce_private *priv = p->mouse->drv_data;

	if (p->nr >= ARRAY_SIZE(priv->cur_dpimapping))
		return NULL;

	return priv->cur_dpimapping[p->nr];
}

static int boomslangce_set_dpimapping(struct razer_mouse_profile *p,
				     struct razer_axis *axis,
				     struct razer_mouse_dpimapping *d)
{
	struct boomslangce_private *priv = p->mouse->drv_data;
	struct razer_mouse_dpimapping *oldmapping;
	int err;

	if (!priv->m->claim_count)
		return -EBUSY;
	if (p->nr >= ARRAY_SIZE(priv->cur_dpimapping))
		return -EINVAL;

	oldmapping = priv->cur_dpimapping[p->nr];
	priv->cur_dpimapping[p->nr] = d;

	err = boomslangce_commit(priv);
	if (err) {
		priv->cur_dpimapping[p->nr] = oldmapping;
		return err;
	}

	return err;
}

static int boomslangce_led_toggle(struct razer_led *led,
				  enum razer_led_state new_state)
{
	struct razer_mouse *m = led->u.mouse;
	struct boomslangce_private *priv = m->drv_data;
	int err;
	enum razer_led_state old_state;

	if (led->id >= BOOMSLANGCE_NR_LEDS)
		return -EINVAL;
	if ((new_state != RAZER_LED_OFF) &&
	    (new_state != RAZER_LED_ON))
		return -EINVAL;

	if (!m->claim_count)
		return -EBUSY;

	old_state = priv->led_states[led->id];
	priv->led_states[led->id] = new_state;

	err = boomslangce_commit(priv);
	if (err) {
		priv->led_states[led->id] = old_state;
		return err;
	}

	return err;
}

static int boomslangce_get_leds(struct razer_mouse *m,
				struct razer_led **leds_list)
{
	struct boomslangce_private *priv = m->drv_data;
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
	scroll->id = BOOMSLANGCE_LED_SCROLL;
	scroll->state = priv->led_states[BOOMSLANGCE_LED_SCROLL];
	scroll->toggle_state = boomslangce_led_toggle;
	scroll->u.mouse = m;

	logo->name = "GlowPipe";
	logo->id = BOOMSLANGCE_LED_GLOWPIPE;
	logo->state = priv->led_states[BOOMSLANGCE_LED_GLOWPIPE];
	logo->toggle_state = boomslangce_led_toggle;
	logo->u.mouse = m;

	/* Link the list */
	*leds_list = scroll;
	scroll->next = logo;
	logo->next = NULL;

	return BOOMSLANGCE_NR_LEDS;
}

static int boomslangce_supported_buttons(struct razer_mouse *m,
					struct razer_button **res_ptr)
{
	*res_ptr = boomslangce_physical_buttons;
	return ARRAY_SIZE(boomslangce_physical_buttons);
}

static int boomslangce_supported_button_functions(struct razer_mouse *m,
						 struct razer_button_function **res_ptr)
{
	*res_ptr = boomslangce_button_functions;
	return ARRAY_SIZE(boomslangce_button_functions);
}

static struct razer_button_function * boomslangce_get_button_function(struct razer_mouse_profile *p,
								     struct razer_button *b)
{
	struct boomslangce_private *priv = p->mouse->drv_data;
	struct boomslangce_buttonmappings *m;
	struct boomslangce_one_buttonmapping *one;
	unsigned int i;

	if (p->nr > ARRAY_SIZE(priv->buttons))
		return NULL;
	m = &priv->buttons[p->nr];

	one = boomslangce_buttonid_to_mapping(m, b->id);
	if (!one)
		return NULL;
	for (i = 0; i < ARRAY_SIZE(boomslangce_button_functions); i++) {
		if (boomslangce_button_functions[i].id == one->logical)
			return &boomslangce_button_functions[i];
	}

	return NULL;
}

static int boomslangce_set_button_function(struct razer_mouse_profile *p,
					  struct razer_button *b,
					  struct razer_button_function *f)
{
	struct boomslangce_private *priv = p->mouse->drv_data;
	struct boomslangce_buttonmappings *m;
	struct boomslangce_one_buttonmapping *one;
	uint8_t oldlogical;
	int err;

	if (!priv->m->claim_count)
		return -EBUSY;
	if (p->nr > ARRAY_SIZE(priv->buttons))
		return -EINVAL;
	m = &priv->buttons[p->nr];

	one = boomslangce_buttonid_to_mapping(m, b->id);
	if (!one)
		return -ENODEV;
	oldlogical = one->logical;
	one->logical = f->id;
	err = boomslangce_commit(priv);
	if (err) {
		one->logical = oldlogical;
		return err;
	}

	return 0;
}

int razer_boomslangce_init(struct razer_mouse *m,
			  struct libusb_device *usbdev)
{
	struct boomslangce_private *priv;
	unsigned int i;
	int err;

	BUILD_BUG_ON(sizeof(struct boomslangce_profcfg_cmd) != 0x15C);

	priv = zalloc(sizeof(struct boomslangce_private));
	if (!priv)
		return -ENOMEM;
	priv->m = m;
	m->drv_data = priv;

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
	priv->dpimappings[2].res = RAZER_MOUSE_RES_1800DPI;
	priv->dpimappings[2].mouse = m;

	for (i = 0; i < BOOMSLANGCE_NR_PROFILES; i++) {
		priv->profiles[i].nr = i;
		priv->profiles[i].get_freq = boomslangce_get_freq;
		priv->profiles[i].set_freq = boomslangce_set_freq;
		priv->profiles[i].get_dpimapping = boomslangce_get_dpimapping;
		priv->profiles[i].set_dpimapping = boomslangce_set_dpimapping;
		priv->profiles[i].get_button_function = boomslangce_get_button_function;
		priv->profiles[i].set_button_function = boomslangce_set_button_function;
		priv->profiles[i].mouse = m;
	}

	for (i = 0; i < BOOMSLANGCE_NR_LEDS; i++)
		priv->led_states[i] = RAZER_LED_ON;

	err = m->claim(m);
	if (err) {
		razer_error("hw_boomslangce: "
			"Failed to initially claim the device\n");
		goto err_free;
	}
	err = boomslangce_read_fw_ver(priv);
	if (err) {
		razer_error("hw_boomslangce: Failed to fetch firmware version number\n");
		goto err_release;
	}
	err = boomslangce_read_config_from_hw(priv);
	if (err) {
		razer_error("hw_boomslangce: Failed to read config from hardware\n");
		goto err_release;
	}

	m->type = RAZER_MOUSETYPE_BOOMSLANGCE;
	razer_generic_usb_gen_idstr(usbdev, NULL, "Boomslang-CE", 1, m->idstr);

	m->get_fw_version = boomslangce_get_fw_version;
	m->get_leds = boomslangce_get_leds;
	m->nr_profiles = BOOMSLANGCE_NR_PROFILES;
	m->get_profiles = boomslangce_get_profiles;
	m->get_active_profile = boomslangce_get_active_profile;
	m->set_active_profile = boomslangce_set_active_profile;
	m->supported_resolutions = boomslangce_supported_resolutions;
	m->supported_freqs = boomslangce_supported_freqs;
	m->supported_dpimappings = boomslangce_supported_dpimappings;
	m->supported_buttons = boomslangce_supported_buttons;
	m->supported_button_functions = boomslangce_supported_button_functions;

	err = boomslangce_commit(priv);
	if (err) {
		razer_error("hw_boomslangce: Failed to commit initial config\n");
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

void razer_boomslangce_release(struct razer_mouse *m)
{
	struct boomslangce_private *priv = m->drv_data;

	free(priv);
}
