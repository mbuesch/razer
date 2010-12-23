/*
 *   Cypress bootloader driver
 *   Firmware update support for Cypress based devices
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

#include "cypress_bootloader.h"
#include "razer_private.h"


#define CYPRESS_USB_TIMEOUT	1000

struct cypress_command {
	be16_t command;
	uint8_t key[8];
	uint8_t payload[54];
} _packed;

#define CYPRESS_CMD_ENTERBL	cpu_to_be16(0xFF38) /* Enter bootloader */
#define CYPRESS_CMD_WRITEFL	cpu_to_be16(0xFF39) /* Write flash */
#define CYPRESS_CMD_VERIFYFL	cpu_to_be16(0xFF3A) /* Verify flash */
#define CYPRESS_CMD_EXITBL	cpu_to_be16(0xFF3B) /* Exit bootloader */
#define CYPRESS_CMD_UPCHK	cpu_to_be16(0xFF3C) /* Update checksum */

struct cypress_status {
	uint8_t status0;
	uint8_t status1;
	uint8_t _padding[62];
} _packed;

#define CYPRESS_STAT_BLMODE	0x20 /* Bootload mode (success) */
#define CYPRESS_STAT_BOOTOK	0x01 /* Boot completed OK */
#define CYPRESS_STAT_IMAGERR	0x02 /* Image verify error */
#define CYPRESS_STAT_FLCHK	0x04 /* Flash checksum error */
#define CYPRESS_STAT_FLPROT	0x08 /* Flash protection error */
#define CYPRESS_STAT_COMCHK	0x10 /* Communication checksum error */
#define CYPRESS_STAT_INVALKEY	0x40 /* Invalid bootloader key */
#define CYPRESS_STAT_INVALCMD	0x80 /* Invalid command error */
#define CYPRESS_STAT_ALL	0xFF


static void cypress_print_one_status(int *ctx, FILE *fd, const char *message)
{
	if (*ctx)
		fprintf(fd, ", ");
	fprintf(fd, message);
	(*ctx)++;
}

static void cypress_print_status(FILE *fd, uint8_t status)
{
	int ctx = 0;

	fprintf(fd, "(");
	if (!(status & CYPRESS_STAT_BLMODE))
		cypress_print_one_status(&ctx, fd, "Not in bootloader mode");
	if (status & CYPRESS_STAT_IMAGERR)
		cypress_print_one_status(&ctx, fd, "Image verify error");
	if (status & CYPRESS_STAT_FLCHK)
		cypress_print_one_status(&ctx, fd, "Flash checksum error");
	if (status & CYPRESS_STAT_FLPROT)
		cypress_print_one_status(&ctx, fd, "Flash protection error");
	if (status & CYPRESS_STAT_COMCHK)
		cypress_print_one_status(&ctx, fd, "Communication checksum error");
	if (status & CYPRESS_STAT_INVALKEY)
		cypress_print_one_status(&ctx, fd, "Invalid bootloader key");
	if (status & CYPRESS_STAT_INVALCMD)
		cypress_print_one_status(&ctx, fd, "Invalid command");
	fprintf(fd, ")");
}

static void cmd_checksum(struct cypress_command *_cmd)
{
	char *cmd = (char *)_cmd;
	unsigned int i, sum = 0;

	for (i = 0; i < 45; i++)
		sum += cmd[i];
	cmd[45] = sum & 0xFF;
}

static int cypress_send_command(struct cypress *c,
				struct cypress_command *command,
				size_t command_size, uint8_t status_mask)
{
	struct cypress_status status;
	int res;
	uint8_t stat;

	cmd_checksum(command);//XXX

//printf("cmd = 0x%02X\n", be16_to_cpu(command->command));
	res = usb_bulk_write(c->usb.h, c->ep_out, (const char *)command, command_size,
			     CYPRESS_USB_TIMEOUT);
	if (res != command_size) {
		fprintf(stderr, "cypress: Failed to send command 0x%02X: %s\n",
			be16_to_cpu(command->command), usb_strerror());
		return -1;
	}
	razer_msleep(100);
	res = usb_bulk_read(c->usb.h, c->ep_in, (char *)&status, sizeof(status),
			    CYPRESS_USB_TIMEOUT);
	if (res != sizeof(status)) {
		fprintf(stderr, "cypress: Failed to receive status report: %s\n",
			usb_strerror());
		return -1;
	}
	status_mask |= CYPRESS_STAT_BLMODE; /* Always check the blmode bit */
	status_mask &= ~CYPRESS_STAT_BOOTOK; /* Always ignore the bootok bit */
	stat = (status.status0 | status.status1) & status_mask;
	if (stat != CYPRESS_STAT_BLMODE) {
		fprintf(stderr, "cypress: Command 0x%02X failed with "
			"status0=0x%02X status1=0x%02X ",
			be16_to_cpu(command->command),
			status.status0, status.status1);
		cypress_print_status(stderr, stat);
		fprintf(stderr, "\n");
		return -1;
	}

	return 0;
}

void cypress_assign_default_key(uint8_t *key)
{
	unsigned int i;

	for (i = 0; i < 8; i++)
		key[i] = i;
}

