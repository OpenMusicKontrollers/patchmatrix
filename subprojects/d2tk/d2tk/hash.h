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

#ifndef _D2TK_HASH_H
#define _D2TK_HASH_H

#include <stdint.h>
#include <unistd.h>

#include <d2tk/d2tk.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _d2tk_hash_dict_t d2tk_hash_dict_t;

struct _d2tk_hash_dict_t {
	const void *key;
	size_t len;
};

D2TK_API uint64_t
d2tk_hash(const void *data, size_t nbytes);

D2TK_API uint64_t
d2tk_hash_foreach(const void *data, size_t nbytes, ...) __attribute__((sentinel));

D2TK_API uint64_t
d2tk_hash_dict(const d2tk_hash_dict_t *dict);

#ifdef __cplusplus
}
#endif

#endif // _D2TK_HASH_H
