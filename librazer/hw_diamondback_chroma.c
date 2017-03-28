/*
 * Lowlevel hardware access for the Razer Diamondback Chroma mouse.
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

#include "hw_diamondback_chroma.h"
#include "razer_private.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static enum razer_mouse_freq diamondback_chroma_freqs_list[] =
{
	RAZER_MOUSE_FREQ_125HZ,
	RAZER_MOUSE_FREQ_500HZ,
	RAZER_MOUSE_FREQ_1000HZ
};

static enum razer_mouse_res diamondback_chroma_resolution_stages_list[] =
{
	RAZER_MOUSE_RES_800DPI,
	RAZER_MOUSE_RES_1800DPI,
	RAZER_MOUSE_RES_3500DPI,
	RAZER_MOUSE_RES_5600DPI,
	RAZER_MOUSE_RES_10000DPI,
	RAZER_MOUSE_RES_16000DPI
};

#define DIAMONDBACK_CHROMA_DEVICE_NAME	"Diamondback Chroma"
#define DIAMONDBACK_CHROMA_LED_NAME	"Basic"


enum diamondback_chroma_led_mode
{
	DIAMONDBACK_CHROMA_LED_MODE_STATIC		= 0x06,
	DIAMONDBACK_CHROMA_LED_MODE_BREATHING		= 0x0301, // 0x0302 is breathing with 2 different colors
	DIAMONDBACK_CHROMA_LED_MODE_SPECTRUM		= 0x0400,
	DIAMONDBACK_CHROMA_LED_MODE_WAVE			= 0x0101, // 0x0102 is waving in a opposite direction
	DIAMONDBACK_CHROMA_LED_MODE_REACTION		= 0x0203,
	DIAMONDBACK_CHROMA_LED_MODE_CUSTOMIZED		= 0x0500,
	DIAMONDBACK_CHROMA_LED_MODE_CUSTOMIZED_COLOR_INFO	= 0x050c, // for further implementation
};

enum diamondback_chroma_led_state
{
	DIAMONDBACK_CHROMA_LED_STATE_OFF			= 0x00,
	DIAMONDBACK_CHROMA_LED_STATE_ON			= 0xFF,
};

//for further implementation
enum diamondback_chroma_led_state_option
{
	DIAMONDBACK_CHROMA_LED_STATE_OPTION_0		= 0x00,
	DIAMONDBACK_CHROMA_LED_STATE_OPTION_1		= 0x01,
	DIAMONDBACK_CHROMA_LED_STATE_OPTION_2		= 0x02,
	DIAMONDBACK_CHROMA_LED_STATE_OPTION_3		= 0x03,
};

/*
 * The 6th byte of Diamondback Chroma's command seems also to be the size of arguments
 * (size of arguments to read in case of read operations). It's not necessarily
 * so, since some values are slightly off (i.e. bigger than the apparent size
 * of the arguments). But when it's in customized mode, there will be 0x32 which is 50 bytes of information
 * Experiments suggest that the value given in the 'size' byte does not matter.
 * I chose to go with the values used by the Synapse driver.
 */
enum diamondback_chroma_request_size
{
	DIAMONDBACK_CHROMA_REQUEST_SIZE_INIT		= 0x02,
	DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_RESOLUTION	= 0x07,
	DIAMONDBACK_CHROMA_REQUEST_SIZE_GET_FIRMWARE	= 0x04,
	DIAMONDBACK_CHROMA_REQUEST_SIZE_GET_SERIAL_NO	= 0x16,
	DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_FREQUENCY	= 0x01,
	DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_LED_STATE	= 0x08,
	DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_LED_MODE	= 0x08,
	DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_LED_COLOR	= 0x08,
};

enum diamondback_chroma_request
{
	DIAMONDBACK_CHROMA_REQUEST_INIT			= 0x0004,
	DIAMONDBACK_CHROMA_REQUEST_SET_RESOLUTION		= 0x0405,
	DIAMONDBACK_CHROMA_REQUEST_GET_FIRMWARE		= 0x0087,
	DIAMONDBACK_CHROMA_REQUEST_GET_SERIAL_NO		= 0x0082,
	DIAMONDBACK_CHROMA_REQUEST_SET_FREQUENCY		= 0x0005,
	DIAMONDBACK_CHROMA_REQUEST_SET_LED_STATE		= 0x030a,
	DIAMONDBACK_CHROMA_REQUEST_SET_LED_COLOR		= 0x030a,
	DIAMONDBACK_CHROMA_REQUEST_SET_LED_MODE		= 0x030a,
	DIAMONDBACK_CHROMA_REQUEST_SET_LED_MODE_CUSTOMIZED = 0x030c, // for further implementation
};

