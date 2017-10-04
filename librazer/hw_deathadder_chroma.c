/*
 * Lowlevel hardware access for the Razer DeathAdder Chroma mouse.
 *
 * Important notice:
 * This hardware driver is based on reverse engineering, only.
 *
 * Copyright (C) 2015 Konrad Zemek <konrad.zemek@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 */

#include "hw_deathadder_chroma.h"
#include "razer_private.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static enum razer_mouse_freq deathadder_chroma_freqs_list[] = {
    RAZER_MOUSE_FREQ_125HZ, RAZER_MOUSE_FREQ_500HZ, RAZER_MOUSE_FREQ_1000HZ};

static enum razer_mouse_res deathadder_chroma_resolution_stages_list[] = {
    RAZER_MOUSE_RES_800DPI, RAZER_MOUSE_RES_1800DPI, RAZER_MOUSE_RES_3500DPI,
    RAZER_MOUSE_RES_5600DPI, RAZER_MOUSE_RES_10000DPI};

#define DEATHADDER_CHROMA_DEVICE_NAME "DeathAdder Chroma"
#define DEATHADDER_CHROMA_SCROLL_NAME "Scrollwheel"
#define DEATHADDER_CHROMA_LOGO_NAME "GlowingLogo"

enum deathadder_chroma_led_id {
	DEATHADDER_CHROMA_LED_ID_SCROLL = 0x01,
	DEATHADDER_CHROMA_LED_ID_LOGO = 0x04
};

enum deathadder_chroma_led_mode {
	DEATHADDER_CHROMA_LED_MODE_STATIC = 0x00,
	DEATHADDER_CHROMA_LED_MODE_BREATHING = 0x02,
	DEATHADDER_CHROMA_LED_MODE_SPECTRUM = 0X04
};

enum deathadder_chroma_led_state {
	DEATHADDER_CHROMA_LED_STATE_OFF = 0x00,
	DEATHADDER_CHROMA_LED_STATE_ON = 0x01
};

/*
 * The 6th byte of DeathAdder Chroma's command seems to be the size of arguments
 * (size of arguments to read in case of read operations). It's not necessarily
 * so, since some values are slightly off (i.e. bigger than the apparent size
 * of the arguments).
 * Experiments suggest that the value given in the 'size' byte does not matter.
 * I chose to go with the values used by the Synapse driver.
 */
enum deathadder_chroma_request_size {
	DEATHADDER_CHROMA_REQUEST_SIZE_INIT = 0x02,
	DEATHADDER_CHROMA_REQUEST_SIZE_SET_RESOLUTION = 0x07,
	DEATHADDER_CHROMA_REQUEST_SIZE_GET_FIRMWARE = 0x04,
	DEATHADDER_CHROMA_REQUEST_SIZE_GET_SERIAL_NO = 0x16,
	DEATHADDER_CHROMA_REQUEST_SIZE_SET_FREQUENCY = 0x01,
	DEATHADDER_CHROMA_REQUEST_SIZE_SET_LED_STATE = 0x03,
	DEATHADDER_CHROMA_REQUEST_SIZE_SET_LED_MODE = 0x03,
	DEATHADDER_CHROMA_REQUEST_SIZE_SET_LED_COLOR = 0x05
};

enum deathadder_chroma_request {
	DEATHADDER_CHROMA_REQUEST_INIT = 0x0004,
	DEATHADDER_CHROMA_REQUEST_SET_RESOLUTION = 0x0405,
	DEATHADDER_CHROMA_REQUEST_GET_FIRMWARE = 0x0087,
	DEATHADDER_CHROMA_REQUEST_GET_SERIAL_NO = 0x0082,
	DEATHADDER_CHROMA_REQUEST_SET_FREQUENCY = 0x0005,
	DEATHADDER_CHROMA_REQUEST_SET_LED_STATE = 0x0300,
	DEATHADDER_CHROMA_REQUEST_SET_LED_MODE = 0x0302,
	DEATHADDER_CHROMA_REQUEST_SET_LED_COLOR = 0x0301
};

