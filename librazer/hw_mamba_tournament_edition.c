/*
 * Lowlevel hardware access for the Razer Mamba Tournament Edition mouse.
 *
 * Important notice:
 * This hardware driver is based on reverse engineering, only.
 *
 * Copyright (C) 2015 Konrad Zemek <konrad.zemek@gmail.com>
 * Copyright (C) 2016 WANG Haoan <wanghaoan.victor@gmail.com>
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

#include "hw_mamba_tournament_edition.h"
#include "razer_private.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static enum razer_mouse_freq mamba_te_freqs_list[] =
{
	RAZER_MOUSE_FREQ_125HZ,
	RAZER_MOUSE_FREQ_500HZ,
	RAZER_MOUSE_FREQ_1000HZ
};

static enum razer_mouse_res mamba_te_resolution_stages_list[] =
{
	RAZER_MOUSE_RES_800DPI,
	RAZER_MOUSE_RES_1800DPI,
	RAZER_MOUSE_RES_3500DPI,
	RAZER_MOUSE_RES_5600DPI,
	RAZER_MOUSE_RES_10000DPI
};

#define MAMBA_TE_DEVICE_NAME	"Mamba Tournament Edition"
#define MAMBA_TE_LED_NAME	"Basic"


enum mamba_te_led_mode
{
	MAMBA_TE_LED_MODE_STATIC		= 0x06,
	MAMBA_TE_LED_MODE_BREATHING		= 0x0301, // 0x0302 is breathing with 2 different colors
	MAMBA_TE_LED_MODE_SPECTRUM		= 0x0400,
	MAMBA_TE_LED_MODE_WAVE			= 0x0101, // 0x0102 is waving in a opposite direction
	MAMBA_TE_LED_MODE_REACTION		= 0x0203,
	MAMBA_TE_LED_MODE_CUSTOMIZED		= 0x0500,
	MAMBA_TE_LED_MODE_CUSTOMIZED_COLOR_INFO	= 0x050c, // for further implementation
};

enum mamba_te_led_state
{
	MAMBA_TE_LED_STATE_OFF			= 0x00,
	MAMBA_TE_LED_STATE_ON			= 0xFF,
};

//for further implementation
enum mamba_te_led_state_option
{
	MAMBA_TE_LED_STATE_OPTION_0		= 0x00,
	MAMBA_TE_LED_STATE_OPTION_1		= 0x01,
	MAMBA_TE_LED_STATE_OPTION_2		= 0x02,
	MAMBA_TE_LED_STATE_OPTION_3		= 0x03,
};

/*
 * The 6th byte of MAMBA TE's command seems also to be the size of arguments
 * (size of arguments to read in case of read operations). It's not necessarily
 * so, since some values are slightly off (i.e. bigger than the apparent size
 * of the arguments). But when it's in customized mode, there will be 0x32 which is 50 bytes of information
 * Experiments suggest that the value given in the 'size' byte does not matter.
 * I chose to go with the values used by the Synapse driver.
 */
enum mamba_te_request_size
{
	MAMBA_TE_REQUEST_SIZE_INIT		= 0x02,
	MAMBA_TE_REQUEST_SIZE_SET_RESOLUTION	= 0x07,
	MAMBA_TE_REQUEST_SIZE_GET_FIRMWARE	= 0x04,
	MAMBA_TE_REQUEST_SIZE_GET_SERIAL_NO	= 0x16,
	MAMBA_TE_REQUEST_SIZE_SET_FREQUENCY	= 0x01,
	MAMBA_TE_REQUEST_SIZE_SET_LED_STATE	= 0x08,
	MAMBA_TE_REQUEST_SIZE_SET_LED_MODE	= 0x08,
	MAMBA_TE_REQUEST_SIZE_SET_LED_COLOR	= 0x08,
};

