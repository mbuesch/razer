/*
 *   Lowlevel hardware access for the
 *   Razer Synapse wire protocol
 *
 *   Important notice:
 *   This hardware driver is based on reverse engineering, only.
 *
 *   Copyright (C) 2012 Michael Buesch <m@bues.ch>
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
#include "synapse.h"
#include "razer_private.h"
#include "util.h"
#include "buttonmapping.h"


enum synapse_constants {
	SYNAPSE_NR_PROFILES		= 5,
	SYNAPSE_NR_DPIMAPPINGS		= 5,
	SYNAPSE_NR_AXES			= 3,
	SYNAPSE_NR_LEDS			= 2,
	SYNAPSE_SERIAL_MAX_LEN		= 32,
	SYNAPSE_PROFNAME_MAX_LEN	= 20,
};

enum synapse_phys_button {
	/* Physical button IDs */
	SYNAPSE_PHYSBUT_LEFT = 0x01,	/* Left button */
	SYNAPSE_PHYSBUT_RIGHT,		/* Right button */
	SYNAPSE_PHYSBUT_MIDDLE,		/* Middle button */
	SYNAPSE_PHYSBUT_LFRONT,		/* Left side, front button */
	SYNAPSE_PHYSBUT_LREAR,		/* Left side, rear button */
	SYNAPSE_PHYSBUT_RFRONT,		/* Right side, front button */
	SYNAPSE_PHYSBUT_RREAR,		/* Right side, rear button */
	SYNAPSE_PHYSBUT_TFRONT,		/* Top side, front button */
	SYNAPSE_PHYSBUT_TREAR,		/* Top side, rear button */
	SYNAPSE_PHYSBUT_SCROLLUP,	/* Scroll wheel up */
	SYNAPSE_PHYSBUT_SCROLLDWN,	/* Scroll wheel down */

	NR_SYNAPSE_PHYSBUT = 11,	/* Number of physical buttons */
};

struct synapse_request {
	uint8_t magic;
	uint8_t flags;
	uint8_t rw;
	uint8_t command;
	uint8_t request;
	uint8_t _padding[3];
	uint8_t payload[80];
	le16_t checksum;
} _packed;

#define SYNAPSE_REQ_MAGIC		0x01
#define SYNAPSE_REQ_FLG_TRANSOK		0x02
#define SYNAPSE_REQ_READ		0x01
#define SYNAPSE_REQ_WRITE		0x00

struct synapse_request_devinfo {
	uint8_t serial[SYNAPSE_SERIAL_MAX_LEN];
	uint8_t fwver[2];
} _packed;

struct synapse_request_globconfig {
	uint8_t profile;
	uint8_t freq;
	uint8_t dpisel;
	uint8_t dpival0;
	uint8_t dpival1;
} _packed;

struct synapse_request_profname {
	uint8_t profile;
	union {
		uint8_t name_raw[SYNAPSE_PROFNAME_MAX_LEN * 2]; /* UTF-16-LE */
		le16_t name_le16[SYNAPSE_PROFNAME_MAX_LEN];
	} _packed;
} _packed;

struct synapse_one_dpimapping {
	uint8_t dpival0;
	uint8_t dpival1;
} _packed;

struct synapse_led_color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t padding;
} _packed;

#define SYNAPSE_LED_COLOR_PADDING	0xFF

struct synapse_request_hwconfig {
	uint8_t profile;
	uint8_t leds;
	uint8_t dpisel;
	uint8_t nr_dpimappings;
	struct synapse_one_dpimapping dpimappings[SYNAPSE_NR_DPIMAPPINGS];
	uint8_t _padding[6];
	uint8_t buttonmap[4 * NR_SYNAPSE_PHYSBUT];
	struct synapse_led_color led_colors[SYNAPSE_NR_LEDS];
} _packed;


struct synapse_buttons {
	struct razer_buttonmapping mapping[NR_SYNAPSE_PHYSBUT];
};

struct synapse_prof_name {
	razer_utf16_t name[SYNAPSE_PROFNAME_MAX_LEN + 1];
};

struct synapse_led_name {
	char name[32];
};

struct razer_synapse {
	struct razer_mouse *m;

	/* Feature selection bits */
	unsigned int features;

	/* Firmware version */
	uint16_t fw_version;
	/* Device serial number */
	char serial[SYNAPSE_SERIAL_MAX_LEN + 1];

	/* LED names */
	struct synapse_led_name led_names[SYNAPSE_NR_LEDS];
	/* The currently set LED states. */
	enum razer_led_state led_states[SYNAPSE_NR_PROFILES][SYNAPSE_NR_LEDS];
	/* LED colors */
	struct razer_rgb_color led_colors[SYNAPSE_NR_PROFILES][SYNAPSE_NR_LEDS];

