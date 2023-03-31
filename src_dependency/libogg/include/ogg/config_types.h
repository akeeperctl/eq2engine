#ifndef __CONFIG_TYPES_H__
#define __CONFIG_TYPES_H__

/* these are filled in by configure or cmake*/
#define INCLUDE_INTTYPES_H 0
#define INCLUDE_STDINT_H 0
#define INCLUDE_SYS_TYPES_H 0

#if INCLUDE_INTTYPES_H
#  include <inttypes.h>
#endif
#if INCLUDE_STDINT_H
#  include <stdint.h>
#endif
#if INCLUDE_SYS_TYPES_H
#  include <sys/types.h>
#endif

typedef short ogg_int16_t;
typedef unsigned ogg_uint16_t;
typedef int ogg_int32_t;
typedef unsigned int ogg_uint32_t;
typedef long long ogg_int64_t;
typedef unsigned long long ogg_uint64_t;

#endif