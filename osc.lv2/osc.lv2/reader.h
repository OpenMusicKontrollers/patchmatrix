/*
 * Copyright (c) 2015-2016 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#ifndef LV2_OSC_READER_H
#define LV2_OSC_READER_H

#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include <osc.lv2/osc.h>
#include <osc.lv2/endian.h>
#include <osc.lv2/util.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _LV2_OSC_Tree LV2_OSC_Tree;
typedef struct _LV2_OSC_Reader LV2_OSC_Reader;
typedef struct _LV2_OSC_Item LV2_OSC_Item;
typedef struct _LV2_OSC_Arg LV2_OSC_Arg;
typedef void (*LV2_OSC_Branch)(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg,
	const LV2_OSC_Tree *tree, void *data);

struct _LV2_OSC_Tree {
	const char *name;
	const LV2_OSC_Tree *trees;
	LV2_OSC_Branch branch;
};

struct _LV2_OSC_Reader {
	const uint8_t *buf;
	const uint8_t *ptr;
	const uint8_t *end;
};

struct _LV2_OSC_Item {
	int32_t size;
	const uint8_t *body;

	uint64_t timetag;
	const uint8_t *end;
};

struct _LV2_OSC_Arg {
	const char *type;
	int32_t size;
	union {
		int32_t i;
		float f;
		const char *s;
		const uint8_t *b;

		int64_t h;
		double d;
		uint64_t t;

		const uint8_t *m;
		const char *S;
		char c;
		struct {
			uint8_t R;
			uint8_t G;
			uint8_t B;
			uint8_t A;
		}; // anonymous RGBA struct
	};

	const char *path;
	const uint8_t *end;
};

static inline void
lv2_osc_reader_initialize(LV2_OSC_Reader *reader, const uint8_t *buf, size_t size)
{
	reader->buf = buf;
	reader->ptr = buf;
	reader->end = buf + size;
}

static inline bool
lv2_osc_reader_overflow(LV2_OSC_Reader *reader, size_t size)
{
	return reader->ptr + size > reader->end;
}

static inline bool
lv2_osc_reader_be32toh(LV2_OSC_Reader *reader, union swap32_t *s32)
{
	if(lv2_osc_reader_overflow(reader, 4))
		return false;

	s32->u = *(const uint32_t *)reader->ptr;
	s32->u = be32toh(s32->u);
	reader->ptr += 4;

	return true;
}

static inline bool
lv2_osc_reader_be64toh(LV2_OSC_Reader *reader, union swap64_t *s64)
{
	if(lv2_osc_reader_overflow(reader, 8))
		return false;

	s64->u = *(const uint64_t *)reader->ptr;
	s64->u = be64toh(s64->u);
	reader->ptr += 8;

	return true;
}

static inline bool
lv2_osc_reader_get_int32(LV2_OSC_Reader *reader, int32_t *i)
{
	union swap32_t s32;
	if(!lv2_osc_reader_be32toh(reader, &s32))
		return false;

	*i = s32.i;

	return true;
}

static inline bool
lv2_osc_reader_get_float(LV2_OSC_Reader *reader, float *f)
{
	union swap32_t s32;
	if(!lv2_osc_reader_be32toh(reader, &s32))
		return false;

	*f = s32.f;

	return true;
}

static inline bool
lv2_osc_reader_get_int64(LV2_OSC_Reader *reader, int64_t *h)
{
	union swap64_t s64;
	if(!lv2_osc_reader_be64toh(reader, &s64))
		return false;

	*h = s64.h;

	return true;
}

static inline bool
lv2_osc_reader_get_timetag(LV2_OSC_Reader *reader, uint64_t *t)
{
	union swap64_t s64;
	if(!lv2_osc_reader_be64toh(reader, &s64))
		return false;

	*t = s64.u;

	return true;
}

static inline bool
lv2_osc_reader_get_double(LV2_OSC_Reader *reader, double *d)
{
	union swap64_t s64;
	if(!lv2_osc_reader_be64toh(reader, &s64))
		return false;

	*d = s64.d;

	return true;
}

static inline bool
lv2_osc_reader_get_string(LV2_OSC_Reader *reader, const char **s)
{
	const char *str = (const char *)reader->ptr;
	const size_t padded = LV2_OSC_PADDED_SIZE(strlen(str) + 1);
	if(lv2_osc_reader_overflow(reader, padded ))
		return false;

	*s = str;
	reader->ptr += padded;

	return true;
}

static inline bool
lv2_osc_reader_get_symbol(LV2_OSC_Reader *reader, const char **S)
{
	return lv2_osc_reader_get_string(reader, S);
}

static inline bool
lv2_osc_reader_get_midi(LV2_OSC_Reader *reader, const uint8_t **m)
{
	if(lv2_osc_reader_overflow(reader, 4))
		return false;

	*m = reader->ptr;
	reader->ptr += 4;

	return true;
}

static inline bool
lv2_osc_reader_get_blob(LV2_OSC_Reader *reader, int32_t *len, const uint8_t **body)
{
	if(!lv2_osc_reader_get_int32(reader, len))
		return false;

	const size_t padded = LV2_OSC_PADDED_SIZE(*len);
	if(lv2_osc_reader_overflow(reader, padded))
		return false;

	*body = reader->ptr;
	reader->ptr += padded;

	return true;
}

static inline bool
lv2_osc_reader_get_rgba(LV2_OSC_Reader *reader, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	if(lv2_osc_reader_overflow(reader, 4))
		return false;

	*r = reader->ptr[0];
	*g = reader->ptr[1];
	*b = reader->ptr[2];
	*a = reader->ptr[3];
	reader->ptr += 4;

	return true;
}

static inline bool
lv2_osc_reader_get_char(LV2_OSC_Reader *reader, char *c)
{
	int32_t i;
	if(!lv2_osc_reader_get_int32(reader, &i))
		return false;

	*c = i;

	return true;
}

static inline LV2_OSC_Item *
lv2_osc_reader_item_raw(LV2_OSC_Reader *reader, LV2_OSC_Item *itm)
{
	if(!lv2_osc_reader_get_int32(reader, &itm->size))
		return NULL;

	if(lv2_osc_reader_overflow(reader, itm->size))
		return NULL;

	itm->body = reader->ptr;

	return itm;
}

static inline LV2_OSC_Item *
lv2_osc_reader_item_begin(LV2_OSC_Reader *reader, LV2_OSC_Item *itm, size_t len)
{
	if(lv2_osc_reader_overflow(reader, len))
		return NULL;

	itm->end = reader->ptr + len;

	if(lv2_osc_reader_overflow(reader, 16))
		return NULL;

	if(strncmp((const char *)reader->ptr, "#bundle", 8))
		return NULL;
	reader->ptr += 8;

	if(!lv2_osc_reader_get_timetag(reader, &itm->timetag))
		return NULL;

	return lv2_osc_reader_item_raw(reader, itm);
}

static inline bool
lv2_osc_reader_item_is_end(LV2_OSC_Reader *reader, LV2_OSC_Item *itm)
{
	return reader->ptr > itm->end;
}

static inline LV2_OSC_Item *
lv2_osc_reader_item_next(LV2_OSC_Reader *reader, LV2_OSC_Item *itm)
{
	reader->ptr += itm->size;

	return lv2_osc_reader_item_raw(reader, itm);
}

#define OSC_READER_BUNDLE_BEGIN(reader, len) \
	lv2_osc_reader_item_begin( \
		(reader), \
		&(LV2_OSC_Item){ .size = 0, .body = NULL, .timetag = 1ULL, .end = NULL }, \
		len)

#define OSC_READER_BUNDLE_ITERATE(reader, itm) \
	for(itm = itm; \
		itm && !lv2_osc_reader_item_is_end((reader), (itm)); \
		itm = lv2_osc_reader_item_next((reader), (itm)))

#define OSC_READER_BUNDLE_FOREACH(reader, itm, len) \
	for(LV2_OSC_Item *(itm) = OSC_READER_BUNDLE_BEGIN((reader), (len)); \
		itm && !lv2_osc_reader_item_is_end((reader), (itm)); \
		itm = lv2_osc_reader_item_next((reader), (itm)))

static inline LV2_OSC_Arg *
lv2_osc_reader_arg_raw(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg)
{
	switch( (LV2_OSC_Type)*arg->type)
	{
		case LV2_OSC_INT32:
		{
			if(!lv2_osc_reader_get_int32(reader, &arg->i))
				return NULL;
			arg->size = 4;

			break;
		}
		case LV2_OSC_FLOAT:
		{
			if(!lv2_osc_reader_get_float(reader, &arg->f))
				return NULL;
			arg->size = 4;

			break;
		}
		case LV2_OSC_STRING:
		{
			if(!lv2_osc_reader_get_string(reader, &arg->s))
				return NULL;
			arg->size = strlen(arg->s) + 1;

			break;
		}
		case LV2_OSC_BLOB:
		{
			if(!lv2_osc_reader_get_blob(reader, &arg->size, &arg->b))
				return NULL;
			//arg->size = arg->size;

			break;
		}

		case LV2_OSC_TRUE:
		case LV2_OSC_FALSE:
		case LV2_OSC_NIL:
		case LV2_OSC_IMPULSE:
			break;

		case LV2_OSC_INT64:
		{
			if(!lv2_osc_reader_get_int64(reader, &arg->h))
				return NULL;
			arg->size = 8;

			break;
		}
		case LV2_OSC_DOUBLE:
		{
			if(!lv2_osc_reader_get_double(reader, &arg->d))
				return NULL;
			arg->size = 8;

			break;
		}
		case LV2_OSC_TIMETAG:
		{
			if(!lv2_osc_reader_get_timetag(reader, &arg->t))
				return NULL;
			arg->size = 8;

			break;
		}

		case LV2_OSC_MIDI:
		{
			if(!lv2_osc_reader_get_midi(reader, &arg->m))
				return NULL;
			arg->size = 4;

			break;
		}
		case LV2_OSC_SYMBOL:
		{
			if(!lv2_osc_reader_get_symbol(reader, &arg->S))
				return NULL;
			arg->size = strlen(arg->S) + 1;

			break;
		}
		case LV2_OSC_CHAR:
		{
			if(!lv2_osc_reader_get_char(reader, &arg->c))
				return NULL;
			arg->size = 4;

			break;
		}
		case LV2_OSC_RGBA:
		{
			if(!lv2_osc_reader_get_rgba(reader, &arg->R, &arg->G, &arg->B, &arg->A))
				return NULL;
			arg->size = 4;

			break;
		}
	}

	return arg;
}

static inline LV2_OSC_Arg *
lv2_osc_reader_arg_begin(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg, size_t len)
{
	if(lv2_osc_reader_overflow(reader, len))
		return NULL;

	arg->end = reader->ptr + len;

	if(!lv2_osc_reader_get_string(reader, &arg->path)) //TODO check for validity
		return NULL;

	if(!lv2_osc_reader_get_string(reader, &arg->type)) //TODO check for validity
		return NULL;

	if(*arg->type != ',')
		return NULL;

	arg->type++; // skip ','

	return lv2_osc_reader_arg_raw(reader, arg);
}

static inline bool
lv2_osc_reader_arg_is_end(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg)
{
	return (*arg->type == '\0') || (reader->ptr > arg->end);
}

static inline LV2_OSC_Arg *
lv2_osc_reader_arg_next(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg)
{
	arg->type++;

	return lv2_osc_reader_arg_raw(reader, arg);
}

#define OSC_READER_MESSAGE_BEGIN(reader, len) \
	lv2_osc_reader_arg_begin( \
		(reader), \
		&(LV2_OSC_Arg){ .type = NULL, .size = 0, .path = NULL, .end = NULL }, \
		len)

#define OSC_READER_MESSAGE_ITERATE(reader, arg) \
	for(arg = arg; \
		arg && !lv2_osc_reader_arg_is_end((reader), (arg)); \
		arg = lv2_osc_reader_arg_next((reader), (arg)))

#define OSC_READER_MESSAGE_FOREACH(reader, arg, len) \
	for(LV2_OSC_Arg *(arg) = OSC_READER_MESSAGE_BEGIN((reader), (len)); \
		arg && !lv2_osc_reader_arg_is_end((reader), (arg)); \
		arg = lv2_osc_reader_arg_next((reader), (arg)))

static inline bool
lv2_osc_reader_arg_varlist(LV2_OSC_Reader *reader, const char *fmt, va_list args)
{
	for(const char *type = fmt; *type; type++)
	{
		switch( (LV2_OSC_Type)*type)
		{
			case LV2_OSC_INT32:
				if(!lv2_osc_reader_get_int32(reader, va_arg(args, int32_t *)))
					return false;
				break;
			case LV2_OSC_FLOAT:
				if(!lv2_osc_reader_get_float(reader, va_arg(args, float *)))
					return false;
				break;
			case LV2_OSC_STRING:
				if(!lv2_osc_reader_get_string(reader, va_arg(args, const char **)))
					return false;
				break;
			case LV2_OSC_BLOB:
				if(!lv2_osc_reader_get_blob(reader, va_arg(args, int32_t *), va_arg(args, const uint8_t **)))
					return false;
				break;

			case LV2_OSC_TRUE:
			case LV2_OSC_FALSE:
			case LV2_OSC_NIL:
			case LV2_OSC_IMPULSE:
				break;

			case LV2_OSC_INT64:
				if(!lv2_osc_reader_get_int64(reader, va_arg(args, int64_t *)))
					return false;
				break;
			case LV2_OSC_DOUBLE:
				if(!lv2_osc_reader_get_double(reader, va_arg(args, double *)))
					return false;
				break;
			case LV2_OSC_TIMETAG:
				if(!lv2_osc_reader_get_timetag(reader, va_arg(args, uint64_t *)))
					return false;
				break;

			case LV2_OSC_MIDI:
				if(!lv2_osc_reader_get_midi(reader, va_arg(args, const uint8_t **)))
					return false;
				break;
			case LV2_OSC_SYMBOL:
				if(!lv2_osc_reader_get_symbol(reader, va_arg(args, const char **)))
					return false;
				break;
			case LV2_OSC_CHAR:
				if(!lv2_osc_reader_get_char(reader, va_arg(args, char *)))
					return false;
				break;
			case LV2_OSC_RGBA:
				if(!lv2_osc_reader_get_rgba(reader, va_arg(args, uint8_t *), va_arg(args, uint8_t *),
						va_arg(args, uint8_t *), va_arg(args, uint8_t *)))
					return false;
				break;
		}
	}

	return true;
}

static inline bool
lv2_osc_reader_arg_vararg(LV2_OSC_Reader *reader, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

	const bool res = lv2_osc_reader_arg_varlist(reader, fmt, args);

	va_end(args);

	return res;
}

static inline bool
lv2_osc_reader_is_bundle(LV2_OSC_Reader *reader)
{
	return strncmp((const char *)reader->ptr, "#bundle", 8) == 0;
}

static inline bool
lv2_osc_reader_is_message(LV2_OSC_Reader *reader)
{
	return reader->ptr[0] == '/'; //FIXME check path
}

static inline void
_lv2_osc_trees_internal(LV2_OSC_Reader *reader, const char *path, const char *from,
	LV2_OSC_Arg *arg, const LV2_OSC_Tree *trees, void *data)
{
	const char *ptr = strchr(from, '/');

	const size_t len = ptr
		? (size_t)(ptr - from)
		: strlen(from);

	for(const LV2_OSC_Tree *tree = trees; tree && tree->name; tree++)
	{
		if(lv2_osc_pattern_match(from, tree->name, len))
		{
			if(tree->trees && ptr)
			{
				if(tree->branch)
				{
					LV2_OSC_Reader reader_clone = *reader;
					tree->branch(&reader_clone, arg, tree, data);
				}

				_lv2_osc_trees_internal(reader, path, &ptr[1], arg, tree->trees, data);
			}
			else if(tree->branch && !ptr)
			{
				LV2_OSC_Reader reader_clone = *reader;
				tree->branch(&reader_clone, arg, tree, data);
			}
		}
	}
}

static inline void
lv2_osc_reader_match(LV2_OSC_Reader *reader, size_t len,
	const LV2_OSC_Tree *trees, void *data)
{
	LV2_OSC_Arg *arg = OSC_READER_MESSAGE_BEGIN(reader, len);
	const char *path = arg->path;
	const char *from = &path[1];

	_lv2_osc_trees_internal(reader, path, from, arg, trees, data);
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LV2_OSC_READER_H
