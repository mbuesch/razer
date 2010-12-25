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


enum { /* LED IDs */
	LACHESIS_LED_SCROLL = 0,
	LACHESIS_LED_LOGO,
	LACHESIS_NR_LEDS,
};

enum { /* Misc constants */
	LACHESIS_NR_PROFILES	= 5,
	LACHESIS_NR_DPIMAPPINGS	= 5,
	LACHESIS_NR_AXES	= 3,
};

/* The wire protocol data structures... */

enum lachesis_phys_button {
	/* Physical button IDs */
	LACHESIS_PHYSBUT_LEFT = 0x01,	/* Left button */
	LACHESIS_PHYSBUT_RIGHT,		/* Right button */
	LACHESIS_PHYSBUT_MIDDLE,	/* Middle button */
	LACHESIS_PHYSBUT_LFRONT,	/* Left side, front button */
	LACHESIS_PHYSBUT_LREAR,		/* Left side, rear button */
	LACHESIS_PHYSBUT_RFRONT,	/* Right side, front button */
	LACHESIS_PHYSBUT_RREAR,		/* Right side, rear button */
	LACHESIS_PHYSBUT_TFRONT,	/* Top side, front button */
	LACHESIS_PHYSBUT_TREAR,		/* Top side, rear button */
	LACHESIS_PHYSBUT_SCROLLUP,	/* Scroll wheel up */
	LACHESIS_PHYSBUT_SCROLLDOWN,	/* Scroll wheel down */

	NR_LACHESIS_PHYSBUT = 11,	/* Number of physical buttons */
};

enum lachesis_button_function {
	/* Logical button function IDs */
	LACHESIS_BUTFUNC_LEFT		= 0x01, /* Left button */
	LACHESIS_BUTFUNC_RIGHT		= 0x02, /* Right button */
	LACHESIS_BUTFUNC_MIDDLE		= 0x03, /* Middle button */
	LACHESIS_BUTFUNC_DOUBLECLICK	= 0x04, /* Left button double click */ //XXX
	LACHESIS_BUTFUNC_ADVANCED	= 0x05,	/* Advanced function */ //XXX
	LACHESIS_BUTFUNC_MACRO		= 0x06, /* Macro function */ //XXX
	LACHESIS_BUTFUNC_PROFDOWN	= 0x0A, /* Profile down */
	LACHESIS_BUTFUNC_PROFUP		= 0x0B, /* Profile up */
	LACHESIS_BUTFUNC_DPIUP		= 0x0C, /* DPI down */
	LACHESIS_BUTFUNC_DPIDOWN	= 0x0D, /* DPI down */
	LACHESIS_BUTFUNC_DPI1		= 0x0E, /* Select first DPI mapping */
	LACHESIS_BUTFUNC_DPI2		= 0x0F, /* Select second DPI mapping */
	LACHESIS_BUTFUNC_DPI3		= 0x10, /* Select third DPI mapping */
	LACHESIS_BUTFUNC_DPI4		= 0x11, /* Select fourth DPI mapping */
	LACHESIS_BUTFUNC_DPI5		= 0x12, /* Select fifth DPI mapping */
	LACHESIS_BUTFUNC_WIN5		= 0x1A, /* Windows button 5 */
	LACHESIS_BUTFUNC_WIN4		= 0x1B, /* Windows button 4 */
	LACHESIS_BUTFUNC_SCROLLUP	= 0x30, /* Scroll wheel up */
	LACHESIS_BUTFUNC_SCROLLDOWN	= 0x31, /* Scroll wheel down */
};

struct lachesis_one_buttonmapping {
	uint8_t physical;
	uint8_t logical;
	uint8_t _padding[33];
} _packed;

struct lachesis_buttonmappings {
	struct lachesis_one_buttonmapping mappings[NR_LACHESIS_PHYSBUT];
} _packed;

struct lachesis_profcfg_cmd {
	le16_t packetlength;
	le16_t magic;
	uint8_t profile;
	uint8_t _padding0;
	uint8_t dpisel;
	uint8_t freq;
	uint8_t _padding1;
	struct lachesis_buttonmappings buttons;
	le16_t checksum;
} _packed;
#define LACHESIS_PROFCFG_MAGIC		cpu_to_le16(0x0002)