enum deathadder_chroma_constants {
	DEATHADDER_CHROMA_MAX_FREQUENCY = RAZER_MOUSE_FREQ_1000HZ,
	DEATHADDER_CHROMA_MAX_RESOLUTION = RAZER_MOUSE_RES_10000DPI,
	DEATHADDER_CHROMA_RESOLUTION_STEP = RAZER_MOUSE_RES_100DPI,

	DEATHADDER_CHROMA_LED_NUM = 2,
	DEATHADDER_CHROMA_AXES_NUM = 2,
	DEATHADDER_CHROMA_SUPPORTED_FREQ_NUM =
	    ARRAY_SIZE(deathadder_chroma_freqs_list),
	DEATHADDER_CHROMA_DPIMAPPINGS_NUM =
	    ARRAY_SIZE(deathadder_chroma_resolution_stages_list),

	DEATHADDER_CHROMA_USB_SETUP_PACKET_VALUE = 0x300,
	DEATHADDER_CHROMA_SUCCESS_STATUS = 0x02,
	DEATHADDER_CHROMA_PACKET_SPACING_MS = 35,

	/*
	 * Experiments suggest that the value in the 'magic' byte of the command
	 * does not necessarily matter (e.g. the commands work when the 'magic'
	 * byte equals 0x77). I chose to go with the value used by the Synapse
	 * driver.
	 */
	DEATHADDER_CHROMA_MAGIC_BYTE = 0xFF,

	/*
	 * These specific arg0 bytes are used by the Synapse driver for their
	 * respective commands. Their value may or may not matter (e.g. matters
	 * in the case of LED commands, doesn't seem to matter for the
	 * resolution command).
	 */
	DEATHADDER_CHROMA_LED_ARG0 = 0x01,
	DEATHADDER_CHROMA_INIT_ARG0 = 0x03,
	DEATHADDER_CHROMA_RESOLUTION_ARG0 = 0x00
};

struct deathadder_chroma_command
{
	uint8_t status;
	uint8_t magic;
	uint8_t padding0[3];
	uint8_t size;
	be16_t request;

	union
	{
		uint8_t bvalue[80];
		struct
		{
			uint8_t padding1;
			be16_t value[38];
			uint8_t padding2;
		} _packed;
	} _packed;

	uint8_t checksum;
	uint8_t padding3;
} _packed;

#define DEATHADDER_CHROMA_COMMAND_INIT                                         \
	(struct deathadder_chroma_command)                                     \
	{                                                                      \
		.magic = DEATHADDER_CHROMA_MAGIC_BYTE                          \
	}

struct deathadder_chroma_rgb_color
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct deathadder_chroma_led
{
	enum deathadder_chroma_led_id id;
	enum deathadder_chroma_led_mode mode;
	enum deathadder_chroma_led_state state;
	struct deathadder_chroma_rgb_color color;
};

struct deathadder_chroma_driver_data
{
	struct razer_event_spacing packet_spacing;
	struct razer_mouse_profile profile;
	struct razer_mouse_dpimapping *current_dpimapping;
	enum razer_mouse_freq current_freq;
	struct deathadder_chroma_led scroll_led;
	struct deathadder_chroma_led logo_led;
	struct razer_mouse_dpimapping
	    dpimappings[DEATHADDER_CHROMA_DPIMAPPINGS_NUM];
	struct razer_axis axes[DEATHADDER_CHROMA_AXES_NUM];
	uint16_t fw_version;
	char serial[DEATHADDER_CHROMA_REQUEST_SIZE_GET_SERIAL_NO + 1];
};

static uint8_t deathadder_chroma_checksum(struct deathadder_chroma_command *cmd)
{
	size_t control_size;

	control_size = sizeof(cmd->size) + sizeof(cmd->request);
	return razer_xor8_checksum((uint8_t *)&cmd->size,
				   control_size + cmd->size);
}

static int deathadder_chroma_translate_frequency(enum razer_mouse_freq freq)
{
	switch (freq) {
	case RAZER_MOUSE_FREQ_UNKNOWN:
		freq = RAZER_MOUSE_FREQ_500HZ;
		/* fall through */
	case RAZER_MOUSE_FREQ_125HZ:
	case RAZER_MOUSE_FREQ_500HZ:
	case RAZER_MOUSE_FREQ_1000HZ:
		return DEATHADDER_CHROMA_MAX_FREQUENCY / freq;

	default:
		return -EINVAL;
	}
}