enum mamba_te_request
{
	MAMBA_TE_REQUEST_INIT			= 0x0004,
	MAMBA_TE_REQUEST_SET_RESOLUTION		= 0x0405,
	MAMBA_TE_REQUEST_GET_FIRMWARE		= 0x0087,
	MAMBA_TE_REQUEST_GET_SERIAL_NO		= 0x0082,
	MAMBA_TE_REQUEST_SET_FREQUENCY		= 0x0005,
	MAMBA_TE_REQUEST_SET_LED_STATE		= 0x030a,
	MAMBA_TE_REQUEST_SET_LED_COLOR		= 0x030a,
	MAMBA_TE_REQUEST_SET_LED_MODE		= 0x030a,
	MAMBA_TE_REQUEST_SET_LED_MODE_CUSTOMIZED = 0x030c, // for further implementation
};

enum mamba_te_constants
{
	MAMBA_TE_MAX_FREQUENCY			= RAZER_MOUSE_FREQ_1000HZ,
	MAMBA_TE_MAX_RESOLUTION			= RAZER_MOUSE_RES_10000DPI,
	MAMBA_TE_RESOLUTION_STEP		= RAZER_MOUSE_RES_100DPI,

	MAMBA_TE_LED_NUM			= 1,
	MAMBA_TE_AXES_NUM			= 2,
	MAMBA_TE_SUPPORTED_FREQ_NUM		= ARRAY_SIZE(mamba_te_freqs_list),
	MAMBA_TE_DPIMAPPINGS_NUM		= ARRAY_SIZE(mamba_te_resolution_stages_list),

	MAMBA_TE_USB_SETUP_PACKET_VALUE		= 0x300,
	MAMBA_TE_SUCCESS_STATUS			= 0x02,
	MAMBA_TE_PACKET_SPACING_MS		= 35,

	/*
	 * Experiments suggest that the value in the 'magic' byte of the command
	 * does not necessarily matter (e.g. the commands work when the 'magic'
	 * byte equals 0x77). I chose to go with the value used by the Synapse
	 * driver.
	 */
	MAMBA_TE_MAGIC_BYTE			= 0xFF,

	/*
	 * These specific arg0 bytes are used by the Synapse driver for their
	 * respective commands. Their value may or may not matter (e.g. matters
	 * in the case of LED commands, doesn't seem to matter for the
	 * resolution command).
	 */
	MAMBA_TE_LED_ARG0			= 0x02,
	MAMBA_TE_INIT_ARG0			= 0x03,
	MAMBA_TE_RESOLUTION_ARG0		= 0x01,
};

struct mamba_te_command
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
		}_packed;
	}_packed;

	uint8_t checksum;
	uint8_t padding3;
}_packed;

#define MAMBA_TE_COMMAND_INIT			\
	(struct mamba_te_command)		\
	{					\
		.magic = MAMBA_TE_MAGIC_BYTE	\
	}

struct mamba_te_rgb_color
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct mamba_te_led
{
	enum mamba_te_led_mode mode;
	enum mamba_te_led_state state;
	enum mamba_te_led_state_option option;
	struct mamba_te_rgb_color color;
};

struct mamba_te_driver_data
{
	struct razer_event_spacing packet_spacing;
	struct razer_mouse_profile profile;
	struct razer_mouse_dpimapping *current_dpimapping;
	enum razer_mouse_freq current_freq;
	struct mamba_te_led led;
	struct razer_mouse_dpimapping dpimappings[MAMBA_TE_DPIMAPPINGS_NUM];
	struct razer_axis axes[MAMBA_TE_AXES_NUM];
	uint16_t fw_version;
	char serial[MAMBA_TE_REQUEST_SIZE_GET_SERIAL_NO];
};

static uint8_t mamba_te_checksum(struct mamba_te_command *cmd)
{
	size_t control_size;

	control_size = sizeof(cmd->size) + sizeof(cmd->request);
	return razer_xor8_checksum((uint8_t *)&cmd->size, control_size + cmd->size);
}