	/* The active profile. */
	struct razer_mouse_profile *cur_profile;
	/* Profile configuration (one per profile). */
	struct razer_mouse_profile profiles[SYNAPSE_NR_PROFILES];
	/* Profile names */
	struct synapse_prof_name profile_names[SYNAPSE_NR_PROFILES];

	/* Supported mouse axes */
	struct razer_axis axes[SYNAPSE_NR_AXES];

	/* The active DPI mapping; per profile. */
	struct razer_mouse_dpimapping *cur_dpimapping[SYNAPSE_NR_PROFILES];
	/* The possible DPI mappings. */
	struct razer_mouse_dpimapping dpimappings[SYNAPSE_NR_PROFILES][SYNAPSE_NR_DPIMAPPINGS];

	/* The active scan frequency. */
	enum razer_mouse_freq cur_freq;

	/* The active button mapping; per profile. */
	struct synapse_buttons buttons[SYNAPSE_NR_PROFILES];

	bool commit_pending;
};


/* A list of physical buttons on the device. */
static struct razer_button synapse_physical_buttons[] = {
	{ .id = SYNAPSE_PHYSBUT_LEFT,		.name = "Leftclick",		},
	{ .id = SYNAPSE_PHYSBUT_RIGHT,		.name = "Rightclick",		},
	{ .id = SYNAPSE_PHYSBUT_MIDDLE,		.name = "Middleclick",		},
	{ .id = SYNAPSE_PHYSBUT_LFRONT,		.name = "Leftside front",	},
	{ .id = SYNAPSE_PHYSBUT_LREAR,		.name = "Leftside rear",	},
	{ .id = SYNAPSE_PHYSBUT_RFRONT,		.name = "Rightside front",	},
	{ .id = SYNAPSE_PHYSBUT_RREAR,		.name = "Rightside rear",	},
	{ .id = SYNAPSE_PHYSBUT_TFRONT,		.name = "Top front",		},
	{ .id = SYNAPSE_PHYSBUT_TREAR,		.name = "Top rear",		},
	{ .id = SYNAPSE_PHYSBUT_SCROLLUP,	.name = "Scroll up",		},
	{ .id = SYNAPSE_PHYSBUT_SCROLLDWN,	.name = "Scroll down",		},
};

/* A list of possible button functions. */
static struct razer_button_function synapse_button_functions[] = {
	BUTTONFUNC_LEFT,
	BUTTONFUNC_RIGHT,
	BUTTONFUNC_MIDDLE,
	BUTTONFUNC_PROFDOWN,
	BUTTONFUNC_PROFUP,
	BUTTONFUNC_DPIUP,
	BUTTONFUNC_DPIDOWN,
	BUTTONFUNC_DPI1,
	BUTTONFUNC_DPI2,
	BUTTONFUNC_DPI3,
	BUTTONFUNC_DPI4,
	BUTTONFUNC_DPI5,
	BUTTONFUNC_WIN5,
	BUTTONFUNC_WIN4,
	BUTTONFUNC_SCROLLUP,
	BUTTONFUNC_SCROLLDWN,
};


/* requests:
 *	description:		command, request / payload
 *	==================================================
 *
 *	get device info:	02, 01
 *		->reply:	02, 22 / struct synapse_request_devinfo
 *
 *	get global config:	05, 01	or
 *				05, 00
 *		->reply:	05, 05 / struct synapse_request_globconfig
 *
 *	set global config:	05, 05
 *
 *	get prof name:		22, 01 / profnr
 *		->reply:	22, 29 / struct synapse_request_profname
 *
 *	set prof name:		22, 29 / struct synapse_request_profile
 *
 *	get hwconfig:		06, 01 / profnr
 *		->reply:	06, 38 / struct synapse_request_hwconfig
 *
 *	set hwconfig:		06, 48 / struct synapse_request_hwconfig
 *
 *	get ???:		08, 00
 *		->reply:	08, 04 / all zero
 */

static le16_t synapse_checksum(const struct synapse_request *req)
{
	uint16_t checksum;

	checksum = razer_xor8_checksum((uint8_t *)req + 2,
				       sizeof(*req) - 4);
	if (!(req->flags & SYNAPSE_REQ_FLG_TRANSOK))
		checksum |= 0x100;

	return cpu_to_le16(checksum);
}

static int synapse_usb_write(struct razer_synapse *s,
			     int request, int command, int index,
			     void *buf, size_t size)
{
	int err;

	err = libusb_control_transfer(
		s->m->usb_ctx->h,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, index,
		buf, size,
		RAZER_USB_TIMEOUT);
	if (err != size) {
		razer_error("synapse: usb_write failed\n");
		return -EIO;
	}
	razer_msleep(5);

	return 0;
}

