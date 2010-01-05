/*
 $Id: value.c,v 1.5 2007-04-11 10:43:27+01 taviso Exp taviso $

 simple routines for working with the value_t data structure.

 Copyright (C) 2009,2010      Tavis Ormandy, Eli Dupree, WANG Lu  <taviso@sdf.lonestar.org, elidupree@charter.net, coolwanglu@gmail.com>
 Copyright (C) 2006,2007,2009 Tavis Ormandy <taviso@sdf.lonestar.org>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include "value.h"
#include "scanroutines.h"
#include "scanmem.h"

bool valtostr(const value_t * val, char *str, size_t n)
{
    char buf[50];
    int max_bytes = 0;
    bool print_as_unsigned = false;

#define FLAG_MACRO(bytes, string) (val->flags.u##bytes##b && val->flags.s##bytes##b) ? (string " ") : (val->flags.u##bytes##b) ? (string "u ") : (val->flags.s##bytes##b) ? (string "s ") : ""
    
    /* set the flags */
    snprintf(buf, sizeof(buf), "[%s%s%s%s%s%s%s]",
             FLAG_MACRO(64, "I64"),
             FLAG_MACRO(32, "I32"),
             FLAG_MACRO(16, "I16"),
             FLAG_MACRO(8,  "I8"),
             val->flags.f64b ? "F64 " : "",
             val->flags.f32b ? "F32 " : "",
             (val->flags.ineq_reverse && !val->flags.ineq_forwards) ? "(reversed inequality) " : "");

         if (val->flags.u64b) { max_bytes = 8; print_as_unsigned =  true; }
    else if (val->flags.s64b) { max_bytes = 8; print_as_unsigned = false; }
    else if (val->flags.u32b) { max_bytes = 4; print_as_unsigned =  true; }
    else if (val->flags.s32b) { max_bytes = 4; print_as_unsigned = false; }
    else if (val->flags.u16b) { max_bytes = 2; print_as_unsigned =  true; }
    else if (val->flags.s16b) { max_bytes = 2; print_as_unsigned = false; }
    else if (val->flags.u8b ) { max_bytes = 1; print_as_unsigned =  true; }
    else if (val->flags.s8b ) { max_bytes = 1; print_as_unsigned = false; }
    
    /* find the right format, considering different integer size implementations */
         if (max_bytes == sizeof(long long)) snprintf(str, n, print_as_unsigned ? "%llu, %s" : "%lld, %s", print_as_unsigned ? get_ulonglong(val) : get_slonglong(val), buf);
    else if (max_bytes == sizeof(long))      snprintf(str, n, print_as_unsigned ? "%lu, %s"  : "%ld, %s" , print_as_unsigned ? get_ulong(val) : get_slong(val), buf);
    else if (max_bytes == sizeof(int))       snprintf(str, n, print_as_unsigned ? "%u, %s"   : "%d, %s"  , print_as_unsigned ? get_uint(val) : get_sint(val), buf);
    else if (max_bytes == sizeof(short))     snprintf(str, n, print_as_unsigned ? "%hu, %s"  : "%hd, %s" , print_as_unsigned ? get_ushort(val) : get_sshort(val), buf);
    else if (max_bytes == sizeof(char))      snprintf(str, n, print_as_unsigned ? "%hhu, %s" : "%hhd, %s", print_as_unsigned ? get_uchar(val) : get_schar(val), buf);
    else if (val->flags.f64b) snprintf(str, n, "%lf, %s", get_f64b(val), buf);
    else if (val->flags.f32b) snprintf(str, n, "%f, %s", get_f32b(val), buf);
    else {
        snprintf(str, n, "%#llx?, %s", get_slonglong(val), buf);
        return false;
    }
    return true;
}

void valcpy(value_t * dst, const value_t * src)
{
    memcpy(dst, src, sizeof(value_t));
    return;
}

void truncval_to_flags(value_t * dst, match_flags flags)
{
    assert(dst != NULL);
    
    dst->flags.u64b &= flags.u64b;
    dst->flags.s64b &= flags.s64b;
    dst->flags.f64b &= flags.f64b;
    dst->flags.u32b &= flags.u32b;
    dst->flags.s32b &= flags.s32b;
    dst->flags.f32b &= flags.f32b;
    dst->flags.u16b &= flags.u16b;
    dst->flags.s16b &= flags.s16b;
    dst->flags.u8b  &= flags.u8b ;
    dst->flags.s8b  &= flags.s8b ;
    
    /* Hack - simply overwrite the inequality flags (this should have no effect except to make them display properly) */
    dst->flags.ineq_forwards = flags.ineq_forwards;
    dst->flags.ineq_reverse  = flags.ineq_reverse ;
}

void truncval(value_t * dst, const value_t * src)
{
    assert(src != NULL);

    truncval_to_flags(dst, src->flags);
}

/* set all possible width flags, if nothing is known about val */
void valnowidth(value_t * val)
{
    assert(val);

    val->flags.u64b = 1;
    val->flags.s64b = 1;
    val->flags.u32b = 1;
    val->flags.s32b = 1;
    val->flags.u16b = 1;
    val->flags.s16b = 1;
    val->flags.u8b  = 1;
    val->flags.s8b  = 1;
    val->flags.f64b = 1;
    val->flags.f32b = 1;

    val->flags.ineq_forwards = 1;
    val->flags.ineq_reverse = 1;
    return;
}

