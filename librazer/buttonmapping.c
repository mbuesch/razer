/*
 *   Physical->logical button mapping
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

#include "buttonmapping.h"

#include <string.h>
#include <errno.h>


int razer_create_buttonmap(void *buffer, size_t bufsize,
			   struct razer_buttonmapping *mappings, size_t nr_mappings,
			   unsigned int struct_spacing)
{
	uint8_t *buf = buffer;
	struct razer_buttonmapping *mapping;
	size_t i, bufptr = 0;

	for (i = 0; i < nr_mappings; i++) {
		mapping = &mappings[i];

		if (bufptr + sizeof(*mapping) >= bufsize) {
			razer_error("razer_create_buttonmap: Buffer overflow\n");
			return -ENOSPC;
		}
		memcpy(buf + bufptr, mapping, sizeof(*mapping));
		bufptr += sizeof(*mapping);
		bufptr += struct_spacing;
	}

	return 0;
}

int razer_parse_buttonmap(void *rawdata, size_t rawsize,
			  struct razer_buttonmapping *mappings, size_t nr_mappings,
			  unsigned int struct_spacing)
{
	uint8_t *raw = rawdata;
	size_t i, rawptr = 0, count;
	struct razer_buttonmapping *mapping, *target;

	for (i = 0; i < nr_mappings; i++)
		mappings[i].logical = 0;

	for (count = 0; count < nr_mappings; count++) {
		if (rawptr + sizeof(*mapping) >= rawsize) {
			razer_error("razer_parse_buttonmap: Raw data does not "
				"contain all mappings\n");
			return -EINVAL;
		}
		mapping = (struct razer_buttonmapping *)&raw[rawptr];
		target = NULL;
		for (i = 0; i < nr_mappings; i++) {
			if (mappings[i].physical == mapping->physical) {
				target = &mappings[i];
				break;
			}
		}
		if (!target) {
			razer_error("razer_parse_buttonmap: Got physical mapping 0x%02X, "
				"but that is not in the map\n", mapping->physical);
			return -EINVAL;
		}
		target->logical = mapping->logical; /* Assign the mapping */
		rawptr += sizeof(*mapping);
		if (!razer_buffer_is_all_zero(&raw[rawptr],
					      min(struct_spacing, rawsize - rawptr))) {
			razer_error("razer_parse_buttonmap: Buttonmap spacing contains "
				"nonzero data\n");
			return -EINVAL;
		}
		rawptr += struct_spacing;
	}

	for (i = 0; i < nr_mappings; i++) {
		mapping = &mappings[i];
		if (mapping->logical == 0) {
			razer_error("razer_parse_buttonmap: Logical mapping for 0x%02X "
				"was not found or is invalid\n", mapping->physical);
			return -EINVAL;
		}
	}

	return 0;
}
