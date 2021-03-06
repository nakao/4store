/*
 *  Copyright (C) 2006 Steve Harris
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  $Id: filter.h $
 */

#ifndef FILTER_DATATYPES_H
#define  FILTER_DATATYPES_H

#include <stdint.h>
#include <time.h>

#include "query-datatypes.h"
#include "decimal.h"
#include "common/datatypes.h"

typedef enum {
    FS_ERROR_INVALID_TYPE = 99
} fs_error;

#if 0
typedef struct _fs_fixp {
    int32_t	in;
    uint32_t	fr;
} fs_fixp;
#endif

typedef enum {
    FS_V_RID = 0,
    FS_V_ATTR,
    FS_V_FP,
    FS_V_IN,
    FS_V_DE,
    FS_V_DA,
    FS_V_ED,
    FS_V_TYPE_ERROR,
    FS_V_DESC
} fs_value_field;

typedef struct _fs_value {
    uint32_t    valid;
    fs_rid      rid;
    fs_rid      attr;
    char       *lex;
    double      fp;
    fs_decimal  de;
    int64_t     in;
    time_t      da;
} fs_value;

static inline uint32_t fs_valid_bit(fs_value_field f)
{
    return 1 << f;
}

fs_value fs_value_blank();
fs_value fs_value_rid(fs_rid r);
fs_value fs_value_resource(fs_query *q, fs_resource *r);
fs_value fs_value_error(fs_error e, const char *msg);

fs_value fs_value_uri(const char *u);
fs_value fs_value_plain(const char *s);
fs_value fs_value_plain_with_lang(const char *s, const char *l);
fs_value fs_value_string(const char *s);
fs_value fs_value_double(double f);
fs_value fs_value_float(double f);
fs_value fs_value_decimal(double d);
fs_value fs_value_decimal_from_string(const char *s);
fs_value fs_value_integer(long long int i);
fs_value fs_value_boolean(int b);
fs_value fs_value_datetime(time_t d);
fs_value fs_value_datetime_from_string(const char *s);

fs_value fs_value_promote(fs_query *q, fs_value a, fs_value b);

int fs_is_numeric(fs_value *a);
int fs_is_error(fs_value a);
int fs_value_is_true(fs_value a);
int fs_value_equal(fs_value a, fs_value b);

void fs_value_print(fs_value v);

#endif

/* vi:set ts=8 sts=4 sw=4: */
