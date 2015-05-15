/*
 *   Copyright (C) 2007-2010 Michael Buesch <m@bues.ch>
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
#include <stdarg.h>


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

int razer_split_tuple(const char *str, char sep,
		      size_t elems_max_len, ...)
{
	char *elem;
	va_list ap;
	int err = 0;

	if (!elems_max_len)
		return -EINVAL;
	if (strlen(str) >= elems_max_len)
		return -EINVAL;

	va_start(ap, elems_max_len);
	while (1) {
		elem = va_arg(ap, char *);
		if (!elem)
			break;
		elem[0] = '\0';
		if (!str) {
			err = -ENODATA;
			continue;
		}
		razer_strlcpy(elem, str, elems_max_len);
		str = razer_strsplit(elem, sep);
	}
	va_end(ap);

	return err;
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

int razer_string_to_mode(const char *string, enum razer_led_mode *mode)
{
	if (strcasecmp(string, "static") == 0) {
		*mode = RAZER_LED_MODE_STATIC;
		return 0;
	}
	if (strcasecmp(string, "spectrum") == 0) {
		*mode = RAZER_LED_MODE_SPECTRUM;
		return 0;
	}
	if (strcasecmp(string, "breathing") == 0) {
		*mode = RAZER_LED_MODE_BREATHING;
		return 0;
	}

	return -EINVAL;
}

int razer_string_to_color(const char *string, struct razer_rgb_color *color)
{
	uint32_t temp = (uint32_t)strtol(string, NULL, 16);

	color->r = (uint8_t)((temp >> 16) & 0xFF);
	color->g = (uint8_t)((temp >> 8) & 0xFF);
	color->b = (uint8_t)(temp & 0xFF);
	color->valid = 1;

	return 0;
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

void razer_strlcpy(char *dst, const char *src, size_t dst_size)
{
	size_t len;

	if (!dst_size)
		return;

	len = strlen(src);
	if (len >= dst_size)
		len = dst_size - 1;
	memcpy(dst, src, len);
	dst[len] = 0;
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

/* Return a-b in milliseconds */
int razer_timeval_msec_diff(const struct timeval *a, const struct timeval *b)
{
	int64_t usec_a, usec_b, usec_diff;

	usec_a = (int64_t)a->tv_sec * 1000000;
	usec_a += (int64_t)a->tv_usec;

	usec_b = (int64_t)b->tv_sec * 1000000;
	usec_b += (int64_t)b->tv_usec;

	usec_diff = usec_a - usec_b;

	return usec_diff / 1000;
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

be16_t razer_xor16_checksum_be(const void *_buffer, size_t size)
{
	return (be16_t)bswap_16((uint16_t)razer_xor16_checksum(_buffer, size));
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

static char razer_char_to_ascii(char c)
{
	if (c >= 32 && c <= 126)
		return c;
	return '.';
}

void razer_dump(const char *prefix, const void *_buf, size_t size)
{
	const unsigned char *buf = _buf;
	size_t i;
	char ascii[17] = { 0, };
	unsigned int ascii_idx = 0;

	for (i = 0; i < size; i++) {
		if (i % 16 == 0) {
			if (i != 0) {
				printf("  |%s|\n", ascii);
				memset(ascii, 0, sizeof(ascii));
				ascii_idx = 0;
			}
			printf("%s-[%04X]:  ", prefix, (unsigned int)i);
		}
		printf("%02X%s", buf[i], (i % 2) ? " " : "");
		ascii[ascii_idx++] = razer_char_to_ascii(buf[i]);
	}
	if (ascii[0]) {
		for (; i % 16; i++)
			printf((i % 2) ? "   " : "  ");
		printf("  |%s|", ascii);
	}
	printf("\n\n");
}

void razer_ascii_to_utf16(razer_utf16_t *dest, size_t dest_max_chars,
			  const char *src)
{
	size_t count = 0;

	if (!dest_max_chars)
		return;
	/* FIXME: This code is wrong.
	 * But it works for most strings in the current setup.
	 * So it probably won't blow up too often.
	 */
	while (count < dest_max_chars - 1) {
		if (!*src)
			break;
		*dest++ = *src++;
		count++;
	}
	*dest = 0;
}

int razer_utf16_cpy(razer_utf16_t *dest, const razer_utf16_t *src,
		    size_t max_chars)
{
	size_t i;

	for (i = 0; i < max_chars; i++, dest++, src++) {
		*dest = *src;
		if (!(*src))
			return 0;
	}

	return -ENOSPC;
}

size_t razer_utf16_strlen(const razer_utf16_t *str)
{
	size_t count = 0;

	while (*str) {
		str++;
		count++;
	}

	return count;
}