enum diamondback_chroma_constants
{
	DIAMONDBACK_CHROMA_MAX_FREQUENCY			= RAZER_MOUSE_FREQ_1000HZ,
	DIAMONDBACK_CHROMA_MAX_RESOLUTION			= RAZER_MOUSE_RES_16000DPI,
	DIAMONDBACK_CHROMA_RESOLUTION_STEP		= RAZER_MOUSE_RES_100DPI,

	DIAMONDBACK_CHROMA_LED_NUM			= 1,
	DIAMONDBACK_CHROMA_AXES_NUM			= 2,
	DIAMONDBACK_CHROMA_SUPPORTED_FREQ_NUM		= ARRAY_SIZE(diamondback_chroma_freqs_list),
	DIAMONDBACK_CHROMA_DPIMAPPINGS_NUM		= ARRAY_SIZE(diamondback_chroma_resolution_stages_list),

	DIAMONDBACK_CHROMA_USB_SETUP_PACKET_VALUE		= 0x300,
	DIAMONDBACK_CHROMA_SUCCESS_STATUS			= 0x02,
	DIAMONDBACK_CHROMA_PACKET_SPACING_MS		= 35,

	/*
	 * Experiments suggest that the value in the 'magic' byte of the command
	 * does not necessarily matter (e.g. the commands work when the 'magic'
	 * byte equals 0x77). I chose to go with the value used by the Synapse
	 * driver.
	 */
	DIAMONDBACK_CHROMA_MAGIC_BYTE			= 0xFF,

	/*
	 * These specific arg0 bytes are used by the Synapse driver for their
	 * respective commands. Their value may or may not matter (e.g. matters
	 * in the case of LED commands, doesn't seem to matter for the
	 * resolution command).
	 */
	DIAMONDBACK_CHROMA_LED_ARG0			= 0x02,
	DIAMONDBACK_CHROMA_INIT_ARG0			= 0x03,
	DIAMONDBACK_CHROMA_RESOLUTION_ARG0		= 0x01,
};

struct diamondback_chroma_command
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

#define DIAMONDBACK_CHROMA_COMMAND_INIT			\
	(struct diamondback_chroma_command)		\
	{					\
		.magic = DIAMONDBACK_CHROMA_MAGIC_BYTE	\
	}

struct diamondback_chroma_rgb_color
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct diamondback_chroma_led
{
	enum diamondback_chroma_led_mode mode;
	enum diamondback_chroma_led_state state;
	enum diamondback_chroma_led_state_option option;
	struct diamondback_chroma_rgb_color color;
};

struct diamondback_chroma_driver_data
{
	struct razer_event_spacing packet_spacing;
	struct razer_mouse_profile profile;
	struct razer_mouse_dpimapping *current_dpimapping;
	enum razer_mouse_freq current_freq;
	struct diamondback_chroma_led led;
	struct razer_mouse_dpimapping dpimappings[DIAMONDBACK_CHROMA_DPIMAPPINGS_NUM];
	struct razer_axis axes[DIAMONDBACK_CHROMA_AXES_NUM];
	uint16_t fw_version;
	char serial[DIAMONDBACK_CHROMA_REQUEST_SIZE_GET_SERIAL_NO + 1];
};

static uint8_t diamondback_chroma_checksum(struct diamondback_chroma_command *cmd)
{
	size_t control_size;

	control_size = sizeof(cmd->size) + sizeof(cmd->request);
	return razer_xor8_checksum((uint8_t *)&cmd->size, control_size + cmd->size);
}

static int diamondback_chroma_translate_frequency(enum razer_mouse_freq freq)
{
	switch (freq) {
	case RAZER_MOUSE_FREQ_UNKNOWN:
		freq = RAZER_MOUSE_FREQ_500HZ;
		/* fallthrough */
	case RAZER_MOUSE_FREQ_125HZ:
	case RAZER_MOUSE_FREQ_500HZ:
	case RAZER_MOUSE_FREQ_1000HZ:
		return DIAMONDBACK_CHROMA_MAX_FREQUENCY / freq;
	default:
		return -EINVAL;
	}
}