static int mamba_te_translate_frequency(enum razer_mouse_freq freq)
{
	switch (freq) {
	case RAZER_MOUSE_FREQ_UNKNOWN:
		freq = RAZER_MOUSE_FREQ_500HZ;
		/* fallthrough */
	case RAZER_MOUSE_FREQ_125HZ:
	case RAZER_MOUSE_FREQ_500HZ:
	case RAZER_MOUSE_FREQ_1000HZ:
		return MAMBA_TE_MAX_FREQUENCY / freq;
	default:
		return -EINVAL;
	}
}

static int mamba_te_usb_action(struct razer_mouse *m,
			       enum libusb_endpoint_direction direction,
			       enum libusb_standard_request request,
			       uint16_t command,
			       struct mamba_te_command *cmd)
{
	int err;
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;

	razer_event_spacing_enter(&drv_data->packet_spacing);
	err = libusb_control_transfer(m->usb_ctx->h,
				      direction |
				      LIBUSB_REQUEST_TYPE_CLASS |
				      LIBUSB_RECIPIENT_INTERFACE,
				      request, command, 0,
				      (unsigned char *)cmd, sizeof(*cmd),
				      RAZER_USB_TIMEOUT);
	razer_event_spacing_leave(&drv_data->packet_spacing);
	if (err != sizeof(*cmd)) {
		razer_error("razer-mamba-tournament-edition: "
			    "USB %s 0x%01X 0x%02X failed with %d\n",
			    direction == LIBUSB_ENDPOINT_IN ? "read" : "write",
			    request, command, err);
		return err;
	}

	return 0;
}

static int mamba_te_send_command(struct razer_mouse *m,
				 struct mamba_te_command *cmd)
{
	int err;
	uint8_t checksum;

	cmd->checksum = mamba_te_checksum(cmd);
	err = mamba_te_usb_action(m, LIBUSB_ENDPOINT_OUT,
				  LIBUSB_REQUEST_SET_CONFIGURATION,
				  MAMBA_TE_USB_SETUP_PACKET_VALUE, cmd);
	if (err)
		return err;
	err = mamba_te_usb_action(m, LIBUSB_ENDPOINT_IN,
				  LIBUSB_REQUEST_CLEAR_FEATURE,
				  MAMBA_TE_USB_SETUP_PACKET_VALUE, cmd);
	if (err)
		return err;

	checksum = mamba_te_checksum (cmd);
	if (checksum != cmd->checksum) {
		razer_error("razer-mamba-tournament-edition: "
			    "Command %02X %04X bad response checksum %02X "
			    "(expected %02X)\n",
			    cmd->size, be16_to_cpu(cmd->request),
			    checksum, cmd->checksum);
		return -EBADMSG;
	}

	if (cmd->status != MAMBA_TE_SUCCESS_STATUS) {
		razer_error("razer-mamba-tournament-edition: "
			    "Command %02X %04X failed with %02X\n",
			    cmd->size, be16_to_cpu(cmd->request), cmd->status);
	}

	return 0;
}

static int mamba_te_send_init_command(struct razer_mouse *m)
{
	struct mamba_te_command cmd;

	cmd = MAMBA_TE_COMMAND_INIT;
	cmd.size = MAMBA_TE_REQUEST_SIZE_INIT;
	cmd.request = cpu_to_be16(MAMBA_TE_REQUEST_INIT);
	cmd.bvalue[0] = MAMBA_TE_INIT_ARG0;

	return mamba_te_send_command(m, &cmd);
}

static int mamba_te_send_set_resolution_command(struct razer_mouse *m)
{
	enum razer_mouse_res res_x, res_y;
	struct mamba_te_command cmd;
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;
	res_x = drv_data->current_dpimapping->res[RAZER_DIM_X];
	res_y = drv_data->current_dpimapping->res[RAZER_DIM_Y];

	cmd = MAMBA_TE_COMMAND_INIT;
	cmd.size = MAMBA_TE_REQUEST_SIZE_SET_RESOLUTION;
	cmd.request = cpu_to_be16(MAMBA_TE_REQUEST_SET_RESOLUTION);
	cmd.bvalue[0] = MAMBA_TE_RESOLUTION_ARG0;
	cmd.value[0] = cpu_to_be16(res_x);
	cmd.value[1] = cpu_to_be16(res_y);

	return mamba_te_send_command(m, &cmd);
}

