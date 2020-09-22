/*
 * Copyright (c) 2018-2019 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#include <stdarg.h>
#include <stdio.h>

#include <d2tk/hash.h>

#include "mum.h"

#define SEED 12345

typedef union _d2tk_hash_item_t {
	uint64_t u64;
	uint8_t u8 [8];
} d2tk_hash_item_t;

__attribute__((always_inline))
static inline uint64_t
_d2tk_hash(const void *key, size_t len, uint64_t hash)
{
	if(len <= sizeof(uint64_t))
	{
		d2tk_hash_item_t item = { .u64 = 0 };
		memcpy(item.u8, key, len);

		return mum_hash_step(hash, item.u64);
	}

	return mum_hash(key, len, hash);
}

D2TK_API uint64_t
d2tk_hash(const void *key, size_t len)
{
	if(len <= sizeof(uint64_t))
	{
		d2tk_hash_item_t item = { .u64 = 0 };
		memcpy(item.u8, key, len);

		return mum_hash64(item.u64, SEED);
	}

	return mum_hash(key, len, SEED);
}

D2TK_API uint64_t
d2tk_hash_foreach(const void *key, size_t len, ...)
{
	va_list args;
	uint64_t hash = mum_hash_init(SEED);

	hash = _d2tk_hash(key, len, hash);

	va_start(args, len);

	while( (key = va_arg(args, const void *)) )
	{
		hash = _d2tk_hash(key, va_arg(args, size_t), hash);
	}

	va_end(args);

  return mum_hash_finish(hash);
}

D2TK_API uint64_t
d2tk_hash_dict(const d2tk_hash_dict_t *dict)
{
	uint64_t hash = mum_hash_init(SEED);

	for( ; dict->key; dict++)
	{
		hash = _d2tk_hash(dict->key, dict->len, hash);
	}

	return mum_hash_finish(hash);
}