static int diamondback_chroma_usb_action(struct razer_mouse *m,
			       enum libusb_endpoint_direction direction,
			       enum libusb_standard_request request,
			       uint16_t command,
			       struct diamondback_chroma_command *cmd)
{
	int err;
	struct diamondback_chroma_driver_data *drv_data;

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
		razer_error("razer-diamondback-chroma: "
			    "USB %s 0x%01X 0x%02X failed with %d\n",
			    direction == LIBUSB_ENDPOINT_IN ? "read" : "write",
			    request, command, err);
		return err;
	}

	return 0;
}

static int diamondback_chroma_send_command(struct razer_mouse *m,
				 struct diamondback_chroma_command *cmd)
{
	int err;
	uint8_t checksum;

	cmd->checksum = diamondback_chroma_checksum(cmd);
	err = diamondback_chroma_usb_action(m, LIBUSB_ENDPOINT_OUT,
				  LIBUSB_REQUEST_SET_CONFIGURATION,
				  DIAMONDBACK_CHROMA_USB_SETUP_PACKET_VALUE, cmd);
	if (err)
		return err;
	err = diamondback_chroma_usb_action(m, LIBUSB_ENDPOINT_IN,
				  LIBUSB_REQUEST_CLEAR_FEATURE,
				  DIAMONDBACK_CHROMA_USB_SETUP_PACKET_VALUE, cmd);
	if (err)
		return err;

	checksum = diamondback_chroma_checksum (cmd);
	if (checksum != cmd->checksum) {
		razer_error("razer-diamondback-chroma: "
			    "Command %02X %04X bad response checksum %02X "
			    "(expected %02X)\n",
			    cmd->size, be16_to_cpu(cmd->request),
			    checksum, cmd->checksum);
		return -EBADMSG;
	}

	if (cmd->status != DIAMONDBACK_CHROMA_SUCCESS_STATUS) {
		razer_error("razer-diamondback-chroma: "
			    "Command %02X %04X failed with %02X\n",
			    cmd->size, be16_to_cpu(cmd->request), cmd->status);
	}

	return 0;
}

static int diamondback_chroma_send_init_command(struct razer_mouse *m)
{
	struct diamondback_chroma_command cmd;

	cmd = DIAMONDBACK_CHROMA_COMMAND_INIT;
	cmd.size = DIAMONDBACK_CHROMA_REQUEST_SIZE_INIT;
	cmd.request = cpu_to_be16(DIAMONDBACK_CHROMA_REQUEST_INIT);
	cmd.bvalue[0] = DIAMONDBACK_CHROMA_INIT_ARG0;

	return diamondback_chroma_send_command(m, &cmd);
}

static int diamondback_chroma_send_set_resolution_command(struct razer_mouse *m)
{
	enum razer_mouse_res res_x, res_y;
	struct diamondback_chroma_command cmd;
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	res_x = drv_data->current_dpimapping->res[RAZER_DIM_X];
	res_y = drv_data->current_dpimapping->res[RAZER_DIM_Y];

	cmd = DIAMONDBACK_CHROMA_COMMAND_INIT;
	cmd.size = DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_RESOLUTION;
	cmd.request = cpu_to_be16(DIAMONDBACK_CHROMA_REQUEST_SET_RESOLUTION);
	cmd.bvalue[0] = DIAMONDBACK_CHROMA_RESOLUTION_ARG0;
	cmd.value[0] = cpu_to_be16(res_x);
	cmd.value[1] = cpu_to_be16(res_y);

	return diamondback_chroma_send_command(m, &cmd);
}

static int diamondback_chroma_send_get_firmware_command(struct razer_mouse *m)
{
	int err;
	uint8_t fw_major;
	uint16_t fw_minor;
	struct diamondback_chroma_command cmd;
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = m->drv_data;

	cmd = DIAMONDBACK_CHROMA_COMMAND_INIT;
	cmd.size = DIAMONDBACK_CHROMA_REQUEST_SIZE_GET_FIRMWARE;
	cmd.request = cpu_to_be16(DIAMONDBACK_CHROMA_REQUEST_GET_FIRMWARE);

	err = diamondback_chroma_send_command(m, &cmd);
	if (err)
		return err;

	fw_major = cmd.bvalue[0];
	fw_minor = be16_to_cpu(cmd.value[0]);
	drv_data->fw_version = (fw_major << 8) | fw_minor;

	return 0;
}

