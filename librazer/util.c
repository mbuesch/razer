/*
 *   Copyright (C) 2007-2010 Michael Buesch <mb@bu3sch.de>
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

#include "util.h"
#include "razer_private.h"

#include <unistd.h>
#include <errno.h>
#include <ctype.h>


void razer_free(void *ptr, size_t size)
{
	if (ptr) {
		memset(ptr, 0, size);
		free(ptr);
	}
}

char * razer_strsplit(char *str, char sep)
{
	char c;

	if (!str)
		return NULL;
	for (c = *str; c != '\0' && c != sep; c = *str)
		str++;
	if (c == sep) {
		*str = '\0';
		return str + 1;
	}

	return NULL;
}

int razer_split_pair(const char *str, char sep, char *a, char *b, size_t len)
{
	char *tmp;

	if (strlen(str) >= len)
		return -EINVAL;
	strcpy(a, str);
	tmp = razer_strsplit(a, sep);
	if (!tmp)
		return -EINVAL;
	strcpy(b, tmp);

	return 0;
}

int razer_string_to_int(const char *string, int *i)
{
	char *tailptr;
	long retval;

	retval = strtol(string, &tailptr, 0);
	if (tailptr == string || tailptr[0] != '\0')
		return -EINVAL;
	*i = retval;

	return 0;
}

int razer_string_to_bool(const char *string, bool *b)
{
	int i;

	if (strcasecmp(string, "yes") == 0 ||
	    strcasecmp(string, "true") == 0 ||
	    strcasecmp(string, "on") == 0) {
		*b = 1;
		return 0;
	}
	if (strcasecmp(string, "no") == 0 ||
	    strcasecmp(string, "false") == 0 ||
	    strcasecmp(string, "off") == 0) {
		*b = 0;
		return 0;
	}
	if (!razer_string_to_int(string, &i)) {
		*b = !!i;
		return 0;
	}

	return -EINVAL;
}

char * razer_string_strip(char *str)
{
	char *start = str;
	size_t len;

	if (!str)
		return NULL;
	while (*start != '\0' && isspace(*start))
		start++;
	len = strlen(start);
	while (len && isspace(start[len - 1])) {
		start[len - 1] = '\0';
		len--;
	}

	return start;
}

void razer_timeval_add_msec(struct timeval *tv, unsigned int msec)
{
	unsigned int seconds, usec;

	seconds = msec / 1000;
	msec = msec % 1000;
	usec = msec * 1000;

	tv->tv_usec += usec;
	while (tv->tv_usec >= 1000000) {
		tv->tv_sec++;
		tv->tv_usec -= 1000000;
	}
	tv->tv_sec += seconds;
}

/* Returns true, if a is after b. */
bool razer_timeval_after(const struct timeval *a, const struct timeval *b)
{
	if (a->tv_sec > b->tv_sec)
		return 1;
	if ((a->tv_sec == b->tv_sec) && (a->tv_usec > b->tv_usec))
		return 1;
	return 0;
}

void razer_msleep(unsigned int msecs)
{
	int err;
	struct timespec time;

	time.tv_sec = 0;
	while (msecs >= 1000) {
		time.tv_sec++;
		msecs -= 1000;
	}
	time.tv_nsec = msecs;
	time.tv_nsec *= 1000000;
	do {
		err = nanosleep(&time, &time);
	} while (err && errno == EINTR);
	if (err) {
		razer_error("nanosleep() failed with: %s\n",
			strerror(errno));
	}
}

le16_t razer_xor16_checksum(const void *_buffer, size_t size)
{
	const uint8_t *buffer = _buffer;
	uint16_t sum = 0;
	size_t i;

	for (i = 0; i < size; i += 2) {
		sum ^= buffer[i];
		if (i < size - 1)
			sum ^= ((uint16_t)(buffer[i + 1])) << 8;
	}

	return cpu_to_le16(sum);
}

uint8_t razer_xor8_checksum(const void *_buffer, size_t size)
{
	const uint8_t *buffer = _buffer;
	uint8_t sum = 0;
	size_t i;

	for (i = 0; i < size; i++)
		sum ^= buffer[i];

	return sum;
}

void razer_dump(const char *prefix, const void *_buf, size_t size)
{
	const unsigned char *buf = _buf;
	size_t i;

	for (i = 0; i < size; i++) {
		if (i % 16 == 0) {
			if (i != 0)
				printf("\n");
			printf("%s-[%04X]:  ", prefix, (unsigned int)i);
		}
		printf("%02X%s", buf[i], (i % 2) ? " " : "");
	}
	printf("\n\n");
}