static int deathadder_chroma_usb_action(
    struct razer_mouse *m, enum libusb_endpoint_direction direction,
    enum libusb_standard_request request, uint16_t command,
    struct deathadder_chroma_command *cmd)
{
	int err;
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;

	razer_event_spacing_enter(&drv_data->packet_spacing);
	err = libusb_control_transfer(m->usb_ctx->h,
				      direction | LIBUSB_REQUEST_TYPE_CLASS |
					  LIBUSB_RECIPIENT_INTERFACE,
				      request, command, 0, (unsigned char *)cmd,
				      sizeof(*cmd), RAZER_USB_TIMEOUT);
	razer_event_spacing_leave(&drv_data->packet_spacing);

	if (err != sizeof(*cmd)) {
		razer_error("razer-deathadder-chroma: "
			    "USB %s 0x%01X 0x%02X failed with %d\n",
			    direction == LIBUSB_ENDPOINT_IN ? "read" : "write",
			    request, command, err);
		return err;
	}

	return 0;
}

static int deathadder_chroma_send_command(struct razer_mouse *m,
					  struct deathadder_chroma_command *cmd)
{
	int err;
	uint8_t checksum;

	cmd->checksum = deathadder_chroma_checksum(cmd);
	err = deathadder_chroma_usb_action(
	    m, LIBUSB_ENDPOINT_OUT, LIBUSB_REQUEST_SET_CONFIGURATION,
	    DEATHADDER_CHROMA_USB_SETUP_PACKET_VALUE, cmd);
	if (err)
		return err;

	err = deathadder_chroma_usb_action(
	    m, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_CLEAR_FEATURE,
	    DEATHADDER_CHROMA_USB_SETUP_PACKET_VALUE, cmd);
	if (err)
		return err;

	checksum = deathadder_chroma_checksum(cmd);
	if (checksum != cmd->checksum) {
		razer_error("razer-deathadder-chroma: "
			    "Command %02X %04X bad response checksum %02X "
			    "(expected %02X)\n",
			    cmd->size, be16_to_cpu(cmd->request), checksum,
			    cmd->checksum);

		return -EBADMSG;
	}

	if (cmd->status != DEATHADDER_CHROMA_SUCCESS_STATUS)
		razer_error("razer-deathadder-chroma: "
			    "Command %02X %04X failed with %02X\n",
			    cmd->size, be16_to_cpu(cmd->request), cmd->status);

	return 0;
}

static int deathadder_chroma_send_init_command(struct razer_mouse *m)
{
	struct deathadder_chroma_command cmd;

	cmd = DEATHADDER_CHROMA_COMMAND_INIT;
	cmd.size = DEATHADDER_CHROMA_REQUEST_SIZE_INIT;
	cmd.request = cpu_to_be16(DEATHADDER_CHROMA_REQUEST_INIT);
	cmd.bvalue[0] = DEATHADDER_CHROMA_INIT_ARG0;
	return deathadder_chroma_send_command(m, &cmd);
}

static int deathadder_chroma_send_set_resolution_command(struct razer_mouse *m)
{
	enum razer_mouse_res res_x, res_y;
	struct deathadder_chroma_command cmd;
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	res_x = drv_data->current_dpimapping->res[RAZER_DIM_X];
	res_y = drv_data->current_dpimapping->res[RAZER_DIM_Y];

	cmd = DEATHADDER_CHROMA_COMMAND_INIT;
	cmd.size = DEATHADDER_CHROMA_REQUEST_SIZE_SET_RESOLUTION;
	cmd.request = cpu_to_be16(DEATHADDER_CHROMA_REQUEST_SET_RESOLUTION);
	cmd.bvalue[0] = DEATHADDER_CHROMA_RESOLUTION_ARG0;
	cmd.value[0] = cpu_to_be16(res_x);
	cmd.value[1] = cpu_to_be16(res_y);
	return deathadder_chroma_send_command(m, &cmd);
}