static int diamondback_chroma_send_get_serial_no_command(struct razer_mouse *m)
{
	int err;
	struct diamondback_chroma_command cmd;
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = m->drv_data;

	cmd = DIAMONDBACK_CHROMA_COMMAND_INIT;
	cmd.size = DIAMONDBACK_CHROMA_REQUEST_SIZE_GET_SERIAL_NO;
	cmd.request = cpu_to_be16(DIAMONDBACK_CHROMA_REQUEST_GET_SERIAL_NO);

	err = diamondback_chroma_send_command(m, &cmd);
	if (err)
		return err;

	strncpy(drv_data->serial, (const char *)cmd.bvalue,
		DIAMONDBACK_CHROMA_REQUEST_SIZE_GET_SERIAL_NO);
	drv_data->serial[DIAMONDBACK_CHROMA_REQUEST_SIZE_GET_SERIAL_NO] = '\0';

	return 0;
}

static int diamondback_chroma_send_set_frequency_command(struct razer_mouse *m)
{
	int tfreq;
	struct diamondback_chroma_command cmd;
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	cmd = DIAMONDBACK_CHROMA_COMMAND_INIT;

	tfreq = diamondback_chroma_translate_frequency(drv_data->current_freq);
	if (tfreq < 0)
		return tfreq;

	cmd.size = DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_FREQUENCY;
	cmd.request = cpu_to_be16(DIAMONDBACK_CHROMA_REQUEST_SET_FREQUENCY);
	cmd.bvalue[0] = tfreq;

	return diamondback_chroma_send_command(m, &cmd);
}

static struct diamondback_chroma_led *diamondback_chroma_get_led(struct diamondback_chroma_driver_data *d)
{
	return &d->led;
}

static int diamondback_chroma_send_set_led_state_command(struct razer_mouse *m,
					       struct diamondback_chroma_led *led)
{
	struct diamondback_chroma_command cmd;

	cmd = DIAMONDBACK_CHROMA_COMMAND_INIT;
	cmd.size = DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_LED_STATE;
	cmd.request = cpu_to_be16(DIAMONDBACK_CHROMA_REQUEST_SET_LED_STATE);

	if (led->mode == DIAMONDBACK_CHROMA_LED_MODE_STATIC) {
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

	return diamondback_chroma_send_command(m, &cmd);
}

static int diamondback_chroma_send_set_led_mode_command(struct razer_mouse *m,
					      struct diamondback_chroma_led *led)
{
	struct diamondback_chroma_command cmd;

	cmd = DIAMONDBACK_CHROMA_COMMAND_INIT;
	cmd.size = DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_LED_MODE;
	cmd.request = cpu_to_be16(DIAMONDBACK_CHROMA_REQUEST_SET_LED_MODE);
	if (led->mode == DIAMONDBACK_CHROMA_LED_MODE_STATIC) {
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

	return diamondback_chroma_send_command(m, &cmd);
}

static int diamondback_chroma_send_set_led_color_command(struct razer_mouse *m,
					       struct diamondback_chroma_led *led)
{
	struct diamondback_chroma_command cmd;

	cmd = DIAMONDBACK_CHROMA_COMMAND_INIT;
	cmd.size = DIAMONDBACK_CHROMA_REQUEST_SIZE_SET_LED_COLOR;
	cmd.request = cpu_to_be16(DIAMONDBACK_CHROMA_REQUEST_SET_LED_COLOR);
	if (led->mode == DIAMONDBACK_CHROMA_LED_MODE_STATIC) {
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

	return diamondback_chroma_send_command(m, &cmd);
}

static int diamondback_chroma_get_fw_version(struct razer_mouse *m)
{
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = m->drv_data;

	return drv_data->fw_version;
}

static struct razer_mouse_profile *diamondback_chroma_get_profiles(struct razer_mouse *m)
{
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = m->drv_data;