static int mamba_te_send_get_firmware_command(struct razer_mouse *m)
{
	int err;
	uint8_t fw_major;
	uint16_t fw_minor;
	struct mamba_te_command cmd;
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;

	cmd = MAMBA_TE_COMMAND_INIT;
	cmd.size = MAMBA_TE_REQUEST_SIZE_GET_FIRMWARE;
	cmd.request = cpu_to_be16(MAMBA_TE_REQUEST_GET_FIRMWARE);

	err = mamba_te_send_command(m, &cmd);
	if (err)
		return err;

	fw_major = cmd.bvalue[0];
	fw_minor = be16_to_cpu(cmd.value[0]);
	drv_data->fw_version = (fw_major << 8) | fw_minor;

	return 0;
}

static int mamba_te_send_get_serial_no_command(struct razer_mouse *m)
{
	int err;
	struct mamba_te_command cmd;
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;

	cmd = MAMBA_TE_COMMAND_INIT;
	cmd.size = MAMBA_TE_REQUEST_SIZE_GET_SERIAL_NO;
	cmd.request = cpu_to_be16(MAMBA_TE_REQUEST_GET_SERIAL_NO);

	err = mamba_te_send_command(m, &cmd);
	if (err)
		return err;

	strncpy(drv_data->serial, (const char *)cmd.bvalue,
		MAMBA_TE_REQUEST_SIZE_GET_SERIAL_NO);

	return 0;
}

static int mamba_te_send_set_frequency_command(struct razer_mouse *m)
{
	int tfreq;
	struct mamba_te_command cmd;
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;
	cmd = MAMBA_TE_COMMAND_INIT;

	tfreq = mamba_te_translate_frequency(drv_data->current_freq);
	if (tfreq < 0)
		return tfreq;

	cmd.size = MAMBA_TE_REQUEST_SIZE_SET_FREQUENCY;
	cmd.request = cpu_to_be16(MAMBA_TE_REQUEST_SET_FREQUENCY);
	cmd.bvalue[0] = tfreq;

	return mamba_te_send_command(m, &cmd);
}

static struct mamba_te_led *mamba_te_get_led(struct mamba_te_driver_data *d)
{
	return &d->led;
}

static int mamba_te_send_set_led_state_command(struct razer_mouse *m,
					       struct mamba_te_led *led)
{
	struct mamba_te_command cmd;

	cmd = MAMBA_TE_COMMAND_INIT;
	cmd.size = MAMBA_TE_REQUEST_SIZE_SET_LED_STATE;
	cmd.request = cpu_to_be16(MAMBA_TE_REQUEST_SET_LED_STATE);

	if (led->mode == MAMBA_TE_LED_MODE_STATIC) {
		cmd.bvalue[0] = led->mode;
		cmd.bvalue[1] = led->color.r;
		cmd.bvalue[2] = led->color.g;
		cmd.bvalue[3] = led->color.b;
	} else {
		uint16_t tempHex;
		uint8_t tempH, tempL;

		tempHex = led->mode;
		tempH = (uint8_t)((tempHex & 0xFF00) >> 8);
		tempL = (uint8_t)(tempHex & 0x00FF);
		cmd.bvalue[0] = tempH;
		cmd.bvalue[1] = tempL;
		cmd.bvalue[2] = led->color.r;
		cmd.bvalue[3] = led->color.g;
		cmd.bvalue[4] = led->color.b;
	}
	cmd.bvalue[0] &= led->state;
	cmd.bvalue[1] &= led->state;

	return mamba_te_send_command(m, &cmd);
}