static int synapse_usb_read(struct razer_synapse *s,
			    int request, int command, int index,
			    void *buf, size_t size)
{
	int err;

	err = libusb_control_transfer(
		s->m->usb_ctx->h,
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE,
		request, command, index,
		buf, size,
		RAZER_USB_TIMEOUT);
	if (err != size) {
		razer_error("synapse: usb_read failed\n");
		return -EIO;
	}
	razer_msleep(5);

	return 0;
}

static int synapse_request_send(struct razer_synapse *s,
				const struct synapse_request *_req)
{
	struct synapse_request req = *_req;

	req.magic = SYNAPSE_REQ_MAGIC;
	req.checksum = synapse_checksum(&req);
//	razer_dump("WR", &req, sizeof(req));

	return synapse_usb_write(s, LIBUSB_REQUEST_SET_CONFIGURATION,
				 0x300, 0, &req, sizeof(req));
}

static int synapse_request_receive(struct razer_synapse *s,
				   struct synapse_request *req,
				   bool do_checksum)
{
	le16_t checksum;
	int err;

	memset(req, 0, sizeof(*req));
	err = synapse_usb_read(s, LIBUSB_REQUEST_CLEAR_FEATURE,
			       0x300, 0, req, sizeof(*req));
	if (err)
		return err;
//	razer_dump("RD", req, sizeof(*req));
	if (do_checksum) {
		checksum = synapse_checksum(req);
		if (req->checksum != checksum) {
			razer_error("synapse: Received request with invalid "
				    "checksum (was 0x%04X, expected 0x%04X)\n",
				    le16_to_cpu(req->checksum),
				    le16_to_cpu(checksum));
			return -EIO;
		}
	}

	return 0;
}

static int synapse_request_write(struct razer_synapse *s,
				 uint8_t command, uint8_t request,
				 const void *payload, size_t payload_len)
{
	struct synapse_request req, nullreq;
	int err;

	if (WARN_ON(payload_len > sizeof(req.payload)))
		return -EINVAL;

	memset(&req, 0, sizeof(req));
	req.rw = SYNAPSE_REQ_WRITE;
	req.command = command;
	req.request = request;
	if (payload)
		memcpy(req.payload, payload, payload_len);
	err = synapse_request_send(s, &req);
	if (err)
		return err;
	err = synapse_request_receive(s, &req, 0);
	if (err)
		return err;
	memset(&nullreq, 0, sizeof(nullreq));
	err = synapse_request_send(s, &nullreq);
	if (err)
		return err;

	if (req.magic != SYNAPSE_REQ_MAGIC) {
		razer_error("synapse: Invalid magic on sent request\n");
		return -EIO;
	}
	if (req.rw != SYNAPSE_REQ_WRITE) {
		razer_error("synapse: Invalid rw flag on sent request\n");
		return -EIO;
	}
	if (req.command != command || req.request != request) {
		razer_error("synapse: Invalid command on sent request\n");
		return -EIO;
	}

	return 0;
}

static int synapse_request_read(struct razer_synapse *s,
				uint8_t command, uint8_t request,
				void *payload, size_t payload_len)
{
	struct synapse_request req, nullreq;
	int err;

	if (WARN_ON(payload_len > sizeof(req.payload)))
		return -EINVAL;

	memset(&req, 0, sizeof(req));
	req.rw = SYNAPSE_REQ_READ;
	req.command = command;
	req.request = request;
	if (payload)
		memcpy(req.payload, payload, payload_len);
	err = synapse_request_send(s, &req);
	if (err)
		return err;
	err = synapse_request_receive(s, &req, 0);
	if (err)
		return err;
	memset(&nullreq, 0, sizeof(nullreq));
	err = synapse_request_send(s, &nullreq);
	if (err)
		return err;
	if (payload)
		memcpy(payload, req.payload, payload_len);

	if (req.magic != SYNAPSE_REQ_MAGIC) {
		razer_error("synapse: Invalid magic on received request\n");
		return -EIO;
	}
	if (!(req.flags & SYNAPSE_REQ_FLG_TRANSOK)) {
		razer_error("synapse: Failed to receive request. (TRANSOK flag)\n");
		return -EIO;
	}
	if (req.rw != SYNAPSE_REQ_READ) {
		razer_error("synapse: Invalid rw flag on received request\n");
		return -EIO;
	}
	if (req.command != command) {
		razer_error("synapse: Invalid command on received request\n");
		return -EIO;
	}

	return 0;
}