	return &drv_data->profile;
}

static int diamondback_chroma_supported_axes(struct razer_mouse *m,
				   struct razer_axis **res_ptr)
{
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	*res_ptr = drv_data->axes;

	return ARRAY_SIZE(drv_data->axes);
}

static int diamondback_chroma_supported_dpimappings(struct razer_mouse *m,
					  struct razer_mouse_dpimapping **res_ptr)
{
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	*res_ptr = drv_data->dpimappings;

	return ARRAY_SIZE(drv_data->dpimappings);
}

static int diamondback_chroma_supported_resolutions(struct razer_mouse *m,
					  enum razer_mouse_res **res_ptr)
{
	size_t i;
	size_t step_number;

	step_number = DIAMONDBACK_CHROMA_MAX_RESOLUTION / DIAMONDBACK_CHROMA_RESOLUTION_STEP;

	*res_ptr = calloc(step_number, sizeof(enum razer_mouse_res));
	if (!*res_ptr)
		return -ENOMEM;
	for (i = 0; i < step_number; i++)
		(*res_ptr)[i] = (i + 1) * DIAMONDBACK_CHROMA_RESOLUTION_STEP;

	return step_number;
}

static int diamondback_chroma_supported_freqs(struct razer_mouse *m,
				    enum razer_mouse_freq **res_ptr)
{
	*res_ptr = malloc(sizeof(diamondback_chroma_freqs_list));
	if (!*res_ptr)
		return -ENOMEM;
	memcpy(*res_ptr, diamondback_chroma_freqs_list, sizeof(diamondback_chroma_freqs_list));

	return DIAMONDBACK_CHROMA_SUPPORTED_FREQ_NUM;
}

static enum razer_mouse_freq diamondback_chroma_get_freq(struct razer_mouse_profile *p)
{
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = p->mouse->drv_data;

	return drv_data->current_freq;
}

static struct razer_mouse_dpimapping *diamondback_chroma_get_dpimapping(struct razer_mouse_profile *p,
							      struct razer_axis *axis)
{
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = p->mouse->drv_data;

	return drv_data->current_dpimapping;
}

static int diamondback_chroma_change_dpimapping(struct razer_mouse_dpimapping *d,
				      enum razer_dimension dim,
				      enum razer_mouse_res res)
{
	struct diamondback_chroma_driver_data *drv_data;

	if (!(d->dimension_mask & (1 << dim)))
		return -EINVAL;

	if (res == RAZER_MOUSE_RES_UNKNOWN)
		res = RAZER_MOUSE_RES_1800DPI;

	if (res < RAZER_MOUSE_RES_100DPI || res > RAZER_MOUSE_RES_16000DPI)
		return -EINVAL;

	d->res[dim] = res;

	drv_data = d->mouse->drv_data;
	if (d == drv_data->current_dpimapping)
		return diamondback_chroma_send_set_resolution_command(d->mouse);

	return 0;
}

static int diamondback_chroma_led_toggle_state(struct razer_led *led,
				     enum razer_led_state new_state)
{
	struct diamondback_chroma_driver_data *drv_data;
	struct diamondback_chroma_led *priv_led;

	drv_data = led->u.mouse->drv_data;
	priv_led = diamondback_chroma_get_led(drv_data);
	if (!priv_led)
		return -EINVAL;

	switch (new_state) {
	case RAZER_LED_UNKNOWN:
	case RAZER_LED_ON:
		priv_led->state = DIAMONDBACK_CHROMA_LED_STATE_ON;
		break;
	case RAZER_LED_OFF:
		priv_led->state = DIAMONDBACK_CHROMA_LED_STATE_OFF;
		break;
	}

	return diamondback_chroma_send_set_led_state_command(led->u.mouse, priv_led);
}

static int diamondback_chroma_led_change_color(struct razer_led *led,
				     const struct razer_rgb_color *new_color)
{
	struct diamondback_chroma_driver_data *drv_data;
	struct diamondback_chroma_led *priv_led;

	drv_data = led->u.mouse->drv_data;
	priv_led = diamondback_chroma_get_led(drv_data);

	if (!priv_led)
		return -EINVAL;
	if (priv_led->mode == DIAMONDBACK_CHROMA_LED_MODE_SPECTRUM)
		return -EINVAL;

	priv_led->color = (struct diamondback_chroma_rgb_color){
		.r = new_color->r,
		.g = new_color->g,
		.b = new_color->b,
	};

	return diamondback_chroma_send_set_led_color_command(led->u.mouse, priv_led);
}

static int diamondback_chroma_set_freq(struct razer_mouse_profile *p,
			     enum razer_mouse_freq freq)
{
	struct diamondback_chroma_driver_data *drv_data;