struct lachesis_one_dpimapping {
	uint8_t magic;
	uint8_t dpival0;
	uint8_t dpival1;
} _packed;
#define LACHESIS_DPIMAPPING_MAGIC	0x01

struct lachesis_dpimap_cmd {
	struct lachesis_one_dpimapping mappings[5];
	uint8_t _padding[81];
} _packed;


/* Context data structure */
struct lachesis_private {
	unsigned int claimed;
	struct razer_usb_context usb;
	uint16_t fw_version;

	/* The currently set LED states. */
	enum razer_led_state led_states[LACHESIS_NR_LEDS];

	/* The active profile. */
	struct razer_mouse_profile *cur_profile;
	/* Profile configuration (one per profile). */
	struct razer_mouse_profile profiles[LACHESIS_NR_PROFILES];

	/* Supported mouse axes */
	struct razer_axis axes[LACHESIS_NR_AXES];

	/* The active DPI mapping; per profile. */
	struct razer_mouse_dpimapping *cur_dpimapping[LACHESIS_NR_PROFILES];
	/* The possible DPI mappings. */
	struct razer_mouse_dpimapping dpimappings[LACHESIS_NR_DPIMAPPINGS];

	/* The active scan frequency; per profile. */
	enum razer_mouse_freq cur_freq[LACHESIS_NR_PROFILES];

	/* The active button mapping; per profile. */
	struct lachesis_buttonmappings buttons[LACHESIS_NR_PROFILES];
};


/* A list of physical buttons on the device. */
static struct razer_button lachesis_physical_buttons[] = {
	{ .id = LACHESIS_PHYSBUT_LEFT,		.name = "Leftclick",		},
	{ .id = LACHESIS_PHYSBUT_RIGHT,		.name = "Rightclick",		},
	{ .id = LACHESIS_PHYSBUT_MIDDLE,	.name = "Middleclick",		},
	{ .id = LACHESIS_PHYSBUT_LFRONT,	.name = "Leftside front",	},
	{ .id = LACHESIS_PHYSBUT_LREAR,		.name = "Leftside rear",	},
	{ .id = LACHESIS_PHYSBUT_RFRONT,	.name = "Rightside front",	},
	{ .id = LACHESIS_PHYSBUT_RREAR,		.name = "Rightside rear",	},
	{ .id = LACHESIS_PHYSBUT_TFRONT,	.name = "Top front",		},
	{ .id = LACHESIS_PHYSBUT_TREAR,		.name = "Top rear",		},
	{ .id = LACHESIS_PHYSBUT_SCROLLUP,	.name = "Scroll up",		},
	{ .id = LACHESIS_PHYSBUT_SCROLLDOWN,	.name = "Scroll down",		},
};

/* A list of possible button functions. */
static struct razer_button_function lachesis_button_functions[] = {
	{ .id = LACHESIS_BUTFUNC_LEFT,		.name = "Leftclick",				},
	{ .id = LACHESIS_BUTFUNC_RIGHT,		.name = "Rightclick",				},
	{ .id = LACHESIS_BUTFUNC_MIDDLE,	.name = "Middleclick",				},
	{ .id = LACHESIS_BUTFUNC_PROFDOWN,	.name = "Profile switch down",			},
	{ .id = LACHESIS_BUTFUNC_PROFUP,	.name = "Profile switch up",			},
	{ .id = LACHESIS_BUTFUNC_DPIUP,		.name = "DPI switch up",			},
	{ .id = LACHESIS_BUTFUNC_DPIDOWN,	.name = "DPI switch down",			},
	{ .id = LACHESIS_BUTFUNC_DPI1,		.name = "Select first scan resolution",		},
	{ .id = LACHESIS_BUTFUNC_DPI2,		.name = "Select second scan resolution",	},
	{ .id = LACHESIS_BUTFUNC_DPI3,		.name = "Select third scan resolution",		},
	{ .id = LACHESIS_BUTFUNC_DPI4,		.name = "Select fourth scan resolution",	},
	{ .id = LACHESIS_BUTFUNC_DPI5,		.name = "Select fifth scan resolution",		},
	{ .id = LACHESIS_BUTFUNC_WIN5,		.name = "Windows Button 5",			},
	{ .id = LACHESIS_BUTFUNC_WIN4,		.name = "Windows Button 4",			},
	{ .id = LACHESIS_BUTFUNC_SCROLLUP,	.name = "Scroll up",				},
	{ .id = LACHESIS_BUTFUNC_SCROLLDOWN,	.name = "Scroll down",				},
};
/* TODO: There are more functions */