static int deathadder_chroma_send_get_firmware_command(struct razer_mouse *m)
{
	int err;
	uint8_t fw_major;
	uint16_t fw_minor;
	struct deathadder_chroma_command cmd;
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;

	cmd = DEATHADDER_CHROMA_COMMAND_INIT;
	cmd.size = DEATHADDER_CHROMA_REQUEST_SIZE_GET_FIRMWARE;
	cmd.request = cpu_to_be16(DEATHADDER_CHROMA_REQUEST_GET_FIRMWARE);

	err = deathadder_chroma_send_command(m, &cmd);
	if (err)
		return err;

	fw_major = cmd.bvalue[0];
	fw_minor = be16_to_cpu(cmd.value[0]);
	drv_data->fw_version = (fw_major << 8) | fw_minor;

	return 0;
}

static int deathadder_chroma_send_get_serial_no_command(struct razer_mouse *m)
{
	int err;
	struct deathadder_chroma_command cmd;
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;

	cmd = DEATHADDER_CHROMA_COMMAND_INIT;
	cmd.size = DEATHADDER_CHROMA_REQUEST_SIZE_GET_SERIAL_NO;
	cmd.request = cpu_to_be16(DEATHADDER_CHROMA_REQUEST_GET_SERIAL_NO);

	err = deathadder_chroma_send_command(m, &cmd);
	if (err)
		return err;

	strncpy(drv_data->serial, (const char *)cmd.bvalue,
		DEATHADDER_CHROMA_REQUEST_SIZE_GET_SERIAL_NO);
	drv_data->serial[DEATHADDER_CHROMA_REQUEST_SIZE_GET_SERIAL_NO] = '\0';

	return 0;
}

static int deathadder_chroma_send_set_frequency_command(struct razer_mouse *m)
{
	int tfreq;
	struct deathadder_chroma_command cmd;
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	cmd = DEATHADDER_CHROMA_COMMAND_INIT;

	tfreq = deathadder_chroma_translate_frequency(drv_data->current_freq);
	if (tfreq < 0)
		return tfreq;

	cmd.size = DEATHADDER_CHROMA_REQUEST_SIZE_SET_FREQUENCY;
	cmd.request = cpu_to_be16(DEATHADDER_CHROMA_REQUEST_SET_FREQUENCY);
	cmd.bvalue[0] = tfreq;
	return deathadder_chroma_send_command(m, &cmd);
}

static struct deathadder_chroma_led *
deathadder_chroma_get_led(struct deathadder_chroma_driver_data *d,
			  enum deathadder_chroma_led_id led_id)
{
	switch (led_id) {
	case DEATHADDER_CHROMA_LED_ID_LOGO:
		return &d->logo_led;
	case DEATHADDER_CHROMA_LED_ID_SCROLL:
		return &d->scroll_led;
	default:
		return NULL;
	}
}

static int
deathadder_chroma_send_set_led_state_command(struct razer_mouse *m,
					     struct deathadder_chroma_led *led)
{
	struct deathadder_chroma_command cmd;

	cmd = DEATHADDER_CHROMA_COMMAND_INIT;
	cmd.size = DEATHADDER_CHROMA_REQUEST_SIZE_SET_LED_STATE;
	cmd.request = cpu_to_be16(DEATHADDER_CHROMA_REQUEST_SET_LED_STATE);
	cmd.bvalue[0] = DEATHADDER_CHROMA_LED_ARG0;
	cmd.bvalue[1] = led->id;
	cmd.bvalue[2] = led->state;
	return deathadder_chroma_send_command(m, &cmd);
}

static int
deathadder_chroma_send_set_led_mode_command(struct razer_mouse *m,
					    struct deathadder_chroma_led *led)
{
	struct deathadder_chroma_command cmd;

	cmd = DEATHADDER_CHROMA_COMMAND_INIT;
	cmd.size = DEATHADDER_CHROMA_REQUEST_SIZE_SET_LED_MODE;
	cmd.request = cpu_to_be16(DEATHADDER_CHROMA_REQUEST_SET_LED_MODE);
	cmd.bvalue[0] = DEATHADDER_CHROMA_LED_ARG0;
	cmd.bvalue[1] = led->id;
	cmd.bvalue[2] = led->mode;
	return deathadder_chroma_send_command(m, &cmd);
}