static int synapse_read_config_from_hw(struct razer_synapse *s)
{
	unsigned int i, j;
	int err;
	struct synapse_request_profname profname;
	struct synapse_request_globconfig globconfig;
	struct synapse_request_hwconfig hwconfig;
	enum razer_mouse_res res_x, res_y;

	/* Get global config */
	memset(&globconfig, 0, sizeof(globconfig));
	err = synapse_request_read(s, 5, 1,
				   &globconfig, sizeof(globconfig));
	if (err)
		return err;
	if (globconfig.profile < 1 || globconfig.profile > SYNAPSE_NR_PROFILES) {
		razer_error("synapse: Got invalid profile number\n");
		return -EIO;
	}
	s->cur_profile = &s->profiles[globconfig.profile - 1];
	switch (globconfig.freq) {
	case 1:
		s->cur_freq = RAZER_MOUSE_FREQ_1000HZ;
		break;
	case 2:
		s->cur_freq = RAZER_MOUSE_FREQ_500HZ;
		break;
	case 8:
		s->cur_freq = RAZER_MOUSE_FREQ_125HZ;
		break;
	default:
		razer_error("synapse: "
			"Read invalid frequency value from device (%u)\n",
			globconfig.freq);
		return -EIO;
	}

	/* Get the profile names */
	for (i = 0; i < SYNAPSE_NR_PROFILES; i++) {
		memset(&profname, 0, sizeof(profname));
		profname.profile = i + 1;
		err = synapse_request_read(s, 0x22, 1,
					   &profname, sizeof(profname));
		if (err)
			return err;
		memset(&s->profile_names[i], 0, sizeof(s->profile_names[i]));
		for (j = 0; j < SYNAPSE_PROFNAME_MAX_LEN; j++) {
			s->profile_names[i].name[j] = profname.name_raw[j * 2 + 0];
			s->profile_names[i].name[j] |= (uint16_t)profname.name_raw[j * 2 + 1] << 8;
		}
	}

	/* Get the profile configs */
	for (i = 0; i < SYNAPSE_NR_PROFILES; i++) {
		memset(&hwconfig, 0, sizeof(hwconfig));
		hwconfig.profile = i + 1;
		err = synapse_request_read(s, 6, 1,
					   &hwconfig, sizeof(hwconfig));
		if (err)
			return err;
		if (hwconfig.profile != i + 1) {
			razer_error("synapse: Failed to read hw config (%u vs %u)\n",
				    hwconfig.profile, i + 1);
			return -EIO;
		}
		for (j = 0; j < SYNAPSE_NR_LEDS; j++)
			s->led_states[i][j] = !!(hwconfig.leds & (1 << j));
		if (hwconfig.dpisel < 1 || hwconfig.dpisel > SYNAPSE_NR_DPIMAPPINGS ||
		    hwconfig.dpisel > hwconfig.nr_dpimappings) {
			razer_error("synapse: Got invalid DPI selection: %u\n",
				    hwconfig.dpisel);
			return -EIO;
		}
		s->cur_dpimapping[i] = &s->dpimappings[i][hwconfig.dpisel - 1];

		if (hwconfig.nr_dpimappings < 1 ||
		    hwconfig.nr_dpimappings > SYNAPSE_NR_DPIMAPPINGS) {
			razer_error("synapse: Got invalid nr_dpimappings: %u\n",
				    hwconfig.nr_dpimappings);
			return -EIO;
		}
		for (j = 0; j < SYNAPSE_NR_DPIMAPPINGS; j++) {
			if (j + 1 > hwconfig.nr_dpimappings) {
				res_x = RAZER_MOUSE_RES_5600DPI;
				res_y = res_x;
			} else {
				res_x = ((hwconfig.dpimappings[j].dpival0 / 4) + 1) * 100;
				res_y = ((hwconfig.dpimappings[j].dpival1 / 4) + 1) * 100;
			}
			s->dpimappings[i][j].res[RAZER_DIM_X] = res_x;
			s->dpimappings[i][j].res[RAZER_DIM_Y] = res_y;
		}
		err = razer_parse_buttonmap(hwconfig.buttonmap, sizeof(hwconfig.buttonmap),
					    s->buttons[i].mapping,
					    ARRAY_SIZE(s->buttons[i].mapping), 2);
		if (err)
			return err;
		for (j = 0; j < SYNAPSE_NR_LEDS; j++) {
			s->led_colors[i][j].r = hwconfig.led_colors[j].r;
			s->led_colors[i][j].g = hwconfig.led_colors[j].g;
			s->led_colors[i][j].b = hwconfig.led_colors[j].b;
			s->led_colors[i][j].valid = !!(s->features & RAZER_SYNFEAT_RGBLEDS);

		}
	}

	return 0;
}