#define LACHESIS_USB_TIMEOUT	3000

/*XXX: read commands:
 *
 *	CLEAR_FEATURE
 *		0x10:	Read DPI map
 *		0x06:	read fw version
 *		0x05:	LEDs
 *		0x09:	cur profile
 *		0x03:	profile config
 *		0x02:	?busy flag?
 *
 *	write commands:
 *	SET_CONFIG
 *		0x0F:	? Data=0x01. Executed on startup
 *
 * TODO: react to profile/dpi/whatever changes via hw buttons. need to poll?
 */

static int lachesis_usb_write(struct lachesis_private *priv,
			      int request, int command,
			      void *buf, size_t size, bool silent)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, 0,
			      buf, size,
			      LACHESIS_USB_TIMEOUT);
	if (err != size) {
		if (!silent) {
			razer_error("hw_lachesis: usb_write failed (%s)\n",
			usb_strerror());
		}
		return err;
	}

	return 0;
}

static int lachesis_usb_read_withindex(struct lachesis_private *priv,
				       int request, int command, int index,
				       void *buf, size_t size, bool silent)
{
	int err;

	err = usb_control_msg(priv->usb.h,
			      USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			      request, command, index,
			      buf, size,
			      LACHESIS_USB_TIMEOUT);
	if (err != size) {
		if (!silent) {
			razer_error("hw_lachesis: usb_read failed (%s)\n",
				usb_strerror());
		}
		return err;
	}

	return 0;
}

static int lachesis_usb_read(struct lachesis_private *priv,
			     int request, int command,
			     void *buf, size_t size, bool silent)
{
	return lachesis_usb_read_withindex(priv, request, command, 0, buf, size, silent);
}

static int lachesis_read_fw_ver(struct lachesis_private *priv)
{
	char buf[2];
	uint16_t ver;
	int err;

	err = lachesis_usb_read(priv, USB_REQ_CLEAR_FEATURE,
				0x06, buf, sizeof(buf), 0);
	if (err)
		return -EIO;
	ver = buf[0];
	ver <<= 8;
	ver |= buf[1];

	return ver;
}

static int lachesis_commit(struct lachesis_private *priv)
{
	unsigned int i;
	int err;
	char value;
	char statusbuf[2];
	struct lachesis_profcfg_cmd profcfg;
	struct lachesis_dpimap_cmd dpimap;

	/* Commit the profile configuration. */
	for (i = 0; i < LACHESIS_NR_PROFILES; i++) {
		memset(&profcfg, 0, sizeof(profcfg));
		profcfg.packetlength = cpu_to_le16(sizeof(profcfg));
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
		profcfg.buttons = priv->buttons[i];
		profcfg.checksum = razer_xor16_checksum(&profcfg,
				sizeof(profcfg) - sizeof(profcfg.checksum));
		err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
					 0x01, &profcfg, sizeof(profcfg), 0);
		if (err)
			return err;
razer_msleep(1000);
		err = lachesis_usb_read(priv, USB_REQ_CLEAR_FEATURE,
					0x02, statusbuf, sizeof(statusbuf), 0);
		if (err)
			printf("STATUS ERROR\n");
razer_msleep(50);
	}

	/* Commit LED states. */
	value = 0;
	if (priv->led_states[LACHESIS_LED_LOGO])
		value |= 0x01;
	if (priv->led_states[LACHESIS_LED_SCROLL])
		value |= 0x02;
	err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				 0x04, &value, sizeof(value), 0);
	if (err)
		return err;

	/* Commit the active profile selection. */
	value = priv->cur_profile->nr + 1;
	err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				 0x08, &value, sizeof(value), 0);
	if (err)
		return err;

	/* Commit the DPI map. */
	memset(&dpimap, 0, sizeof(dpimap));
	for (i = 0; i < LACHESIS_NR_DPIMAPPINGS; i++) {
		dpimap.mappings[i].magic = LACHESIS_DPIMAPPING_MAGIC;
		dpimap.mappings[i].dpival0 = (priv->dpimappings[i].res / 125) - 1;
		dpimap.mappings[i].dpival1 = dpimap.mappings[i].dpival0;
	}
	err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				 0x12, &dpimap, sizeof(dpimap), 0);
	if (err)
		return err;

	return 0;
}