static int
deathadder_chroma_send_set_led_color_command(struct razer_mouse *m,
					     struct deathadder_chroma_led *led)
{
	struct deathadder_chroma_command cmd;

	cmd = DEATHADDER_CHROMA_COMMAND_INIT;
	cmd.size = DEATHADDER_CHROMA_REQUEST_SIZE_SET_LED_COLOR;
	cmd.request = cpu_to_be16(DEATHADDER_CHROMA_REQUEST_SET_LED_COLOR);
	cmd.bvalue[0] = DEATHADDER_CHROMA_LED_ARG0;
	cmd.bvalue[1] = led->id;
	cmd.bvalue[2] = led->color.r;
	cmd.bvalue[3] = led->color.g;
	cmd.bvalue[4] = led->color.b;
	return deathadder_chroma_send_command(m, &cmd);
}

static int deathadder_chroma_get_fw_version(struct razer_mouse *m)
{
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	return drv_data->fw_version;
}

static struct razer_mouse_profile *
deathadder_chroma_get_profiles(struct razer_mouse *m)
{
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	return &drv_data->profile;
}

static int deathadder_chroma_supported_axes(struct razer_mouse *m,
					    struct razer_axis **res_ptr)
{
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	*res_ptr = drv_data->axes;
	return ARRAY_SIZE(drv_data->axes);
}

static int
deathadder_chroma_supported_dpimappings(struct razer_mouse *m,
					struct razer_mouse_dpimapping **res_ptr)
{
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	*res_ptr = drv_data->dpimappings;
	return ARRAY_SIZE(drv_data->dpimappings);
}

static int
deathadder_chroma_supported_resolutions(struct razer_mouse *m,
					enum razer_mouse_res **res_ptr)
{
	size_t i;
	size_t step_number;

	step_number = DEATHADDER_CHROMA_MAX_RESOLUTION /
		      DEATHADDER_CHROMA_RESOLUTION_STEP;

	*res_ptr = calloc(step_number, sizeof(enum razer_mouse_res));
	if (!*res_ptr)
		return -ENOMEM;

	for (i = 0; i < step_number; ++i)
		(*res_ptr)[i] = (i + 1) * DEATHADDER_CHROMA_RESOLUTION_STEP;

	return step_number;
}

static int deathadder_chroma_supported_freqs(struct razer_mouse *m,
					     enum razer_mouse_freq **res_ptr)
{
	*res_ptr = malloc(sizeof(deathadder_chroma_freqs_list));
	if (!*res_ptr)
		return -ENOMEM;

	memcpy(*res_ptr, deathadder_chroma_freqs_list,
	       sizeof(deathadder_chroma_freqs_list));
	return DEATHADDER_CHROMA_SUPPORTED_FREQ_NUM;
}

static enum razer_mouse_freq
deathadder_chroma_get_freq(struct razer_mouse_profile *p)
{
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = p->mouse->drv_data;
	return drv_data->current_freq;
}

static struct razer_mouse_dpimapping *
deathadder_chroma_get_dpimapping(struct razer_mouse_profile *p,
				 struct razer_axis *axis)
{
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = p->mouse->drv_data;
	return drv_data->current_dpimapping;
}

static int deathadder_chroma_change_dpimapping(struct razer_mouse_dpimapping *d,
					       enum razer_dimension dim,
					       enum razer_mouse_res res)
{
	struct deathadder_chroma_driver_data *drv_data;

	if (!(d->dimension_mask & (1 << dim)))
		return -EINVAL;

	if (res == RAZER_MOUSE_RES_UNKNOWN)
		res = RAZER_MOUSE_RES_1800DPI;

	if (res < RAZER_MOUSE_RES_100DPI || res > RAZER_MOUSE_RES_10000DPI)
		return -EINVAL;

	d->res[dim] = res;

	drv_data = d->mouse->drv_data;
	if (d == drv_data->current_dpimapping)
		return deathadder_chroma_send_set_resolution_command(d->mouse);

