#ifndef RAZER_UTIL_H_
#define RAZER_UTIL_H_

#include "librazer.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <byteswap.h>
#include <stdio.h>
#include <stdbool.h>


#undef min
#undef max
#undef offsetof
#define offsetof(type, member)	((size_t)&((type *)0)->member)
#define min(x, y)		({ __typeof__(x) __x = (x); \
				   __typeof__(y) __y = (y); \
				   __x < __y ? __x : __y; })
#define max(x, y)		({ __typeof__(x) __x = (x); \
				   __typeof__(y) __y = (y); \
				   __x > __y ? __x : __y; })

#define BUILD_BUG_ON(x)		((void)sizeof(char[1 - 2 * !!(x)]))
#define ARRAY_SIZE(array)	(sizeof(array) / sizeof((array)[0]))

#define _packed			__attribute__((__packed__))

typedef uint16_t	be16_t;
typedef uint32_t	be32_t;
typedef uint16_t	le16_t;
typedef uint32_t	le32_t;

#ifdef BIG_ENDIAN_HOST
# define RAZER_HOST_BE	1
#else
# define RAZER_HOST_BE	0
#endif

static inline uint16_t cond_swap16(uint16_t v, bool to_from_bigendian)
{
	if ((unsigned int)to_from_bigendian ^ RAZER_HOST_BE)
		return bswap_16(v);
	return v;
}

static inline uint32_t cond_swap32(uint32_t v, bool to_from_bigendian)
{
	if ((unsigned int)to_from_bigendian ^ RAZER_HOST_BE)
		return bswap_32(v);
	return v;
}

static inline be16_t cpu_to_be16(uint16_t v) { return (be16_t)cond_swap16(v, 1);   }
static inline uint16_t be16_to_cpu(be16_t v) { return cond_swap16((uint16_t)v, 1); }
static inline be32_t cpu_to_be32(uint32_t v) { return (be32_t)cond_swap32(v, 1);   }
static inline uint32_t be32_to_cpu(be32_t v) { return cond_swap32((uint32_t)v, 1); }
static inline le16_t cpu_to_le16(uint16_t v) { return (le16_t)cond_swap16(v, 0);   }
static inline uint16_t le16_to_cpu(le16_t v) { return cond_swap16((uint16_t)v, 0); }
static inline le32_t cpu_to_le32(uint32_t v) { return (le32_t)cond_swap32(v, 0);   }
static inline uint32_t le32_to_cpu(le32_t v) { return cond_swap32((uint32_t)v, 0); }

static inline void * zalloc(size_t size)
{
	return calloc(1, size);
}

void razer_free(void *ptr, size_t size);

char * razer_strsplit(char *str, char sep);
int razer_split_tuple(const char *str, char sep, size_t elems_max_len, ...);
int razer_string_to_int(const char *string, int *i);
int razer_string_to_bool(const char *string, bool *b);
int razer_string_to_mode(const char *string, enum razer_led_mode *mode);
int razer_string_to_color(const char *string, struct razer_rgb_color *color);
char * razer_string_strip(char *str);

void razer_timeval_add_msec(struct timeval *tv, unsigned int msec);
bool razer_timeval_after(const struct timeval *a, const struct timeval *b);
int razer_timeval_msec_diff(const struct timeval *a, const struct timeval *b);

le16_t razer_xor16_checksum(const void *_buffer, size_t size);
be16_t razer_xor16_checksum_be(const void *_buffer, size_t size);
uint8_t razer_xor8_checksum(const void *_buffer, size_t size);

void razer_dump(const char *prefix, const void *_buf, size_t size);

static inline bool razer_buffer_is_all_zero(const void *_buf, size_t size)
{
	const uint8_t *buf = _buf;
	uint8_t value = 0;
	size_t i;
	for (i = 0; i < size; i++)
		value |= buf[i];
	return value == 0;
}

#endif /* RAZER_UTIL_H_ */