static int mamba_te_send_set_led_mode_command(struct razer_mouse *m,
					      struct mamba_te_led *led)
{
	struct mamba_te_command cmd;

	cmd = MAMBA_TE_COMMAND_INIT;
	cmd.size = MAMBA_TE_REQUEST_SIZE_SET_LED_MODE;
	cmd.request = cpu_to_be16(MAMBA_TE_REQUEST_SET_LED_MODE);
	if (led->mode == MAMBA_TE_LED_MODE_STATIC) {
		cmd.bvalue[0] = led->mode;
		cmd.bvalue[1] = led->color.r;
		cmd.bvalue[2] = led->color.g;
		cmd.bvalue[3] = led->color.b;
	} else {
		uint16_t tempHex;
		uint8_t tempH, tempL;

		tempHex = led->mode;
		tempH = (uint8_t)((tempHex & 0xFF00) >> 8);
		tempL = (uint8_t)(tempHex & 0x00FF);
		cmd.bvalue[0] = tempH;
		cmd.bvalue[1] = tempL;
		cmd.bvalue[2] = led->color.r;
		cmd.bvalue[3] = led->color.g;
		cmd.bvalue[4] = led->color.b;
	}
	cmd.bvalue[0] &= led->state;
	cmd.bvalue[1] &= led->state;

	return mamba_te_send_command(m, &cmd);
}

static int mamba_te_send_set_led_color_command(struct razer_mouse *m,
					       struct mamba_te_led *led)
{
	struct mamba_te_command cmd;

	cmd = MAMBA_TE_COMMAND_INIT;
	cmd.size = MAMBA_TE_REQUEST_SIZE_SET_LED_COLOR;
	cmd.request = cpu_to_be16(MAMBA_TE_REQUEST_SET_LED_COLOR);
	if (led->mode == MAMBA_TE_LED_MODE_STATIC) {
		cmd.bvalue[0] = led->mode;
		cmd.bvalue[1] = led->color.r;
		cmd.bvalue[2] = led->color.g;
		cmd.bvalue[3] = led->color.b;
	} else {
		uint16_t tempHex;
		uint8_t tempH, tempL;

		tempHex = led->mode;
		tempH = (uint8_t)((tempHex & 0xFF00) >> 8);
		tempL = (uint8_t)(tempHex & 0x00FF);
		cmd.bvalue[0] = tempH;
		cmd.bvalue[1] = tempL;
		cmd.bvalue[2] = led->color.r;
		cmd.bvalue[3] = led->color.g;
		cmd.bvalue[4] = led->color.b;
	}
	cmd.bvalue[0] &= led->state;
	cmd.bvalue[1] &= led->state;

	return mamba_te_send_command(m, &cmd);
}

static int mamba_te_get_fw_version(struct razer_mouse *m)
{
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;

	return drv_data->fw_version;
}

static struct razer_mouse_profile *mamba_te_get_profiles(struct razer_mouse *m)
{
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;

	return &drv_data->profile;
}

static int mamba_te_supported_axes(struct razer_mouse *m,
				   struct razer_axis **res_ptr)
{
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;
	*res_ptr = drv_data->axes;

	return ARRAY_SIZE(drv_data->axes);
}

static int mamba_te_supported_dpimappings(struct razer_mouse *m,
					  struct razer_mouse_dpimapping **res_ptr)
{
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;
	*res_ptr = drv_data->dpimappings;

	return ARRAY_SIZE(drv_data->dpimappings);
}

static int mamba_te_supported_resolutions(struct razer_mouse *m,
					  enum razer_mouse_res **res_ptr)
{
	size_t i;
	size_t step_number;

	step_number = MAMBA_TE_MAX_RESOLUTION / MAMBA_TE_RESOLUTION_STEP;

	*res_ptr = calloc(step_number, sizeof(enum razer_mouse_res));
	if (!*res_ptr)
		return -ENOMEM;
	for (i = 0; i < step_number; i++)
		(*res_ptr)[i] = (i + 1) * MAMBA_TE_RESOLUTION_STEP;