#define DEFINE_DEF_BUTMAP(phys, func)		\
	{ .physical = LACHESIS_PHYSBUT_##phys,	\
	  .logical = LACHESIS_BUTFUNC_##func,	\
	}
static const struct lachesis_buttonmappings lachesis_default_buttonmap = {
	.mappings = {
		DEFINE_DEF_BUTMAP(LEFT, LEFT),
		DEFINE_DEF_BUTMAP(RIGHT, RIGHT),
		DEFINE_DEF_BUTMAP(MIDDLE, MIDDLE),
		DEFINE_DEF_BUTMAP(LFRONT, WIN5),
		DEFINE_DEF_BUTMAP(LREAR, WIN4),
		DEFINE_DEF_BUTMAP(RFRONT, PROFUP),
		DEFINE_DEF_BUTMAP(RREAR, PROFDOWN),
		DEFINE_DEF_BUTMAP(TFRONT, DPIUP),
		DEFINE_DEF_BUTMAP(TREAR, DPIDOWN),
		DEFINE_DEF_BUTMAP(SCROLLUP, SCROLLUP),
		DEFINE_DEF_BUTMAP(SCROLLDOWN, SCROLLDOWN),
	},
};

static int lachesis_read_config_from_hw(struct lachesis_private *priv)
{
	int err;
	unsigned char value;
	unsigned int i;
	struct lachesis_dpimap_cmd dpimap;
	struct lachesis_profcfg_cmd profcfg;

	/* Assign sane defaults for config values that might fail. */
	priv->cur_profile = &priv->profiles[0];
	for (i = 0; i < LACHESIS_NR_DPIMAPPINGS; i++)
		priv->cur_dpimapping[i] = &priv->dpimappings[0];
	for (i = 0; i < LACHESIS_NR_PROFILES; i++) {
		priv->cur_freq[i] = RAZER_MOUSE_FREQ_1000HZ;
		priv->buttons[i] = lachesis_default_buttonmap;
	}

	value = 0x01;
	err = lachesis_usb_write(priv, USB_REQ_SET_CONFIGURATION,
				 0x0F, &value, sizeof(value), 0);
	if (err)
		return err;
razer_msleep(100);

//printf("Read prof\n");
	/* Get the current profile number */
	err = lachesis_usb_read(priv, USB_REQ_CLEAR_FEATURE,
				0x09, &value, sizeof(value), 1);
	if (err)
		return err;
razer_msleep(3000);
	if (value >= 1 && value <= LACHESIS_NR_PROFILES) {
		/* Got a valid current profile number. */
		priv->cur_profile = &priv->profiles[value - 1];
//printf("Read prof conf\n");
		/* Get the profile configuration */
		for (i = 0; i < LACHESIS_NR_PROFILES; i++) {
			err = lachesis_usb_read_withindex(priv, USB_REQ_CLEAR_FEATURE,
							  0x03, i + 1, &profcfg, sizeof(profcfg), 0);
//razer_msleep(5000);
			if (err) {
//printf("profcfg %u read err\n", i+1);
				continue;
			}
//			printf("Got prof config %u\n", i+1);
			if (profcfg.dpisel < 1 || profcfg.dpisel > LACHESIS_NR_DPIMAPPINGS)
				continue;
//			printf("profcfg valid\n");
//printf("Got magic = 0x%04X, prof %u, freq %u\n", profcfg.magic, profcfg.profile, profcfg.freq);
			priv->cur_dpimapping[i] = &priv->dpimappings[profcfg.dpisel - 1];
			switch (profcfg.freq) {
			case 1:
				priv->cur_freq[i] = RAZER_MOUSE_FREQ_1000HZ;
				break;
			case 2:
				priv->cur_freq[i] = RAZER_MOUSE_FREQ_500HZ;
				break;
			case 3:
				priv->cur_freq[i] = RAZER_MOUSE_FREQ_125HZ;
				break;
			default:
				razer_error("hw_lachesis: "
					"Read invalid frequency value from device (%u)\n",
					profcfg.freq);
				return -EINVAL;
			}
			priv->buttons[i] = profcfg.buttons;
			//TODO validate buttonmap

//XXX
#if 0
		err = lachesis_read_fw_ver(priv);
		if (err < 0)
			printf("FW VER READ FAILED\n");
		priv->fw_version = err;
#endif
//XXX
		}
	}

	/* Get the LED states */
	err = lachesis_usb_read(priv, USB_REQ_CLEAR_FEATURE,
				0x05, &value, sizeof(value), 0);
	if (err)
		return err;
	priv->led_states[LACHESIS_LED_LOGO] = !!(value & 0x01);
	priv->led_states[LACHESIS_LED_SCROLL] = !!(value & 0x02);

razer_msleep(300);
	/* Get the DPI map */
	err = lachesis_usb_read(priv, USB_REQ_CLEAR_FEATURE,
				0x10, &dpimap, sizeof(dpimap), 0);
	if (err)
		return err;
	for (i = 0; i < LACHESIS_NR_DPIMAPPINGS; i++)
		priv->dpimappings[i].res = (dpimap.mappings[i].dpival0 + 1) * 125;

	return 0;
}