	return 0;
}

static int deathadder_chroma_led_toggle_state(struct razer_led *led,
					      enum razer_led_state new_state)
{
	struct deathadder_chroma_driver_data *drv_data;
	struct deathadder_chroma_led *priv_led;

	drv_data = led->u.mouse->drv_data;
	priv_led = deathadder_chroma_get_led(drv_data, led->id);

	if (!priv_led)
		return -EINVAL;

	switch (new_state) {
	case RAZER_LED_UNKNOWN:
	case RAZER_LED_ON:
		priv_led->state = DEATHADDER_CHROMA_LED_STATE_ON;
		break;
	case RAZER_LED_OFF:
		priv_led->state = DEATHADDER_CHROMA_LED_STATE_OFF;
		break;
	}

	return deathadder_chroma_send_set_led_state_command(led->u.mouse,
							    priv_led);
}

static int
deathadder_chroma_led_change_color(struct razer_led *led,
				   const struct razer_rgb_color *new_color)
{
	struct deathadder_chroma_driver_data *drv_data;
	struct deathadder_chroma_led *priv_led;

	drv_data = led->u.mouse->drv_data;
	priv_led = deathadder_chroma_get_led(drv_data, led->id);

	if (!priv_led)
		return -EINVAL;

	if (priv_led->mode == DEATHADDER_CHROMA_LED_MODE_SPECTRUM)
		return -EINVAL;

	priv_led->color = (struct deathadder_chroma_rgb_color){
	    .r = new_color->r, .g = new_color->g, .b = new_color->b};

	return deathadder_chroma_send_set_led_color_command(led->u.mouse,
							    priv_led);
}

static int deathadder_chroma_set_freq(struct razer_mouse_profile *p,
				      enum razer_mouse_freq freq)
{
	struct deathadder_chroma_driver_data *drv_data;

	if (freq == RAZER_MOUSE_FREQ_UNKNOWN)
		freq = RAZER_MOUSE_FREQ_500HZ;

	if (freq != RAZER_MOUSE_FREQ_125HZ && freq != RAZER_MOUSE_FREQ_500HZ &&
	    freq != RAZER_MOUSE_FREQ_1000HZ)
		return -EINVAL;

	drv_data = p->mouse->drv_data;
	drv_data->current_freq = freq;

	return deathadder_chroma_send_set_frequency_command(p->mouse);
}

static int deathadder_chroma_set_dpimapping(struct razer_mouse_profile *p,
					    struct razer_axis *axis,
					    struct razer_mouse_dpimapping *d)
{
	struct deathadder_chroma_driver_data *drv_data;

	if (axis && axis->id > 0)
		return -EINVAL;

	drv_data = p->mouse->drv_data;
	drv_data->current_dpimapping = &drv_data->dpimappings[d->nr];

	return deathadder_chroma_send_set_resolution_command(p->mouse);
}

static int
deathadder_chroma_translate_led_mode(enum deathadder_chroma_led_mode mode)
{
	switch (mode) {
	case DEATHADDER_CHROMA_LED_MODE_STATIC:
		return RAZER_LED_MODE_STATIC;
	case DEATHADDER_CHROMA_LED_MODE_BREATHING:
		return RAZER_LED_MODE_BREATHING;
	case DEATHADDER_CHROMA_LED_MODE_SPECTRUM:
		return RAZER_LED_MODE_SPECTRUM;
	default:
		return -EINVAL;
	}
}

static int deathadder_chroma_translate_razer_led_mode(enum razer_led_mode mode)
{
	switch (mode) {
	case RAZER_LED_MODE_STATIC:
		return DEATHADDER_CHROMA_LED_MODE_STATIC;
	case RAZER_LED_MODE_BREATHING:
		return DEATHADDER_CHROMA_LED_MODE_BREATHING;
	case RAZER_LED_MODE_SPECTRUM:
		return DEATHADDER_CHROMA_LED_MODE_SPECTRUM;
	default:
		return -EINVAL;
	}
}