void strtoval(const char *nptr, char **endptr, int base, value_t * val)
{
    int64_t num;

    assert(nptr != NULL);
    assert(val != NULL);

    memset(val, 0x00, sizeof(value_t));

    /* skip past any whitespace */
    while (isspace(*nptr))
        nptr++;

    /* now parse it using strtoul */
    num = strtoll(nptr, endptr, base);

    /* determine correct flags */
    if (num >=       (0) && num < (1LL<< 8)) val->flags.u8b  = 1;
    if (num > -(1LL<< 7) && num < (1LL<< 7)) val->flags.s8b  = 1;
    if (num >=       (0) && num < (1LL<<16)) val->flags.u16b = 1;
    if (num > -(1LL<<15) && num < (1LL<<15)) val->flags.s16b = 1;
    if (num >=       (0) && num < (1LL<<32)) val->flags.u32b = 1;
    if (num > -(1LL<<31) && num < (1LL<<31)) val->flags.s32b = 1;
    if (num >=       (0) &&          (true)) val->flags.u64b = 1; // what if num >= 1<<32 ?
    if (          (true) &&          (true)) val->flags.s64b = 1;

    /* finally insert the number */
    val->int_value = num;

    /* TODO: (UGLY) currently, assume users always provide an integer value */
    val->flags.f32b = val->flags.f64b = 1;
    val->float_value = (float) num;
    val->double_value = (double) num;   

    /* values from strings are always done by bit math */
    val->how_to_calculate_values = BY_BIT_MATH;

    return;

}

int flags_to_max_width_in_bytes(match_flags flags)
{
	     if (flags.u64b || flags.s64b || flags.f64b) return 8;
	else if (flags.u32b || flags.s32b || flags.f32b) return 4;
	else if (flags.u16b || flags.s16b              ) return 2;
	else if (flags.u8b  || flags.s8b               ) return 1;
	else    /* It can't be a variable of any size */ return 0;
}

int val_max_width_in_bytes(value_t *val)
{
	return flags_to_max_width_in_bytes(val->flags);
}

#define DEFINE_GET_SET_FUNCTIONS(inttype, signedness_letter, bits) \
inttype get_##signedness_letter##bits##b(value_t const* val) \
{ \
	switch (val->how_to_calculate_values) \
	{ \
		case BY_POINTER_SHIFTING: return *((inttype *)&val->int_value); \
		case BY_BIT_MATH:         return (val->int_value & ((1LL << bits) - 1)); \
		default: assert(false); \
	} \
} \
 \
void set_##signedness_letter##bits##b(value_t *val, inttype data) \
{ \
	switch (val->how_to_calculate_values) \
	{ \
		case BY_POINTER_SHIFTING: \
			*((inttype *)&val->int_value) = data; \
			break; \
		case BY_BIT_MATH: \
			val->int_value = (val->int_value & ~((1LL << bits) - 1)) + data; \
			break; \
		default: assert(false); \
	} \
}

DEFINE_GET_SET_FUNCTIONS(uint8_t, u, 8)
DEFINE_GET_SET_FUNCTIONS(int8_t, s, 8)
DEFINE_GET_SET_FUNCTIONS(uint16_t, u, 16)
DEFINE_GET_SET_FUNCTIONS(int16_t, s, 16)
DEFINE_GET_SET_FUNCTIONS(uint32_t, u, 32)
DEFINE_GET_SET_FUNCTIONS(int32_t, s, 32)

int64_t get_s64b(value_t const* val) { return val->int_value; }
void set_s64b(value_t *val, int64_t data) { val->int_value = data; }

uint64_t get_u64b(value_t const* val)
{
	switch (val->how_to_calculate_values)
	{
		case BY_POINTER_SHIFTING:
			return *((uint64_t *)&val->int_value);
			break;
		case BY_BIT_MATH:
			return (uint64_t)val->int_value;
			break;
		default: assert(false);
	}
}
void set_u64b(value_t *val, uint64_t data)
{
	switch (val->how_to_calculate_values)
	{
		case BY_POINTER_SHIFTING:
			val->int_value = *((uint64_t *)&data);
			break;
		case BY_BIT_MATH:
			val->int_value = (uint64_t)data;
			break;
		default: assert(false);
	}
}

float get_f32b(value_t const* val) { return val->float_value; }
void set_f32b(value_t *val, float data) { val->float_value = data; }

double get_f64b(value_t const* val) { return val->double_value; }
void set_f64b(value_t *val, double data) { val->double_value = data; }

#define DEFINE_GET_BY_SYSTEM_DEPENDENT_TYPE_FUNCTION(type, typename, signedness_letter) \
type get_##signedness_letter##typename (value_t const* val) \
{ \
	     if (sizeof(type) <= 1) return (type)get_##signedness_letter##8b(val); \
	else if (sizeof(type) <= 2) return (type)get_##signedness_letter##16b(val); \
	else if (sizeof(type) <= 4) return (type)get_##signedness_letter##32b(val); \
	else if (sizeof(type) <= 8) return (type)get_##signedness_letter##64b(val); \
	else assert(false); \
}
#define DEFINE_GET_BY_SYSTEM_DEPENDENT_TYPE_FUNCTIONS(type, typename) \
	DEFINE_GET_BY_SYSTEM_DEPENDENT_TYPE_FUNCTION(unsigned type, typename, u) \
	DEFINE_GET_BY_SYSTEM_DEPENDENT_TYPE_FUNCTION(signed type, typename, s)

DEFINE_GET_BY_SYSTEM_DEPENDENT_TYPE_FUNCTIONS(char, char)
DEFINE_GET_BY_SYSTEM_DEPENDENT_TYPE_FUNCTIONS(short, short)
DEFINE_GET_BY_SYSTEM_DEPENDENT_TYPE_FUNCTIONS(int, int)
DEFINE_GET_BY_SYSTEM_DEPENDENT_TYPE_FUNCTIONS(long, long)
DEFINE_GET_BY_SYSTEM_DEPENDENT_TYPE_FUNCTIONS(long long, longlong)
