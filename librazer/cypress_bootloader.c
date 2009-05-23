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


int cypress_open(struct cypress *c, struct usb_device *dev)
{
	int err;

	c->usb.dev = dev;
	err = razer_generic_usb_claim(&c->usb);
	if (err) {
		fprintf(stderr, "cypress: Failed to open and claim device\n");
		return -1;
	}

	return 0;
}

void cypress_close(struct cypress *c)
{
	razer_generic_usb_release(&c->usb);
}

int cypress_upload_image(struct cypress *c,
			 const char *image, size_t len)
{
	//TODO
	return 0;
}