static int synapse_read_devinfo(struct razer_synapse *s)
{
	struct synapse_request_devinfo devinfo;
	int err;

	s->fw_version = 0;
	memset(s->serial, 0, sizeof(s->serial));

	memset(&devinfo, 0, sizeof(devinfo));
	err = synapse_request_read(s, 2, 1,
				   &devinfo, sizeof(devinfo));
	if (err)
		return -EIO;
	s->fw_version = ((uint16_t)(devinfo.fwver[0]) << 8) |
			devinfo.fwver[1];
	memcpy(s->serial, devinfo.serial, SYNAPSE_SERIAL_MAX_LEN);

	return 0;
}

static int synapse_get_fw_version(struct razer_mouse *m)
{
	struct razer_synapse *s = m->synapse_data;

	return s->fw_version;
}

static int synapse_do_commit(struct razer_synapse *s)
{
	struct synapse_request_profname profname;
	struct synapse_request_globconfig globconfig;
	struct synapse_request_hwconfig hwconfig;
	int err;
	unsigned int i, j;

	/* Commit profile configs */
	for (i = 0; i < SYNAPSE_NR_PROFILES; i++) {
		memset(&hwconfig, 0, sizeof(hwconfig));
		hwconfig.profile = i + 1;
		hwconfig.leds = 0x04; /* Bit 2 is always set */
		for (j = 0; j < SYNAPSE_NR_LEDS; j++) {
			if (s->led_states[i][j])
				hwconfig.leds |= (1 << j);
		}
		hwconfig.dpisel = (s->cur_dpimapping[i]->nr % 10) + 1;
		hwconfig.nr_dpimappings = SYNAPSE_NR_DPIMAPPINGS;
		for (j = 0; j < SYNAPSE_NR_DPIMAPPINGS; j++) {
			hwconfig.dpimappings[j].dpival0 = ((s->dpimappings[i][j].res[RAZER_DIM_X] / 100) - 1) * 4;
			hwconfig.dpimappings[j].dpival1 = ((s->dpimappings[i][j].res[RAZER_DIM_Y] / 100) - 1) * 4;
		}
		err = razer_create_buttonmap(hwconfig.buttonmap, sizeof(hwconfig.buttonmap),
					     s->buttons[i].mapping,
					     ARRAY_SIZE(s->buttons[i].mapping), 2);
		if (err)
			return err;
		if (s->features & RAZER_SYNFEAT_RGBLEDS) {
			for (j = 0; j < SYNAPSE_NR_LEDS; j++) {
				hwconfig.led_colors[j].padding = SYNAPSE_LED_COLOR_PADDING;
				hwconfig.led_colors[j].r = s->led_colors[i][j].r;
				hwconfig.led_colors[j].g = s->led_colors[i][j].g;
				hwconfig.led_colors[j].b = s->led_colors[i][j].b;
			}
		}
		err = synapse_request_write(s, 6, 0x48,
					    &hwconfig, sizeof(hwconfig));
		if (err)
			return err;
	}

	/* Commit profile names */
	for (i = 0; i < SYNAPSE_NR_PROFILES; i++) {
		memset(&profname, 0, sizeof(profname));
		profname.profile = i + 1;
		for (j = 0; j < SYNAPSE_PROFNAME_MAX_LEN; j++) {
			le16_t c = cpu_to_le16(s->profile_names[i].name[j]);
			profname.name_le16[j] = c;
		}
		err = synapse_request_write(s, 0x22, 0x29,
					    &profname, sizeof(profname));
		if (err)
			return err;
	}

	/* Commit global config */
	memset(&globconfig, 0, sizeof(globconfig));
	globconfig.profile = s->cur_profile->nr + 1;
	switch (s->cur_freq) {
	default:
	case RAZER_MOUSE_FREQ_1000HZ:
		globconfig.freq = 1;
		break;
	case RAZER_MOUSE_FREQ_500HZ:
		globconfig.freq = 2;
		break;
	case RAZER_MOUSE_FREQ_125HZ:
		globconfig.freq = 8;
		break;
	}
	globconfig.dpisel = (s->cur_dpimapping[s->cur_profile->nr]->nr % 10) + 1;
	globconfig.dpival0 = ((s->cur_dpimapping[s->cur_profile->nr]->res[RAZER_DIM_X] / 100) - 1) * 4;
	globconfig.dpival1 = ((s->cur_dpimapping[s->cur_profile->nr]->res[RAZER_DIM_Y] / 100) - 1) * 4;
	err = synapse_request_write(s, 5, 5,
				    &globconfig, sizeof(globconfig));
	if (err)
		return err;

	return 0;
}

static int synapse_commit(struct razer_mouse *m, int force)
{
	struct razer_synapse *s = m->synapse_data;
	int err = 0;

	if (!m->claim_count)
		return -EBUSY;
	if (s->commit_pending || force) {
		err = synapse_do_commit(s);
		if (!err)
			s->commit_pending = 0;
	}

	return err;
}