static int cypress_cmd_enterbl(struct cypress *c)
{
	struct cypress_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.command = CYPRESS_CMD_ENTERBL;
	c->assign_key(cmd.key);

	return cypress_send_command(c, &cmd, sizeof(cmd),
				    CYPRESS_STAT_INVALKEY |
				    CYPRESS_STAT_INVALCMD);
}

static int cypress_cmd_exitbl(struct cypress *c)
{
	struct cypress_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.command = CYPRESS_CMD_EXITBL;
	c->assign_key(cmd.key);

	return cypress_send_command(c, &cmd, sizeof(cmd),
				    CYPRESS_STAT_ALL);
}

static int cypress_cmd_verifyfl(struct cypress *c)
{
	struct cypress_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.command = CYPRESS_CMD_VERIFYFL;
	c->assign_key(cmd.key);

	return cypress_send_command(c, &cmd, sizeof(cmd),
				    CYPRESS_STAT_ALL);
}

static int cypress_cmd_writefl(struct cypress *c, uint16_t blocknr,
			       uint8_t segment, const char *data)
{
	struct cypress_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.command = CYPRESS_CMD_WRITEFL;
	c->assign_key(cmd.key);

	cmd.payload[0] = blocknr >> 8;
	cmd.payload[1] = blocknr;
	cmd.payload[2] = segment;
	memcpy(&cmd.payload[3], data, 32);

	return cypress_send_command(c, &cmd, sizeof(cmd),
				    CYPRESS_STAT_ALL);
}

static int cypress_writeflash(struct cypress *c,
			      const char *image, size_t len)
{
	unsigned int block;
	int err;

	if (len % 64) {
		fprintf(stderr, "cypress: internal error\n");
		return -1;
	}

	for (block = 0; block < len / 64; block++) {
		/* First 32 bytes */
		err = cypress_cmd_writefl(c, block, 0, image);
		if (err) {
			fprintf(stderr, "cypress: Failed to write image "
				"(block %u, segment 0)\n",
				block);
			return -1;
		}
		image += 32;
fprintf(stderr, ".");
		/* Last 32 bytes */
		err = cypress_cmd_writefl(c, block, 1, image);
		if (err) {
			fprintf(stderr, "cypress: Failed to write image "
				"(block %u, segment 1)\n",
				block);
			return -1;
		}
		image += 32;
fprintf(stderr, ".");
	}

	return 0;
}

int cypress_open(struct cypress *c, struct usb_device *dev,
		 void (*assign_key)(uint8_t *key))
{
	int err;
	unsigned int i;
	bool have_in = 0, have_out = 0;
	struct usb_interface_descriptor *alt;
	struct usb_endpoint_descriptor *ep;

	BUILD_BUG_ON(sizeof(struct cypress_command) != 64);
	BUILD_BUG_ON(sizeof(struct cypress_status) != 64);

return -1; //FIXME: Does not work, yet.

	if (!assign_key) {
		fprintf(stderr, "cypress_open: assign_key must not be NULL\n");
		return -1;
	}
	c->assign_key = assign_key;

	c->usb.dev = dev;
	err = razer_generic_usb_claim(&c->usb);
	if (err) {
		fprintf(stderr, "cypress: Failed to open and claim device\n");
		return -1;
	}
	alt = &c->usb.dev->config->interface->altsetting[0];
	for (i = 0; i < alt->bNumEndpoints; i++) {
		ep = &alt->endpoint[i];
		if (!have_in && (ep->bEndpointAddress & USB_ENDPOINT_IN)) {
			c->ep_in = ep->bEndpointAddress;
			have_in = 1;
			continue;
		}
		if (!have_out && !(ep->bEndpointAddress & USB_ENDPOINT_IN)) {
			c->ep_out = ep->bEndpointAddress;
			have_out = 1;
		}
		if (have_in && have_out)
			break;
	}
	if (!have_in || !have_out) {
		fprintf(stderr, "cypress: Did not find in and out endpoints (%u %u)\n",
			have_in, have_out);
		razer_generic_usb_release(&c->usb);
		return -1;
	}

	return 0;
}

void cypress_close(struct cypress *c)
{
	razer_generic_usb_release(&c->usb);
	c->assign_key = NULL;
}

int cypress_upload_image(struct cypress *c,
			 const char *image, size_t len)
{
	int err;
	int result = 0;

	if (len % 64) {
		fprintf(stderr, "cypress: Image size is not a multiple "
			"of the block size (64)\n");
		return -1;
	}

	err = cypress_cmd_enterbl(c);
	if (err) {
		fprintf(stderr, "cypress: Failed to enter bootloader\n");
		result = -1;
		goto out;
	}
	err = cypress_writeflash(c, image, len);
	if (err) {
		fprintf(stderr, "cypress: Failed to write flash image\n");
		result = -1;
		goto out;
	}
/*	err = cypress_cmd_verifyfl(c);
	if (err) {
		fprintf(stderr, "cypress: Failed to verify the flash\n");
		result = -1;
		goto out;
	}*/
	err = cypress_cmd_exitbl(c);
	if (err) {
		fprintf(stderr, "cypress: Failed to exit bootloader\n");
		result = -1;
	}
out:

	return result;
}
