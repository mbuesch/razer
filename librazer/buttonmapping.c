/*
 *   Physical->logical button mapping
 *
 *   Copyright (C) 2010-2011 Michael Buesch <m@bues.ch>
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

	memset(buffer, 0, bufsize);

	for (i = 0; i < nr_mappings; i++) {
		mapping = &mappings[i];

		if (bufptr + 2 >= bufsize) {
			razer_error("razer_create_buttonmap: Buffer overflow\n");
			return -ENOSPC;
		}
		buf[bufptr + 0] = mapping->physical;
		buf[bufptr + 1] = mapping->logical;

		bufptr += 2;
		bufptr += struct_spacing;
	}

	return 0;
}

int razer_parse_buttonmap(void *rawdata, size_t rawsize,
			  struct razer_buttonmapping *mappings, size_t nr_mappings,
			  unsigned int struct_spacing)
{
	uint8_t *raw = rawdata;
	size_t rawptr = 0, count;
	struct razer_buttonmapping mapping, *target;

	memset(mappings, 0, nr_mappings * sizeof(*mappings));

	target = mappings;
	for (count = 0; count < nr_mappings; count++) {
		if (rawptr + 2 >= rawsize) {
			razer_error("razer_parse_buttonmap: Raw data does not "
				"contain all mappings\n");
			return -EINVAL;
		}
		mapping.physical = raw[rawptr + 0];
		mapping.logical = raw[rawptr + 1];
		if (mapping.physical == 0) {
			razer_error("razer_parse_buttonmap: Physical mapping for %u "
				"is invalid\n", (unsigned int)count);
			return -EINVAL;
		}
		if (mapping.logical == 0) {
			razer_error("razer_parse_buttonmap: Logical mapping for 0x%02X "
				"is invalid\n", mapping.physical);
			return -EINVAL;
		}

		target->physical = mapping.physical;
		target->logical = mapping.logical;

		rawptr += 2;
		if (!razer_buffer_is_all_zero(&raw[rawptr],
					      min(struct_spacing, rawsize - rawptr))) {
			razer_debug("razer_parse_buttonmap: Buttonmap spacing contains "
				"nonzero data\n");
		}
		rawptr += struct_spacing;
		target++;
	}

	return 0;
}

struct razer_button_function * razer_get_buttonfunction_by_id(
		struct razer_button_function *functions, size_t nr_functions,
		uint8_t logical_id)
{
	struct razer_button_function *func = NULL;
	size_t i;

	for (i = 0; i < nr_functions; i++) {
		if (functions[i].id == logical_id) {
			func = &functions[i];
			break;
		}
	}

	return func;
}

struct razer_button_function * razer_get_buttonfunction_by_button(
		struct razer_buttonmapping *mappings, size_t nr_mappings,
		struct razer_button_function *functions, size_t nr_functions,
		const struct razer_button *button)
{
	struct razer_buttonmapping *mapping;

	mapping = razer_get_buttonmapping_by_physid(mappings, nr_mappings,
						    button->id);
	if (!mapping)
		return NULL;

	return razer_get_buttonfunction_by_id(functions, nr_functions,
					      mapping->logical);
}

struct razer_buttonmapping * razer_get_buttonmapping_by_physid(
		struct razer_buttonmapping *mappings, size_t nr_mappings,
		uint8_t physical_id)
{
	struct razer_buttonmapping *mapping = NULL;
	size_t i;

	for (i = 0; i < nr_mappings; i++) {
		if (mappings[i].physical == physical_id) {
			mapping = &mappings[i];
			break;
		}
	}

	return mapping;
}