static const razer_utf16_t * synapse_profile_get_name(struct razer_mouse_profile *p)
{
	struct razer_mouse *m = p->mouse;
	struct razer_synapse *s = m->synapse_data;

	if (p->nr >= SYNAPSE_NR_PROFILES)
		return NULL;

	return s->profile_names[p->nr].name;
}

static int synapse_profile_set_name(struct razer_mouse_profile *p,
				    const razer_utf16_t *new_name)
{
	struct razer_mouse *m = p->mouse;
	struct razer_synapse *s = m->synapse_data;
	int err;

	if (p->nr >= SYNAPSE_NR_PROFILES)
		return -EINVAL;

	if (!m->claim_count)
		return -EBUSY;

	err = razer_utf16_cpy(s->profile_names[p->nr].name,
			      new_name, SYNAPSE_PROFNAME_MAX_LEN);
	s->commit_pending = 1;

	return err;
}

static int synapse_led_toggle(struct razer_led *led,
			      enum razer_led_state new_state)
{
	struct razer_mouse_profile *p = led->u.mouse_prof;
	struct razer_mouse *m = p->mouse;
	struct razer_synapse *s = m->synapse_data;

	if (led->id >= SYNAPSE_NR_LEDS)
		return -EINVAL;
	if ((new_state != RAZER_LED_OFF) &&
	    (new_state != RAZER_LED_ON))
		return -EINVAL;
	if (p->nr >= SYNAPSE_NR_PROFILES)
		return -EINVAL;

	if (!s->m->claim_count)
		return -EBUSY;

	s->led_states[p->nr][led->id] = new_state;
	s->commit_pending = 1;

	return 0;
}

static int synapse_led_change_color(struct razer_led *led,
				    const struct razer_rgb_color *new_color)
{
	struct razer_mouse_profile *p = led->u.mouse_prof;
	struct razer_mouse *m = p->mouse;
	struct razer_synapse *s = m->synapse_data;

	if (led->id >= SYNAPSE_NR_LEDS)
		return -EINVAL;
	if (p->nr >= SYNAPSE_NR_PROFILES)
		return -EINVAL;

	if (!s->m->claim_count)
		return -EBUSY;

	s->led_colors[p->nr][led->id] = *new_color;
	s->commit_pending = 1;

	return 0;
}



static struct razer_mouse_profile * synapse_get_profiles(struct razer_mouse *m)
{
	struct razer_synapse *s = m->synapse_data;

	return &s->profiles[0];
}

static struct razer_mouse_profile * synapse_get_active_profile(struct razer_mouse *m)
{
	struct razer_synapse *s = m->synapse_data;

	return s->cur_profile;
}

static int synapse_set_active_profile(struct razer_mouse *m,
				      struct razer_mouse_profile *p)
{
	struct razer_synapse *s = m->synapse_data;

	if (!s->m->claim_count)
		return -EBUSY;

	s->cur_profile = p;
	s->commit_pending = 1;

	return 0;
}

static int synapse_supported_dpimappings(struct razer_mouse *m,
					 struct razer_mouse_dpimapping **res_ptr)
{
	struct razer_synapse *s = m->synapse_data;

	*res_ptr = &s->dpimappings[0][0];

	return SYNAPSE_NR_PROFILES * SYNAPSE_NR_DPIMAPPINGS;
}

static struct razer_mouse_dpimapping * synapse_get_dpimapping(struct razer_mouse_profile *p,
							      struct razer_axis *axis)
{
	struct razer_synapse *s = p->mouse->synapse_data;

	if (p->nr >= ARRAY_SIZE(s->cur_dpimapping))
		return NULL;

	return s->cur_dpimapping[p->nr];
}

static int synapse_set_dpimapping(struct razer_mouse_profile *p,
				  struct razer_axis *axis,
				  struct razer_mouse_dpimapping *d)
{
	struct razer_synapse *s = p->mouse->synapse_data;
	razer_id_mask_t idmask;

	if (!s->m->claim_count)
		return -EBUSY;
	if (p->nr >= ARRAY_SIZE(s->cur_dpimapping))
		return -EINVAL;

	razer_id_mask_zero(&idmask);
	razer_id_mask_set(&idmask, p->nr);
	if (d->profile_mask != idmask)
		return -EINVAL;

	s->cur_dpimapping[p->nr] = d;
	s->commit_pending = 1;

	return 0;
}

static int synapse_dpimapping_modify(struct razer_mouse_dpimapping *d,
				     enum razer_dimension dim,
				     enum razer_mouse_res res)
{
	struct razer_synapse *s = d->mouse->synapse_data;