static int deathadder_chroma_led_set_mode(struct razer_led *led,
					  enum razer_led_mode new_mode)
{
	int err;
	struct deathadder_chroma_driver_data *drv_data;
	struct deathadder_chroma_led *priv_led;

	drv_data = led->u.mouse->drv_data;
	priv_led = deathadder_chroma_get_led(drv_data, led->id);

	if (!priv_led)
		return -EINVAL;

	err = deathadder_chroma_translate_razer_led_mode(new_mode);
	if (err < 0)
		return err;

	priv_led->mode = err;
	return deathadder_chroma_send_set_led_mode_command(led->u.mouse,
							   priv_led);
}

static int deathadder_chroma_get_leds(struct razer_mouse *m,
				      struct razer_led **leds_list)
{
	unsigned int supported_modes;
	enum razer_led_state scroll_state, logo_state;
	struct deathadder_chroma_driver_data *drv_data;
	struct razer_led *scroll, *logo;

	drv_data = m->drv_data;

	scroll = zalloc(sizeof(struct razer_led));
	if (!scroll)
		return -ENOMEM;

	logo = zalloc(sizeof(struct razer_led));
	if (!logo) {
		free(scroll);
		return -ENOMEM;
	}

	supported_modes = (1 << RAZER_LED_MODE_BREATHING) |
			  (1 << RAZER_LED_MODE_SPECTRUM) |
			  (1 << RAZER_LED_MODE_STATIC);

	scroll_state =
	    drv_data->scroll_led.state == DEATHADDER_CHROMA_LED_STATE_OFF
		? RAZER_LED_OFF
		: RAZER_LED_ON;

	*scroll = (struct razer_led){
	    .id = drv_data->scroll_led.id,
	    .name = DEATHADDER_CHROMA_SCROLL_NAME,
	    .state = scroll_state,
	    .u.mouse = m,
	    .toggle_state = deathadder_chroma_led_toggle_state,
	    .change_color = deathadder_chroma_led_change_color,
	    .set_mode = deathadder_chroma_led_set_mode,
	    .next = logo,
	    .color = {.r = drv_data->scroll_led.color.r,
		      .g = drv_data->scroll_led.color.g,
		      .b = drv_data->scroll_led.color.b,
		      .valid = 1},
	    .supported_modes_mask = supported_modes,
	    .mode = deathadder_chroma_translate_led_mode(
		drv_data->scroll_led.mode)};

	logo_state = drv_data->logo_led.state == DEATHADDER_CHROMA_LED_STATE_OFF
			 ? RAZER_LED_OFF
			 : RAZER_LED_ON;

	*logo = (struct razer_led){
	    .id = drv_data->logo_led.id,
	    .name = DEATHADDER_CHROMA_LOGO_NAME,
	    .state = logo_state,
	    .u.mouse = m,
	    .toggle_state = deathadder_chroma_led_toggle_state,
	    .change_color = deathadder_chroma_led_change_color,
	    .set_mode = deathadder_chroma_led_set_mode,
	    .color = {.r = drv_data->logo_led.color.r,
		      .g = drv_data->logo_led.color.g,
		      .b = drv_data->logo_led.color.b,
		      .valid = 1},
	    .supported_modes_mask = supported_modes,
	    .mode =
		deathadder_chroma_translate_led_mode(drv_data->logo_led.mode)};

	*leds_list = scroll;
	return DEATHADDER_CHROMA_LED_NUM;
}

int razer_deathadder_chroma_init(struct razer_mouse *m,
				 struct libusb_device *usbdev)
{
	int err;
	size_t i;
	struct deathadder_chroma_driver_data *drv_data;
	struct deathadder_chroma_led *scroll_led, *logo_led;

	BUILD_BUG_ON(sizeof(struct deathadder_chroma_command) != 90);

	drv_data = zalloc(sizeof(*drv_data));
	if (!drv_data)
		return -ENOMEM;

	razer_event_spacing_init(&drv_data->packet_spacing,
				 DEATHADDER_CHROMA_PACKET_SPACING_MS);

