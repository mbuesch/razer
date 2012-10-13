/*
 *   Lowlevel hardware access for the
 *   Razer Lachesis 5600 DPI mouse
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

#include "hw_lachesis5k6.h"
#include "synapse.h"
#include "razer_private.h"


int razer_lachesis5k6_init(struct razer_mouse *m,
			   struct libusb_device *usbdev)
{
	int err;

	err = razer_synapse_init(m, RAZER_SYNFEAT_RGBLEDS);
	if (err)
		return err;

	razer_generic_usb_gen_idstr(usbdev, m->usb_ctx->h, "Lachesis 5600 DPI", 1,
				    razer_synapse_get_serial(m), m->idstr);

	return 0;
}

void razer_lachesis5k6_release(struct razer_mouse *m)
{
	razer_synapse_exit(m);
}