	if ((int)dim < 0 || (int)dim >= ARRAY_SIZE(d->res))
		return -EINVAL;

	if (!s->m->claim_count)
		return -EBUSY;

	d->res[dim] = res;
	s->commit_pending = 1;

	return 0;
}

static int synapse_profile_get_leds(struct razer_mouse_profile *p,
				    struct razer_led **leds_list)
{
	struct razer_synapse *s = p->mouse->synapse_data;
	struct razer_led *leds[SYNAPSE_NR_LEDS];
	int i;

	if (p->nr >= SYNAPSE_NR_PROFILES)
		return -EINVAL;

	for (i = 0; i < SYNAPSE_NR_LEDS; i++) {
		leds[i] = zalloc(sizeof(struct razer_led));
		if (!leds[i]) {
			for (i--; i >= 0; i--)
				razer_free(leds[i], sizeof(struct razer_led));
			return -ENOMEM;
		}
	}

	for (i = 0; i < SYNAPSE_NR_LEDS; i++) {
		leds[i]->name = s->led_names[i].name;
		leds[i]->id = i;
		leds[i]->state = s->led_states[p->nr][i];
		leds[i]->toggle_state = synapse_led_toggle;
		if (s->features & RAZER_SYNFEAT_RGBLEDS) {
			leds[i]->color = s->led_colors[p->nr][i];
			leds[i]->change_color = synapse_led_change_color;
		}
		leds[i]->u.mouse_prof = &s->profiles[p->nr];
	}

	/* Link the list */
	*leds_list = leds[0];
	for (i = 0; i < SYNAPSE_NR_LEDS; i++)
		leds[i]->next = (i < SYNAPSE_NR_LEDS - 1) ? leds[i + 1] : NULL;

	return SYNAPSE_NR_LEDS;
}

static int synapse_supported_axes(struct razer_mouse *m,
				  struct razer_axis **axes_list)
{
	struct razer_synapse *s = m->synapse_data;

	*axes_list = s->axes;

	return ARRAY_SIZE(s->axes);
}