	return step_number;
}

static int mamba_te_supported_freqs(struct razer_mouse *m,
				    enum razer_mouse_freq **res_ptr)
{
	*res_ptr = malloc(sizeof(mamba_te_freqs_list));
	if (!*res_ptr)
		return -ENOMEM;
	memcpy(*res_ptr, mamba_te_freqs_list, sizeof(mamba_te_freqs_list));

	return MAMBA_TE_SUPPORTED_FREQ_NUM;
}

static enum razer_mouse_freq mamba_te_get_freq(struct razer_mouse_profile *p)
{
	struct mamba_te_driver_data *drv_data;

	drv_data = p->mouse->drv_data;

	return drv_data->current_freq;
}

static struct razer_mouse_dpimapping *mamba_te_get_dpimapping(struct razer_mouse_profile *p,
							      struct razer_axis *axis)
{
	struct mamba_te_driver_data *drv_data;

	drv_data = p->mouse->drv_data;

	return drv_data->current_dpimapping;
}

static int mamba_te_change_dpimapping(struct razer_mouse_dpimapping *d,
				      enum razer_dimension dim,
				      enum razer_mouse_res res)
{
	struct mamba_te_driver_data *drv_data;

	if (!(d->dimension_mask & (1 << dim)))
		return -EINVAL;

	if (res == RAZER_MOUSE_RES_UNKNOWN)
		res = RAZER_MOUSE_RES_1800DPI;

	if (res < RAZER_MOUSE_RES_100DPI || res > RAZER_MOUSE_RES_10000DPI)
		return -EINVAL;

	d->res[dim] = res;

	drv_data = d->mouse->drv_data;
	if (d == drv_data->current_dpimapping)
		return mamba_te_send_set_resolution_command(d->mouse);

	return 0;
}

static int mamba_te_led_toggle_state(struct razer_led *led,
				     enum razer_led_state new_state)
{
	struct mamba_te_driver_data *drv_data;
	struct mamba_te_led *priv_led;

	drv_data = led->u.mouse->drv_data;
	priv_led = mamba_te_get_led(drv_data);
	if (!priv_led)
		return -EINVAL;

	switch (new_state) {
	case RAZER_LED_UNKNOWN:
	case RAZER_LED_ON:
		priv_led->state = MAMBA_TE_LED_STATE_ON;
		break;
	case RAZER_LED_OFF:
		priv_led->state = MAMBA_TE_LED_STATE_OFF;
		break;
	}

	return mamba_te_send_set_led_state_command(led->u.mouse, priv_led);
}

static int mamba_te_led_change_color(struct razer_led *led,
				     const struct razer_rgb_color *new_color)
{
	struct mamba_te_driver_data *drv_data;
	struct mamba_te_led *priv_led;

	drv_data = led->u.mouse->drv_data;
	priv_led = mamba_te_get_led(drv_data);

	if (!priv_led)
		return -EINVAL;
	if (priv_led->mode == MAMBA_TE_LED_MODE_SPECTRUM)
		return -EINVAL;

	priv_led->color = (struct mamba_te_rgb_color){
		.r = new_color->r,
		.g = new_color->g,
		.b = new_color->b,
	};

	return mamba_te_send_set_led_color_command(led->u.mouse, priv_led);
}

static int mamba_te_set_freq(struct razer_mouse_profile *p,
			     enum razer_mouse_freq freq)
{
	struct mamba_te_driver_data *drv_data;

	if (freq == RAZER_MOUSE_FREQ_UNKNOWN)
		freq = RAZER_MOUSE_FREQ_500HZ;

	if (freq != RAZER_MOUSE_FREQ_125HZ &&
	    freq != RAZER_MOUSE_FREQ_500HZ &&
	    freq != RAZER_MOUSE_FREQ_1000HZ)
		return -EINVAL;

	drv_data = p->mouse->drv_data;
	drv_data->current_freq = freq;

	return mamba_te_send_set_frequency_command(p->mouse);
}