static int lachesis_claim(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	return razer_generic_usb_claim_refcount(&priv->usb, &priv->claimed);
}

static void lachesis_release(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	return razer_generic_usb_release_refcount(&priv->usb, &priv->claimed);
}

static int lachesis_get_fw_version(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	/* Version is read on claim. */
	if (!priv->claimed)
		return -EBUSY;
	return priv->fw_version;
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

static int lachesis_supported_axes(struct razer_mouse *m,
				   struct razer_axis **axes_list)
{
	struct lachesis_private *priv = m->internal;

	*axes_list = priv->axes;

	return ARRAY_SIZE(priv->axes);
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

static struct razer_mouse_dpimapping * lachesis_get_dpimapping(struct razer_mouse_profile *p,
							       struct razer_axis *axis)
{
	struct lachesis_private *priv = p->mouse->internal;

	if (p->nr >= ARRAY_SIZE(priv->cur_dpimapping))
		return NULL;

	return priv->cur_dpimapping[p->nr];
}

static int lachesis_set_dpimapping(struct razer_mouse_profile *p,
				   struct razer_axis *axis,
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

static int lachesis_supported_buttons(struct razer_mouse *m,
				      struct razer_button **res_ptr)
{
	*res_ptr = lachesis_physical_buttons;
	return ARRAY_SIZE(lachesis_physical_buttons);
}

static int lachesis_supported_button_functions(struct razer_mouse *m,
					       struct razer_button_function **res_ptr)
{
	*res_ptr = lachesis_button_functions;
	return ARRAY_SIZE(lachesis_button_functions);
}

static struct razer_button_function * lachesis_get_button_function(struct razer_mouse_profile *p,
								   struct razer_button *b)
{
	struct lachesis_private *priv = p->mouse->internal;
	struct lachesis_buttonmappings *m;
	unsigned int i, j;

	if (p->nr > ARRAY_SIZE(priv->buttons))
		return NULL;
	m = &priv->buttons[p->nr];

	for (i = 0; i < ARRAY_SIZE(m->mappings); i++) {
		if (m->mappings[i].physical == b->id) {
			for (j = 0; j < ARRAY_SIZE(lachesis_button_functions); j++) {
				if (lachesis_button_functions[j].id == m->mappings[i].logical)
					return &lachesis_button_functions[j];
			}
		}
	}

	return NULL;
}

static int lachesis_set_button_function(struct razer_mouse_profile *p,
					struct razer_button *b,
					struct razer_button_function *f)
{
	struct lachesis_private *priv = p->mouse->internal;
	struct lachesis_buttonmappings *m;
	unsigned int i;
	uint8_t oldlogical;
	int err;

	if (!priv->claimed)
		return -EBUSY;
	if (p->nr > ARRAY_SIZE(priv->buttons))
		return -EINVAL;
	m = &priv->buttons[p->nr];

	for (i = 0; i < ARRAY_SIZE(m->mappings); i++) {
		if (m->mappings[i].physical == b->id) {
			oldlogical = m->mappings[i].logical;
			m->mappings[i].logical = f->id;
			err = lachesis_commit(priv);
			if (err) {
				m->mappings[i].logical = oldlogical;
				return err;
			}
			return 0;
		}
	}

	return -ENODEV;
}

void razer_lachesis_gen_idstr(struct usb_device *udev, char *buf)
{
	razer_generic_usb_gen_idstr(udev, NULL, "Lachesis", 1, buf);
}

void razer_lachesis_assign_usb_device(struct razer_mouse *m,
				      struct usb_device *usbdev)
{
	struct lachesis_private *priv = m->internal;

	priv->usb.dev = usbdev;
}

int razer_lachesis_init(struct razer_mouse *m,
			struct usb_device *usbdev)
{
	struct lachesis_private *priv;
	unsigned int i;
	int err, fwver;

	BUILD_BUG_ON(sizeof(struct lachesis_profcfg_cmd) != 0x18C);
	BUILD_BUG_ON(sizeof(struct lachesis_dpimap_cmd) != 0x60);

	priv = zalloc(sizeof(struct lachesis_private));
	if (!priv)
		return -ENOMEM;
	m->internal = priv;
	razer_lachesis_assign_usb_device(m, usbdev);

	for (i = 0; i < LACHESIS_NR_PROFILES; i++) {
		priv->profiles[i].nr = i;
		priv->profiles[i].get_freq = lachesis_get_freq;
		priv->profiles[i].set_freq = lachesis_set_freq;
		priv->profiles[i].get_dpimapping = lachesis_get_dpimapping;
		priv->profiles[i].set_dpimapping = lachesis_set_dpimapping;
		priv->profiles[i].get_button_function = lachesis_get_button_function;
		priv->profiles[i].set_button_function = lachesis_set_button_function;
		priv->profiles[i].mouse = m;
	}
	for (i = 0; i < LACHESIS_NR_AXES; i++) {
		priv->axes[i].id = i;
		switch (i) {
		case 0: /* X */
			priv->axes[i].name = "X";
			break;
		case 1: /* Y */
			priv->axes[i].name = "Y";
			break;
		case 2: /* Scrollwheel */
			priv->axes[i].name = "Scroll";
			break;
		}
	}
	for (i = 0; i < LACHESIS_NR_DPIMAPPINGS; i++) {
		priv->dpimappings[i].nr = i;
		priv->dpimappings[i].res = RAZER_MOUSE_RES_UNKNOWN;
		priv->dpimappings[i].change = lachesis_dpimapping_modify;
		priv->dpimappings[i].mouse = m;
	}

	err = lachesis_claim(m);
	if (err) {
		razer_error("hw_lachesis: "
			    "Failed to initially claim the device\n");
		goto err_free;
	}
	fwver = lachesis_read_fw_ver(priv);
	if (fwver < 0) {
		razer_error("hw_lachesis: Failed to get firmware version\n");
		err = fwver;
		goto err_release;
	}
	priv->fw_version = fwver;

	err = lachesis_read_config_from_hw(priv);
	if (err) {
		razer_error("hw_lachesis: "
			    "Failed to read the configuration from hardware\n");
		goto err_release;
	}
	razer_generic_usb_gen_idstr(usbdev, priv->usb.h, "Lachesis", 1, m->idstr);

	m->type = RAZER_MOUSETYPE_LACHESIS;

	m->claim = lachesis_claim;
	m->release = lachesis_release;
	m->get_fw_version = lachesis_get_fw_version;
	m->get_leds = lachesis_get_leds;
	m->nr_profiles = ARRAY_SIZE(priv->profiles);
	m->get_profiles = lachesis_get_profiles;
	m->get_active_profile = lachesis_get_active_profile;
	m->set_active_profile = lachesis_set_active_profile;
	m->supported_axes = lachesis_supported_axes;
	m->supported_resolutions = lachesis_supported_resolutions;
	m->supported_freqs = lachesis_supported_freqs;
	m->supported_dpimappings = lachesis_supported_dpimappings;
	m->supported_buttons = lachesis_supported_buttons;
	m->supported_button_functions = lachesis_supported_button_functions;

	err = lachesis_commit(priv);
	if (err) {
		razer_error("hw_lachesis: Failed to commit initial settings\n");
		goto err_release;
	}
	lachesis_release(m);

	return 0;

err_release:
	lachesis_release(m);
err_free:
	free(priv);

	return err;
}

void razer_lachesis_release(struct razer_mouse *m)
{
	struct lachesis_private *priv = m->internal;

	while (priv->claimed)
		lachesis_release(m);
	free(priv);
}