static int synapse_supported_freqs(struct razer_mouse *m,
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

static int synapse_supported_resolutions(struct razer_mouse *m,
					 enum razer_mouse_res **res_list)
{
	enum razer_mouse_res *list;
	unsigned int i, count = 0, res, step = 0;

	count = 56;
	step = RAZER_MOUSE_RES_100DPI;

	list = malloc(sizeof(*list) * count);
	if (!list)
		return -ENOMEM;

	res = step;
	for (i = 0; i < count; i++) {
		list[i] = res;
		res += step;
	}

	*res_list = list;

	return count;
}

static int synapse_supported_buttons(struct razer_mouse *m,
				     struct razer_button **res_ptr)
{
	*res_ptr = synapse_physical_buttons;
	return ARRAY_SIZE(synapse_physical_buttons);
}

static int synapse_supported_button_functions(struct razer_mouse *m,
					      struct razer_button_function **res_ptr)
{
	*res_ptr = synapse_button_functions;
	return ARRAY_SIZE(synapse_button_functions);
}

static struct razer_button_function * synapse_get_button_function(struct razer_mouse_profile *p,
								  struct razer_button *b)
{
	struct razer_synapse *s = p->mouse->synapse_data;
	struct synapse_buttons *buttons;

	if (p->nr > ARRAY_SIZE(s->buttons))
		return NULL;
	buttons = &s->buttons[p->nr];

	return razer_get_buttonfunction_by_button(
			buttons->mapping, ARRAY_SIZE(buttons->mapping),
			synapse_button_functions, ARRAY_SIZE(synapse_button_functions),
			b);
}

static int synapse_set_button_function(struct razer_mouse_profile *p,
				       struct razer_button *b,
				       struct razer_button_function *f)
{
	struct razer_synapse *s = p->mouse->synapse_data;
	struct synapse_buttons *buttons;
	struct razer_buttonmapping *mapping;

	if (!s->m->claim_count)
		return -EBUSY;
	if (p->nr > ARRAY_SIZE(s->buttons))
		return -EINVAL;
	buttons = &s->buttons[p->nr];

	mapping = razer_get_buttonmapping_by_physid(
			buttons->mapping, ARRAY_SIZE(buttons->mapping),
			b->id);
	if (!mapping)
		return -ENODEV;

	mapping->logical = f->id;
	s->commit_pending = 1;

	return 0;
}

int razer_synapse_init(struct razer_mouse *m,
		       unsigned int features)
{
	struct razer_synapse *s;
	int i, j, k, err;

	BUILD_BUG_ON(sizeof(struct synapse_request) != 90);
	BUILD_BUG_ON(sizeof(struct synapse_request_devinfo) != 34);
	BUILD_BUG_ON(sizeof(struct synapse_request_globconfig) != 5);
	BUILD_BUG_ON(sizeof(struct synapse_request_profname) != 41);
	BUILD_BUG_ON(sizeof(struct synapse_request_hwconfig) != 72);

	s = zalloc(sizeof(*s));
	if (!s)
		return -ENOMEM;
	m->synapse_data = s;
	s->m = m;

	s->features = features;

	err = razer_usb_add_used_interface(m->usb_ctx, 0, 0);
	if (err) {
		err = -ENODEV;
		goto err_free;
	}

	for (i = 0; i < SYNAPSE_NR_PROFILES; i++) {
		s->profiles[i].nr = i;
		s->profiles[i].get_leds = synapse_profile_get_leds;
		s->profiles[i].get_name = synapse_profile_get_name;
		s->profiles[i].set_name = synapse_profile_set_name;
		s->profiles[i].get_dpimapping = synapse_get_dpimapping;
		s->profiles[i].set_dpimapping = synapse_set_dpimapping;
		s->profiles[i].get_button_function = synapse_get_button_function;
		s->profiles[i].set_button_function = synapse_set_button_function;
		s->profiles[i].mouse = m;
	}

	razer_init_axes(&s->axes[0],
			"X", 0,
			"Y", 0,
			"Scroll", 0);

	for (i = 0; i < SYNAPSE_NR_PROFILES; i++) {
		for (j = 0; j < SYNAPSE_NR_DPIMAPPINGS; j++) {
			s->dpimappings[i][j].nr = i * 10 + j;
			for (k = 0; k < RAZER_NR_DIMS; k++)
				s->dpimappings[i][j].res[k] = RAZER_MOUSE_RES_UNKNOWN;
			s->dpimappings[i][j].dimension_mask = (1 << RAZER_DIM_X) |
							      (1 << RAZER_DIM_Y);
			razer_id_mask_zero(&s->dpimappings[i][j].profile_mask);
			razer_id_mask_set(&s->dpimappings[i][j].profile_mask, i);
			s->dpimappings[i][j].change = synapse_dpimapping_modify;
			s->dpimappings[i][j].mouse = m;
		}
	}

	/* Default LED names */
	razer_synapse_set_led_name(m, 0, "ScrollWheel");
	razer_synapse_set_led_name(m, 1, "GlowingLogo");

	err = m->claim(m);
	if (err) {
		razer_error("synapse: "
			    "Failed to initially claim the device\n");
		goto err_free;
	}
	err = synapse_read_devinfo(s);
	if (err) {
		razer_error("synapse: Failed to get firmware version\n");
		goto err_release;
	}

	err = synapse_read_config_from_hw(s);
	if (err) {
		razer_error("synapse: "
			    "Failed to read the configuration from hardware\n");
		goto err_release;
	}

	m->get_fw_version = synapse_get_fw_version;
	m->commit = synapse_commit;
//TODO	m->global_get_freq = synapse_global_get_freq;
//TODO	m->global_set_freq = synapse_global_set_freq;
	m->nr_profiles = SYNAPSE_NR_PROFILES;
	m->get_profiles = synapse_get_profiles;
	m->get_active_profile = synapse_get_active_profile;
	m->set_active_profile = synapse_set_active_profile;
	m->supported_axes = synapse_supported_axes;
	m->supported_resolutions = synapse_supported_resolutions;
	m->supported_freqs = synapse_supported_freqs;
	m->supported_dpimappings = synapse_supported_dpimappings;
	m->supported_buttons = synapse_supported_buttons;
	m->supported_button_functions = synapse_supported_button_functions;

	err = synapse_do_commit(s);
	if (err) {
		razer_error("synapse: Failed to commit initial settings\n");
		goto err_release;
	}
	m->release(m);

	return 0;

err_release:
	m->release(m);
err_free:
	razer_free(s, sizeof(*s));

	return err;
}

void razer_synapse_exit(struct razer_mouse *m)
{
	struct razer_synapse *s = m->synapse_data;

	razer_free(s, sizeof(*s));
	m->synapse_data = NULL;
}

int razer_synapse_set_led_name(struct razer_mouse *m,
			       unsigned int index,
			       const char *name)
{
	struct razer_synapse *s = m->synapse_data;

	if (index >= ARRAY_SIZE(s->led_names))
		return -EINVAL;

	memset(&s->led_names[index], 0, sizeof(s->led_names[index]));
	strncpy(s->led_names[index].name, name,
		sizeof(s->led_names[index].name) - 1);

	return 0;
}

const char * razer_synapse_get_serial(struct razer_mouse *m)
{
	struct razer_synapse *s = m->synapse_data;

	return s->serial;
}