static int mamba_te_set_dpimapping(struct razer_mouse_profile *p,
				   struct razer_axis *axis,
				   struct razer_mouse_dpimapping *d)
{
	struct mamba_te_driver_data *drv_data;

	if (axis && axis->id > 0)
		return -EINVAL;

	drv_data = p->mouse->drv_data;
	drv_data->current_dpimapping = &drv_data->dpimappings[d->nr];

	return mamba_te_send_set_resolution_command(p->mouse);
}

static int mamba_te_translate_led_mode(enum mamba_te_led_mode mode)
{
	switch (mode) {
	case MAMBA_TE_LED_MODE_STATIC:
		return RAZER_LED_MODE_STATIC;
	case MAMBA_TE_LED_MODE_BREATHING:
		return RAZER_LED_MODE_BREATHING;
	case MAMBA_TE_LED_MODE_SPECTRUM:
		return RAZER_LED_MODE_SPECTRUM;
	case MAMBA_TE_LED_MODE_WAVE:
		return RAZER_LED_MODE_WAVE;
	case MAMBA_TE_LED_MODE_REACTION:
		return RAZER_LED_MODE_REACTION;
	default:
		return -EINVAL;
	}
}

static int mamba_te_translate_razer_led_mode(enum razer_led_mode mode)
{
	switch (mode) {
	case RAZER_LED_MODE_STATIC:
		return MAMBA_TE_LED_MODE_STATIC;
	case RAZER_LED_MODE_BREATHING:
		return MAMBA_TE_LED_MODE_BREATHING;
	case RAZER_LED_MODE_SPECTRUM:
		return MAMBA_TE_LED_MODE_SPECTRUM;
	case RAZER_LED_MODE_WAVE:
		return MAMBA_TE_LED_MODE_WAVE;
	case RAZER_LED_MODE_REACTION:
		return MAMBA_TE_LED_MODE_REACTION;
	default:
		return -EINVAL;
	}
}

static int mamba_te_led_set_mode(struct razer_led *led,
				 enum razer_led_mode new_mode)
{
	int err;
	struct mamba_te_driver_data *drv_data;
	struct mamba_te_led *priv_led;

	drv_data = led->u.mouse->drv_data;
	priv_led = mamba_te_get_led(drv_data);

	if (!priv_led)
		return -EINVAL;

	err = mamba_te_translate_razer_led_mode(new_mode);
	if (err < 0)
		return err;
	priv_led->mode = err;

	return mamba_te_send_set_led_mode_command(led->u.mouse, priv_led);
}

static int mamba_te_get_leds(struct razer_mouse *m,
			     struct razer_led **leds_list)
{
	unsigned int supported_modes;
	enum razer_led_state led_state;
	struct mamba_te_driver_data *drv_data;
	struct razer_led *led;
	drv_data = m->drv_data;

	led = zalloc(sizeof(struct razer_led));
	if (!led)
		return -ENOMEM;

	supported_modes = (1 << RAZER_LED_MODE_BREATHING) |
			  (1 << RAZER_LED_MODE_SPECTRUM) |
			  (1 << RAZER_LED_MODE_STATIC) |
			  (1 << RAZER_LED_MODE_WAVE) |
			  (1 << RAZER_LED_MODE_REACTION);
	led_state = drv_data->led.state == MAMBA_TE_LED_STATE_OFF ?
		    RAZER_LED_OFF : RAZER_LED_ON;
	*led = (struct razer_led){
		.name = MAMBA_TE_LED_NAME,
		.state = led_state,
		.u.mouse = m,
		.toggle_state = mamba_te_led_toggle_state,
		.change_color = mamba_te_led_change_color,
		.set_mode = mamba_te_led_set_mode,
		.color = {
			.r = drv_data->led.color.r,
			.g = drv_data->led.color.g,
			.b = drv_data->led.color.b,
			.valid = 1,
		},
		.supported_modes_mask = supported_modes,
		.mode = mamba_te_translate_led_mode(drv_data->led.mode),
	};
	*leds_list = led;