	for (i = 0; i < DEATHADDER_CHROMA_DPIMAPPINGS_NUM; ++i) {
		drv_data->dpimappings[i] = (struct razer_mouse_dpimapping){
		    .nr = i,
		    .change = deathadder_chroma_change_dpimapping,
		    .dimension_mask = (1 << RAZER_DIM_X) | (1 << RAZER_DIM_Y),
		    .mouse = m};

		drv_data->dpimappings[i].res[RAZER_DIM_X] =
		    drv_data->dpimappings[i].res[RAZER_DIM_Y] =
			deathadder_chroma_resolution_stages_list[i];
	}

	drv_data->current_dpimapping = &drv_data->dpimappings[1];
	drv_data->current_freq = RAZER_MOUSE_FREQ_500HZ;

	drv_data->scroll_led = (struct deathadder_chroma_led){
	    .id = DEATHADDER_CHROMA_LED_ID_SCROLL,
	    .mode = DEATHADDER_CHROMA_LED_MODE_SPECTRUM,
	    .state = DEATHADDER_CHROMA_LED_STATE_ON,
	    .color = {0x00, 0xFF, 0x00}};

	drv_data->logo_led = (struct deathadder_chroma_led){
	    .id = DEATHADDER_CHROMA_LED_ID_LOGO,
	    .mode = DEATHADDER_CHROMA_LED_MODE_SPECTRUM,
	    .state = DEATHADDER_CHROMA_LED_STATE_ON,
	    .color = {0x00, 0xFF, 0x00}};

	razer_init_axes(drv_data->axes, "X/Y",
			RAZER_AXIS_INDEPENDENT_DPIMAPPING, "Scroll", 0, NULL,
			0);

	m->drv_data = drv_data;

	if ((err = razer_usb_add_used_interface(m->usb_ctx, 0, 0)) ||
	    (err = m->claim(m))) {
		free(drv_data);
		return err;
	}

	scroll_led = &drv_data->scroll_led;
	logo_led = &drv_data->logo_led;

	if ((err = deathadder_chroma_send_init_command(m)) ||
	    (err = deathadder_chroma_send_set_resolution_command(m)) ||
	    (err = deathadder_chroma_send_get_firmware_command(m)) ||
	    (err = deathadder_chroma_send_get_serial_no_command(m)) ||
	    (err = deathadder_chroma_send_set_frequency_command(m)) ||
	    (err =
		 deathadder_chroma_send_set_led_state_command(m, scroll_led)) ||
	    (err =
		 deathadder_chroma_send_set_led_mode_command(m, scroll_led)) ||
	    (err =
		 deathadder_chroma_send_set_led_color_command(m, scroll_led)) ||
	    (err = deathadder_chroma_send_set_led_state_command(m, logo_led)) ||
	    (err = deathadder_chroma_send_set_led_mode_command(m, logo_led)) ||
	    (err = deathadder_chroma_send_set_led_color_command(m, logo_led))) {
		m->release(m);
		free(drv_data);
		return err;
	}

	m->release(m);

	drv_data->profile = (struct razer_mouse_profile){
	    .mouse = m,
	    .get_freq = deathadder_chroma_get_freq,
	    .set_freq = deathadder_chroma_set_freq,
	    .get_dpimapping = deathadder_chroma_get_dpimapping,
	    .set_dpimapping = deathadder_chroma_set_dpimapping};

	razer_generic_usb_gen_idstr(usbdev, m->usb_ctx->h,
				    DEATHADDER_CHROMA_DEVICE_NAME, false,
				    drv_data->serial, m->idstr);

	m->type = RAZER_MOUSETYPE_DEATHADDER;
	m->get_fw_version = deathadder_chroma_get_fw_version;
	m->global_get_leds = deathadder_chroma_get_leds;
	m->get_profiles = deathadder_chroma_get_profiles;
	m->supported_axes = deathadder_chroma_supported_axes;
	m->supported_resolutions = deathadder_chroma_supported_resolutions;
	m->supported_freqs = deathadder_chroma_supported_freqs;
	m->supported_dpimappings = deathadder_chroma_supported_dpimappings;

	return 0;
}

void razer_deathadder_chroma_release(struct razer_mouse *m)
{
	struct deathadder_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	free(drv_data);
	m->drv_data = NULL;
}