	if (freq == RAZER_MOUSE_FREQ_UNKNOWN)
		freq = RAZER_MOUSE_FREQ_500HZ;

	if (freq != RAZER_MOUSE_FREQ_125HZ &&
	    freq != RAZER_MOUSE_FREQ_500HZ &&
	    freq != RAZER_MOUSE_FREQ_1000HZ)
		return -EINVAL;

	drv_data = p->mouse->drv_data;
	drv_data->current_freq = freq;

	return diamondback_chroma_send_set_frequency_command(p->mouse);
}

static int diamondback_chroma_set_dpimapping(struct razer_mouse_profile *p,
				   struct razer_axis *axis,
				   struct razer_mouse_dpimapping *d)
{
	struct diamondback_chroma_driver_data *drv_data;

	if (axis && axis->id > 0)
		return -EINVAL;

	drv_data = p->mouse->drv_data;
	drv_data->current_dpimapping = &drv_data->dpimappings[d->nr];

	return diamondback_chroma_send_set_resolution_command(p->mouse);
}

static int diamondback_chroma_translate_led_mode(enum diamondback_chroma_led_mode mode)
{
	switch (mode) {
	case DIAMONDBACK_CHROMA_LED_MODE_STATIC:
		return RAZER_LED_MODE_STATIC;
	case DIAMONDBACK_CHROMA_LED_MODE_BREATHING:
		return RAZER_LED_MODE_BREATHING;
	case DIAMONDBACK_CHROMA_LED_MODE_SPECTRUM:
		return RAZER_LED_MODE_SPECTRUM;
	case DIAMONDBACK_CHROMA_LED_MODE_WAVE:
		return RAZER_LED_MODE_WAVE;
	case DIAMONDBACK_CHROMA_LED_MODE_REACTION:
		return RAZER_LED_MODE_REACTION;
	default:
		return -EINVAL;
	}
}

static int diamondback_chroma_translate_razer_led_mode(enum razer_led_mode mode)
{
	switch (mode) {
	case RAZER_LED_MODE_STATIC:
		return DIAMONDBACK_CHROMA_LED_MODE_STATIC;
	case RAZER_LED_MODE_BREATHING:
		return DIAMONDBACK_CHROMA_LED_MODE_BREATHING;
	case RAZER_LED_MODE_SPECTRUM:
		return DIAMONDBACK_CHROMA_LED_MODE_SPECTRUM;
	case RAZER_LED_MODE_WAVE:
		return DIAMONDBACK_CHROMA_LED_MODE_WAVE;
	case RAZER_LED_MODE_REACTION:
		return DIAMONDBACK_CHROMA_LED_MODE_REACTION;
	default:
		return -EINVAL;
	}
}

static int diamondback_chroma_led_set_mode(struct razer_led *led,
				 enum razer_led_mode new_mode)
{
	int err;
	struct diamondback_chroma_driver_data *drv_data;
	struct diamondback_chroma_led *priv_led;

	drv_data = led->u.mouse->drv_data;
	priv_led = diamondback_chroma_get_led(drv_data);

	if (!priv_led)
		return -EINVAL;

	err = diamondback_chroma_translate_razer_led_mode(new_mode);
	if (err < 0)
		return err;
	priv_led->mode = err;

	return diamondback_chroma_send_set_led_mode_command(led->u.mouse, priv_led);
}

static int diamondback_chroma_get_leds(struct razer_mouse *m,
			     struct razer_led **leds_list)
{
	unsigned int supported_modes;
	enum razer_led_state led_state;
	struct diamondback_chroma_driver_data *drv_data;
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
	led_state = drv_data->led.state == DIAMONDBACK_CHROMA_LED_STATE_OFF ?
		    RAZER_LED_OFF : RAZER_LED_ON;
	*led = (struct razer_led){
		.name = DIAMONDBACK_CHROMA_LED_NAME,
		.state = led_state,
		.u.mouse = m,
		.toggle_state = diamondback_chroma_led_toggle_state,
		.change_color = diamondback_chroma_led_change_color,
		.set_mode = diamondback_chroma_led_set_mode,
		.color = {
			.r = drv_data->led.color.r,
			.g = drv_data->led.color.g,
			.b = drv_data->led.color.b,
			.valid = 1,
		},
		.supported_modes_mask = supported_modes,
		.mode = diamondback_chroma_translate_led_mode(drv_data->led.mode),
	};
	*leds_list = led;