	return MAMBA_TE_LED_NUM;
}

int razer_mamba_te_init(struct razer_mouse *m,
			struct libusb_device *usbdev)
{
	int err;
	size_t i;
	struct mamba_te_driver_data *drv_data;
	struct mamba_te_led *led;

	BUILD_BUG_ON(sizeof(struct mamba_te_command) != 90);

	drv_data = zalloc(sizeof(*drv_data));
	if (!drv_data)
		return -ENOMEM;

	razer_event_spacing_init(&drv_data->packet_spacing,
				 MAMBA_TE_PACKET_SPACING_MS);

	for (i = 0; i < MAMBA_TE_DPIMAPPINGS_NUM; i++) {
		drv_data->dpimappings[i] = (struct razer_mouse_dpimapping){
			.nr = i,
			.change = mamba_te_change_dpimapping,
			.dimension_mask = (1 << RAZER_DIM_X) |
					  (1 << RAZER_DIM_Y),
			.mouse = m,
		};
		drv_data->dpimappings[i].res[RAZER_DIM_X] =
			mamba_te_resolution_stages_list[i];
		drv_data->dpimappings[i].res[RAZER_DIM_Y] =
			mamba_te_resolution_stages_list[i];
	}
	drv_data->current_dpimapping = &drv_data->dpimappings[1];
	drv_data->current_freq = RAZER_MOUSE_FREQ_500HZ;

	drv_data->led = (struct mamba_te_led){
		.mode = MAMBA_TE_LED_MODE_STATIC,
		.state = MAMBA_TE_LED_STATE_ON,
		.color = {0x00, 0xFF, 0x00},
	};

	razer_init_axes(drv_data->axes, "X/Y",
			RAZER_AXIS_INDEPENDENT_DPIMAPPING,
			"Scroll", 0, NULL, 0);

	m->drv_data = drv_data;

	if ((err = razer_usb_add_used_interface(m->usb_ctx, 0, 0)) ||
	    (err = m->claim(m))) {
		free(drv_data);
		return err;
	}

	led = &drv_data->led;

	if ((err = mamba_te_send_init_command (m)) ||
	    (err = mamba_te_send_set_resolution_command (m)) ||
	    (err = mamba_te_send_get_firmware_command (m)) ||
	    (err = mamba_te_send_get_serial_no_command (m)) ||
	    (err = mamba_te_send_set_frequency_command (m)) ||
	    (err = mamba_te_send_set_led_mode_command (m, led)) ||
	    (err = mamba_te_send_set_led_color_command (m, led))) {
		m->release(m);
		free(drv_data);
		return err;
	}
	m->release(m);

	drv_data->profile = (struct razer_mouse_profile){
		.mouse = m,
		.get_freq = mamba_te_get_freq,
		.set_freq = mamba_te_set_freq,
		.get_dpimapping = mamba_te_get_dpimapping,
		.set_dpimapping = mamba_te_set_dpimapping,
	};

	razer_generic_usb_gen_idstr(usbdev, m->usb_ctx->h,
				    MAMBA_TE_DEVICE_NAME, false,
				    drv_data->serial, m->idstr);

	m->type = RAZER_MOUSETYPE_MAMBA_TE;
	m->get_fw_version = mamba_te_get_fw_version;
	m->global_get_leds = mamba_te_get_leds;
	m->get_profiles = mamba_te_get_profiles;
	m->supported_axes = mamba_te_supported_axes;
	m->supported_resolutions = mamba_te_supported_resolutions;
	m->supported_freqs = mamba_te_supported_freqs;
	m->supported_dpimappings = mamba_te_supported_dpimappings;

	return 0;
}

void razer_mamba_te_release(struct razer_mouse *m)
{
	struct mamba_te_driver_data *drv_data;

	drv_data = m->drv_data;
	free(drv_data);
	m->drv_data = NULL;
}
