/*
 *   Razer daemon
 *
 *   Copyright (C) 2008 Michael Buesch <mb@bu3sch.de>
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

#include <stdio.h>


int main(int argc, char **argv)
{
	struct razer_mouse *mice, *mouse;

	razer_init();

	while (1) {
		mice = razer_rescan_mice();
		for (mouse = mice; mouse; mouse = mouse->next) {
			printf("Have mouse: %s\n", mouse->idstr);
		}
		printf("\n");
		sleep(1);
	}

	razer_exit();
}