	return DIAMONDBACK_CHROMA_LED_NUM;
}

int razer_diamondback_chroma_init(struct razer_mouse *m,
			struct libusb_device *usbdev)
{
	int err;
	size_t i;
	struct diamondback_chroma_driver_data *drv_data;
	struct diamondback_chroma_led *led;

	BUILD_BUG_ON(sizeof(struct diamondback_chroma_command) != 90);

	drv_data = zalloc(sizeof(*drv_data));
	if (!drv_data)
		return -ENOMEM;

	razer_event_spacing_init(&drv_data->packet_spacing,
				 DIAMONDBACK_CHROMA_PACKET_SPACING_MS);

	for (i = 0; i < DIAMONDBACK_CHROMA_DPIMAPPINGS_NUM; i++) {
		drv_data->dpimappings[i] = (struct razer_mouse_dpimapping){
			.nr = i,
			.change = diamondback_chroma_change_dpimapping,
			.dimension_mask = (1 << RAZER_DIM_X) |
					  (1 << RAZER_DIM_Y),
			.mouse = m,
		};
		drv_data->dpimappings[i].res[RAZER_DIM_X] =
			diamondback_chroma_resolution_stages_list[i];
		drv_data->dpimappings[i].res[RAZER_DIM_Y] =
			diamondback_chroma_resolution_stages_list[i];
	}
	drv_data->current_dpimapping = &drv_data->dpimappings[1];
	drv_data->current_freq = RAZER_MOUSE_FREQ_500HZ;

	drv_data->led = (struct diamondback_chroma_led){
		.mode = DIAMONDBACK_CHROMA_LED_MODE_STATIC,
		.state = DIAMONDBACK_CHROMA_LED_STATE_ON,
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

	if ((err = diamondback_chroma_send_init_command (m)) ||
	    (err = diamondback_chroma_send_set_resolution_command (m)) ||
	    (err = diamondback_chroma_send_get_firmware_command (m)) ||
	    (err = diamondback_chroma_send_get_serial_no_command (m)) ||
	    (err = diamondback_chroma_send_set_frequency_command (m)) ||
	    (err = diamondback_chroma_send_set_led_mode_command (m, led)) ||
	    (err = diamondback_chroma_send_set_led_color_command (m, led))) {
		m->release(m);
		free(drv_data);
		return err;
	}
	m->release(m);

	drv_data->profile = (struct razer_mouse_profile){
		.mouse = m,
		.get_freq = diamondback_chroma_get_freq,
		.set_freq = diamondback_chroma_set_freq,
		.get_dpimapping = diamondback_chroma_get_dpimapping,
		.set_dpimapping = diamondback_chroma_set_dpimapping,
	};

	razer_generic_usb_gen_idstr(usbdev, m->usb_ctx->h,
				    DIAMONDBACK_CHROMA_DEVICE_NAME, false,
				    drv_data->serial, m->idstr);

	m->type = RAZER_MOUSETYPE_DIAMONDBACK_CHROMA;
	m->get_fw_version = diamondback_chroma_get_fw_version;
	m->global_get_leds = diamondback_chroma_get_leds;
	m->get_profiles = diamondback_chroma_get_profiles;
	m->supported_axes = diamondback_chroma_supported_axes;
	m->supported_resolutions = diamondback_chroma_supported_resolutions;
	m->supported_freqs = diamondback_chroma_supported_freqs;
	m->supported_dpimappings = diamondback_chroma_supported_dpimappings;

	return 0;
}

void razer_diamondback_chroma_release(struct razer_mouse *m)
{
	struct diamondback_chroma_driver_data *drv_data;

	drv_data = m->drv_data;
	free(drv_data);
	m->drv_data = NULL;
}
