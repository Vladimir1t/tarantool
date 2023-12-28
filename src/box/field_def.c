/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "field_def.h"
#include "identifier.h"
#include "trivia/util.h"
#include "key_def.h"
#include "mp_extension_types.h"
#include "mp_uuid.h"
#include "schema_def.h"
#include "tt_uuid.h"
#include "tt_static.h"
#include "tuple_constraint_def.h"
#include "tuple_dictionary.h"
#include "tuple_format.h"
#include "salad/grp_alloc.h"
#include "small/region.h"

const char *mp_type_strs[] = {
	/* .MP_NIL    = */ "nil",
	/* .MP_UINT   = */ "unsigned",
	/* .MP_INT    = */ "integer",
	/* .MP_STR    = */ "string",
	/* .MP_BIN    = */ "blob",
	/* .MP_ARRAY  = */ "array",
	/* .MP_MAP    = */ "map",
	/* .MP_BOOL   = */ "boolean",
	/* .MP_FLOAT  = */ "float",
	/* .MP_DOUBLE = */ "double",
	/* .MP_EXT    = */ "extension",
};

/*
 * messagepack types supported by given field types.
 * MP_EXT requires to parse extension type to check
 * compatibility with field type.
 */
const uint32_t field_mp_type[] = {
	/* [FIELD_TYPE_ANY]      =  */ UINT32_MAX,
	/* [FIELD_TYPE_UNSIGNED] =  */ 1U << MP_UINT,
	/* [FIELD_TYPE_STRING]   =  */ 1U << MP_STR,
	/* [FIELD_TYPE_NUMBER]   =  */ (1U << MP_UINT) | (1U << MP_INT) |
		(1U << MP_FLOAT) | (1U << MP_DOUBLE),
	/* [FIELD_TYPE_DOUBLE]   =  */ (1U << MP_UINT) | (1U << MP_INT) |
		(1U << MP_FLOAT) | (1U << MP_DOUBLE),
	/* [FIELD_TYPE_INTEGER]  =  */ (1U << MP_UINT) | (1U << MP_INT),
	/* [FIELD_TYPE_BOOLEAN]  =  */ 1U << MP_BOOL,
	/* [FIELD_TYPE_VARBINARY] =  */ 1U << MP_BIN,
	/* [FIELD_TYPE_SCALAR]   =  */ (1U << MP_UINT) | (1U << MP_INT) |
		(1U << MP_FLOAT) | (1U << MP_DOUBLE) | (1U << MP_STR) |
		(1U << MP_BIN) | (1U << MP_BOOL),
	/* [FIELD_TYPE_DECIMAL]  =  */ 0, /* only MP_DECIMAL is supported */
	/* [FIELD_TYPE_UUID]     =  */ 0, /* only MP_UUID is supported */
	/* [FIELD_TYPE_DATETIME] =  */ 0, /* only MP_DATETIME is supported */
	/* [FIELD_TYPE_INTERVAL] =  */ 0,
	/* [FIELD_TYPE_ARRAY]    =  */ 1U << MP_ARRAY,
	/* [FIELD_TYPE_MAP]      =  */ (1U << MP_MAP),
	/* [FIELD_TYPE_INT8]     =  */ (1U << MP_UINT) | (1U << MP_INT),
	/* [FIELD_TYPE_UINT8]    =  */ 1U << MP_UINT,
	/* [FIELD_TYPE_INT16]    =  */ (1U << MP_UINT) | (1U << MP_INT),
	/* [FIELD_TYPE_UINT16]   =  */ 1U << MP_UINT,
	/* [FIELD_TYPE_INT32]    =  */ (1U << MP_UINT) | (1U << MP_INT),
	/* [FIELD_TYPE_UINT32]   =  */ 1U << MP_UINT,
	/* [FIELD_TYPE_INT64]    =  */ (1U << MP_UINT) | (1U << MP_INT),
	/* [FIELD_TYPE_UINT64]   =  */ 1U << MP_UINT,
	/* [FIELD_TYPE_FLOAT32]  =  */ 1U << MP_FLOAT,
	/* [FIELD_TYPE_FLOAT64]  =  */ 1U << MP_DOUBLE,
};

static_assert(lengthof(field_mp_type) == field_type_MAX,
	      "Each field type must be present in field_mp_type");

const uint32_t field_ext_type[] = {
	/* [FIELD_TYPE_ANY]       = */ UINT32_MAX ^ (1U << MP_UNKNOWN_EXTENSION),
	/* [FIELD_TYPE_UNSIGNED]  = */ 0,
	/* [FIELD_TYPE_STRING]    = */ 0,
	/* [FIELD_TYPE_NUMBER]    = */ 1U << MP_DECIMAL,
	/* [FIELD_TYPE_DOUBLE]    = */ 0,
	/* [FIELD_TYPE_INTEGER]   = */ 0,
	/* [FIELD_TYPE_BOOLEAN]   = */ 0,
	/* [FIELD_TYPE_VARBINARY] = */ 0,
	/* [FIELD_TYPE_SCALAR]    = */ (1U << MP_DECIMAL) | (1U << MP_UUID) |
		(1U << MP_DATETIME),
	/* [FIELD_TYPE_DECIMAL]   = */ 1U << MP_DECIMAL,
	/* [FIELD_TYPE_UUID]      = */ 1U << MP_UUID,
	/* [FIELD_TYPE_DATETIME]  = */ 1U << MP_DATETIME,
	/* [FIELD_TYPE_INTERVAL]  = */ 1U << MP_INTERVAL,
	/* [FIELD_TYPE_ARRAY]     = */ 0,
	/* [FIELD_TYPE_MAP]       = */ 0,
	/* [FIELD_TYPE_INT8]      = */ 0,
	/* [FIELD_TYPE_UINT8]     = */ 0,
	/* [FIELD_TYPE_INT16]     = */ 0,
	/* [FIELD_TYPE_UINT16]    = */ 0,
	/* [FIELD_TYPE_INT32]     = */ 0,
	/* [FIELD_TYPE_UINT32]    = */ 0,
	/* [FIELD_TYPE_INT64]     = */ 0,
	/* [FIELD_TYPE_UINT64]    = */ 0,
	/* [FIELD_TYPE_FLOAT32]   = */ 0,
	/* [FIELD_TYPE_FLOAT64]   = */ 0,
};

static_assert(lengthof(field_ext_type) == field_type_MAX,
	      "Each field type must be present in field_ext_type");

const char *field_type_strs[] = {
	/* [FIELD_TYPE_ANY]      = */ "any",
	/* [FIELD_TYPE_UNSIGNED] = */ "unsigned",
	/* [FIELD_TYPE_STRING]   = */ "string",
	/* [FIELD_TYPE_NUMBER]   = */ "number",
	/* [FIELD_TYPE_DOUBLE]   = */ "double",
	/* [FIELD_TYPE_INTEGER]  = */ "integer",
	/* [FIELD_TYPE_BOOLEAN]  = */ "boolean",
	/* [FIELD_TYPE_VARBINARY] = */"varbinary",
	/* [FIELD_TYPE_SCALAR]   = */ "scalar",
	/* [FIELD_TYPE_DECIMAL]  = */ "decimal",
	/* [FIELD_TYPE_UUID]     = */ "uuid",
	/* [FIELD_TYPE_DATETIME] = */ "datetime",
	/* [FIELD_TYPE_INTERVAL] = */ "interval",
	/* [FIELD_TYPE_ARRAY]    = */ "array",
	/* [FIELD_TYPE_MAP]      = */ "map",
	/* [FIELD_TYPE_INT8]     = */ "int8",
	/* [FIELD_TYPE_UINT8]    = */ "uint8",
	/* [FIELD_TYPE_INT16]    = */ "int16",
	/* [FIELD_TYPE_UINT16]   = */ "uint16",
	/* [FIELD_TYPE_INT32]    = */ "int32",
	/* [FIELD_TYPE_UINT32]   = */ "uint32",
	/* [FIELD_TYPE_INT64]    = */ "int64",
	/* [FIELD_TYPE_UINT64]   = */ "uint64",
	/* [FIELD_TYPE_FLOAT32]  = */ "float32",
	/* [FIELD_TYPE_FLOAT64]  = */ "float64",
};

static_assert(lengthof(field_type_strs) == field_type_MAX,
	      "Each field type must be present in field_type_strs");

/**
 * True for fixed-size signed integer field types.
 */
const bool field_type_is_fixed_signed[] = {
	/* [FIELD_TYPE_ANY]       = */ false,
	/* [FIELD_TYPE_UNSIGNED]  = */ false,
	/* [FIELD_TYPE_STRING]    = */ false,
	/* [FIELD_TYPE_NUMBER]    = */ false,
	/* [FIELD_TYPE_DOUBLE]    = */ false,
	/* [FIELD_TYPE_INTEGER]   = */ false,
	/* [FIELD_TYPE_BOOLEAN]   = */ false,
	/* [FIELD_TYPE_VARBINARY] = */ false,
	/* [FIELD_TYPE_SCALAR]    = */ false,
	/* [FIELD_TYPE_DECIMAL]   = */ false,
	/* [FIELD_TYPE_UUID]      = */ false,
	/* [FIELD_TYPE_DATETIME]  = */ false,
	/* [FIELD_TYPE_INTERVAL]  = */ false,
	/* [FIELD_TYPE_ARRAY]     = */ false,
	/* [FIELD_TYPE_MAP]       = */ false,
	/* [FIELD_TYPE_INT8]      = */ true,
	/* [FIELD_TYPE_UINT8]     = */ false,
	/* [FIELD_TYPE_INT16]     = */ true,
	/* [FIELD_TYPE_UINT16]    = */ false,
	/* [FIELD_TYPE_INT32]     = */ true,
	/* [FIELD_TYPE_UINT32]    = */ false,
	/* [FIELD_TYPE_INT64]     = */ true,
	/* [FIELD_TYPE_UINT64]    = */ false,
	/* [FIELD_TYPE_FLOAT32]   = */ false,
	/* [FIELD_TYPE_FLOAT64]   = */ false,
};

static_assert(lengthof(field_type_is_fixed_signed) == field_type_MAX,
	      "Each field type must be present in field_type_is_fixed_signed");

/**
 * True for fixed-size unsigned integer field types.
 */
const bool field_type_is_fixed_unsigned[] = {
	/* [FIELD_TYPE_ANY]       = */ false,
	/* [FIELD_TYPE_UNSIGNED]  = */ false,
	/* [FIELD_TYPE_STRING]    = */ false,
	/* [FIELD_TYPE_NUMBER]    = */ false,
	/* [FIELD_TYPE_DOUBLE]    = */ false,
	/* [FIELD_TYPE_INTEGER]   = */ false,
	/* [FIELD_TYPE_BOOLEAN]   = */ false,
	/* [FIELD_TYPE_VARBINARY] = */ false,
	/* [FIELD_TYPE_SCALAR]    = */ false,
	/* [FIELD_TYPE_DECIMAL]   = */ false,
	/* [FIELD_TYPE_UUID]      = */ false,
	/* [FIELD_TYPE_DATETIME]  = */ false,
	/* [FIELD_TYPE_INTERVAL]  = */ false,
	/* [FIELD_TYPE_ARRAY]     = */ false,
	/* [FIELD_TYPE_MAP]       = */ false,
	/* [FIELD_TYPE_INT8]      = */ false,
	/* [FIELD_TYPE_UINT8]     = */ true,
	/* [FIELD_TYPE_INT16]     = */ false,
	/* [FIELD_TYPE_UINT16]    = */ true,
	/* [FIELD_TYPE_INT32]     = */ false,
	/* [FIELD_TYPE_UINT32]    = */ true,
	/* [FIELD_TYPE_INT64]     = */ false,
	/* [FIELD_TYPE_UINT64]    = */ true,
	/* [FIELD_TYPE_FLOAT32]   = */ false,
	/* [FIELD_TYPE_FLOAT64]   = */ false,
};

static_assert(lengthof(field_type_is_fixed_unsigned) == field_type_MAX, "Each "
	      "field type must be present in field_type_is_fixed_unsigned");

const int64_t field_type_min_value[] = {
	/* [FIELD_TYPE_ANY]       = */ 0,
	/* [FIELD_TYPE_UNSIGNED]  = */ 0,
	/* [FIELD_TYPE_STRING]    = */ 0,
	/* [FIELD_TYPE_NUMBER]    = */ 0,
	/* [FIELD_TYPE_DOUBLE]    = */ 0,
	/* [FIELD_TYPE_INTEGER]   = */ 0,
	/* [FIELD_TYPE_BOOLEAN]   = */ 0,
	/* [FIELD_TYPE_VARBINARY] = */ 0,
	/* [FIELD_TYPE_SCALAR]    = */ 0,
	/* [FIELD_TYPE_DECIMAL]   = */ 0,
	/* [FIELD_TYPE_UUID]      = */ 0,
	/* [FIELD_TYPE_DATETIME]  = */ 0,
	/* [FIELD_TYPE_INTERVAL]  = */ 0,
	/* [FIELD_TYPE_ARRAY]     = */ 0,
	/* [FIELD_TYPE_MAP]       = */ 0,
	/* [FIELD_TYPE_INT8]      = */ INT8_MIN,
	/* [FIELD_TYPE_UINT8]     = */ 0,
	/* [FIELD_TYPE_INT16]     = */ INT16_MIN,
	/* [FIELD_TYPE_UINT16]    = */ 0,
	/* [FIELD_TYPE_INT32]     = */ INT32_MIN,
	/* [FIELD_TYPE_UINT32]    = */ 0,
	/* [FIELD_TYPE_INT64]     = */ INT64_MIN,
	/* [FIELD_TYPE_UINT64]    = */ 0,
	/* [FIELD_TYPE_FLOAT32]   = */ 0,
	/* [FIELD_TYPE_FLOAT64]   = */ 0,
};

static_assert(lengthof(field_type_min_value) == field_type_MAX,
	      "Each field type must be present in field_type_min_value");

const uint64_t field_type_max_value[] = {
	/* [FIELD_TYPE_ANY]       = */ 0,
	/* [FIELD_TYPE_UNSIGNED]  = */ 0,
	/* [FIELD_TYPE_STRING]    = */ 0,
	/* [FIELD_TYPE_NUMBER]    = */ 0,
	/* [FIELD_TYPE_DOUBLE]    = */ 0,
	/* [FIELD_TYPE_INTEGER]   = */ 0,
	/* [FIELD_TYPE_BOOLEAN]   = */ 0,
	/* [FIELD_TYPE_VARBINARY] = */ 0,
	/* [FIELD_TYPE_SCALAR]    = */ 0,
	/* [FIELD_TYPE_DECIMAL]   = */ 0,
	/* [FIELD_TYPE_UUID]      = */ 0,
	/* [FIELD_TYPE_DATETIME]  = */ 0,
	/* [FIELD_TYPE_INTERVAL]  = */ 0,
	/* [FIELD_TYPE_ARRAY]     = */ 0,
	/* [FIELD_TYPE_MAP]       = */ 0,
	/* [FIELD_TYPE_INT8]      = */ INT8_MAX,
	/* [FIELD_TYPE_UINT8]     = */ UINT8_MAX,
	/* [FIELD_TYPE_INT16]     = */ INT16_MAX,
	/* [FIELD_TYPE_UINT16]    = */ UINT16_MAX,
	/* [FIELD_TYPE_INT32]     = */ INT32_MAX,
	/* [FIELD_TYPE_UINT32]    = */ UINT32_MAX,
	/* [FIELD_TYPE_INT64]     = */ INT64_MAX,
	/* [FIELD_TYPE_UINT64]    = */ UINT64_MAX,
	/* [FIELD_TYPE_FLOAT32]   = */ 0,
	/* [FIELD_TYPE_FLOAT64]   = */ 0,
};

static_assert(lengthof(field_type_max_value) == field_type_MAX,
	      "Each field type must be present in field_type_max_value");

const char *on_conflict_action_strs[] = {
	/* [ON_CONFLICT_ACTION_NONE]     = */ "none",
	/* [ON_CONFLICT_ACTION_ROLLBACK] = */ "rollback",
	/* [ON_CONFLICT_ACTION_ABORT]    = */ "abort",
	/* [ON_CONFLICT_ACTION_FAIL]     = */ "fail",
	/* [ON_CONFLICT_ACTION_IGNORE]   = */ "ignore",
	/* [ON_CONFLICT_ACTION_REPLACE]  = */ "replace",
	/* [ON_CONFLICT_ACTION_DEFAULT]  = */ "default"
};

static int64_t
field_type_by_name_wrapper(const char *str, uint32_t len)
{
	return field_type_by_name(str, len);
}

bool
field_type1_contains_type2(enum field_type type1, enum field_type type2)
{
	uint32_t mp_type1, mp_type2;
	assert(type1 < field_type_MAX);
	assert(type2 < field_type_MAX);
	if (type1 == type2)
		return true;

	bool is_mp_ext = field_mp_type[type2] == 0;
	if (is_mp_ext) {
		mp_type1 = field_ext_type[type1];
		mp_type2 = field_ext_type[type2];
	} else {
		mp_type1 = field_mp_type[type1];
		mp_type2 = field_mp_type[type2];
	}

	bool type1_contains_type2 = (mp_type1 & mp_type2) == mp_type2;
	if (!type1_contains_type2)
		return false;
	if (!tuple_field_type_is_fixed_int(type1))
		return true;
	if (!tuple_field_type_is_fixed_int(type2)) {
		/* uint64 is an alias for unsigned. */
		return type1 == FIELD_TYPE_UINT64 &&
		       type2 == FIELD_TYPE_UNSIGNED;
	}

	int64_t min1 = field_type_min_value[type1];
	int64_t min2 = field_type_min_value[type2];
	uint64_t max1 = field_type_max_value[type1];
	uint64_t max2 = field_type_max_value[type2];
	return min1 <= min2 && max1 >= max2;
}

/**
 * Callback to parse a value with 'default' key in msgpack field definition.
 * See function definition below.
 */
static int
field_def_parse_default_value(const char **data, void *opts,
			      struct region *region);

/**
 * Callback to parse a value with 'constraint' key in msgpack field definition.
 * See function definition below.
 */
static int
field_def_parse_constraint(const char **data, void *opts,
			   struct region *region);

/**
 * Callback to parse a value with 'foreign_key' key in msgpack field definition.
 * See function definition below.
 */
static int
field_def_parse_foreign_key(const char **data, void *opts,
			    struct region *region);

static const struct opt_def field_def_reg[] = {
	OPT_DEF_ENUM("type", field_type, struct field_def, type,
		     field_type_by_name_wrapper),
	OPT_DEF("name", OPT_STRPTR, struct field_def, name),
	OPT_DEF("is_nullable", OPT_BOOL, struct field_def, is_nullable),
	OPT_DEF_ENUM("nullable_action", on_conflict_action, struct field_def,
		     nullable_action, NULL),
	OPT_DEF("collation", OPT_UINT32, struct field_def, coll_id),
	OPT_DEF_ENUM("compression", compression_type, struct field_def,
		     compression_type, NULL),
	OPT_DEF_CUSTOM("default", field_def_parse_default_value),
	OPT_DEF("default_func", OPT_UINT32, struct field_def, default_func_id),
	OPT_DEF_CUSTOM("constraint", field_def_parse_constraint),
	OPT_DEF_CUSTOM("foreign_key", field_def_parse_foreign_key),
	OPT_END,
};

static const struct opt_def field_def_reg_names_only[] = {
	OPT_DEF("name", OPT_STRPTR, struct field_def, name),
	OPT_END,
};

const struct field_def field_def_default = {
	.type = FIELD_TYPE_ANY,
	.name = NULL,
	.is_nullable = false,
	.nullable_action = ON_CONFLICT_ACTION_DEFAULT,
	.coll_id = COLL_NONE,
	.default_value = NULL,
	.default_value_size = 0,
	.default_func_id = 0,
	.constraint_count = 0,
	.constraint_def = NULL,
};

enum field_type
field_type_by_name(const char *name, size_t len)
{
	enum field_type field_type = strnindex(field_type_strs, name, len,
					       field_type_MAX);
	if (field_type != field_type_MAX)
		return field_type;
	/* 'num' and 'str' in _index are deprecated since Tarantool 1.7 */
	if (len == 3 && strncasecmp(name, "num", len) == 0)
		return FIELD_TYPE_UNSIGNED;
	else if (len == 3 && strncasecmp(name, "str", len) == 0)
		return FIELD_TYPE_STRING;
	else if (len == 1 && name[0] == '*')
		return FIELD_TYPE_ANY;
	return field_type_MAX;
}

/**
 * Parse default field value from msgpack.
 * Used as callback to parse a value with 'default' key in field definition.
 * Move @a data msgpack pointer to the end of msgpack value.
 * By convention @a opts must point to corresponding struct field_def.
 * Allocate a temporary copy of a default value on @a region and set pointer to
 * it as field_def->default_value, also setting field_def->default_value_size.
 */
static int
field_def_parse_default_value(const char **data, void *opts,
			      struct region *region)
{
	struct field_def *def = (struct field_def *)opts;
	const char *default_value = *data;
	mp_next(data);
	const char *default_value_end = *data;
	size_t size = default_value_end - default_value;

	def->default_value = xregion_alloc(region, size);
	def->default_value_size = size;
	memcpy(def->default_value, default_value, size);
	return 0;
}

/**
 * Parse constraint array from msgpack.
 * Used as callback to parse a value with 'constraint' key in field definition.
 * Move @a data msgpack pointer to the end of msgpack value.
 * By convention @a opts must point to corresponding struct field_def.
 * Allocate a temporary constraint array on @a region and set pointer to it
 *  as field_def->constraint, also setting field_def->constraint_count.
 * If there are constraints already - realloc and append array.
 * Return 0 on success or -1 on error (diag is set to IllegalParams).
 */
static int
field_def_parse_constraint(const char **data, void *opts, struct region *region)
{
	/* Expected normal form of constraints: {name1=func1, name2=func2..}. */
	struct field_def *def = (struct field_def *)opts;
	return tuple_constraint_def_decode(data, &def->constraint_def,
					   &def->constraint_count, region);
}

/**
 * Parse foreign_key array from msgpack.
 * Used as callback to parse a value with 'foreign_key' key in field definition.
 * Move @a data msgpack pointer to the end of msgpack value.
 * By convention @a opts must point to corresponding struct field_def.
 * Allocate a temporary constraint array on @a region and set pointer to it
 *  as field_def->constraint, also setting field_def->constraint_count.
 * If there are constraints already - realloc and append array.
 * Return 0 on success or -1 on error (diag is set to IllegalParams).
 */
static int
field_def_parse_foreign_key(const char **data, void *opts,
			    struct region *region)
{
	/* Expected normal form of constraints: {name1={space=.., field=..}.. */
	struct field_def *def = (struct field_def *)opts;
	return tuple_constraint_def_decode_fkey(data, &def->constraint_def,
						&def->constraint_count,
						region, false);
}

#define field_def_error(fieldno, errmsg)				\
	diag_set(ClientError, ER_WRONG_SPACE_FORMAT,			\
		 (unsigned)((fieldno) + TUPLE_INDEX_BASE), (errmsg))

/**
 * Decode field definition from MessagePack map. Format:
 * {name: <string>, type: <string>}. Type is optional.
 * @param[out] field Field to decode to.
 * @param data MessagePack map to decode.
 * @param fieldno Field number to decode. Used in error messages.
 * @param region Region to allocate field name.
 * @param names_only Only decode 'name' field, ignore the rest.
 */
static int
field_def_decode(struct field_def *field, const char **data,
		 uint32_t fieldno, struct region *region, bool names_only)
{
	if (mp_typeof(**data) != MP_MAP) {
		field_def_error(fieldno, "expected a map");
		return -1;
	}
	int count = mp_decode_map(data);
	*field = field_def_default;
	bool is_action_missing = true;
	uint32_t action_literal_len = strlen("nullable_action");
	for (int i = 0; i < count; ++i) {
		if (mp_typeof(**data) != MP_STR) {
			field_def_error(fieldno,
					"expected a map with string keys");
			return -1;
		}
		uint32_t key_len;
		const char *key = mp_decode_str(data, &key_len);
		const struct opt_def *reg =
			names_only ? field_def_reg_names_only : field_def_reg;
		if (opts_parse_key(field, reg, key, key_len, data,
				   region, true) != 0) {
			field_def_error(fieldno,
					diag_last_error(diag_get())->errmsg);
			return -1;
		}
		if (is_action_missing &&
		    key_len == action_literal_len &&
		    memcmp(key, "nullable_action", action_literal_len) == 0)
			is_action_missing = false;
	}
	if (is_action_missing) {
		field->nullable_action = field->is_nullable ?
					 ON_CONFLICT_ACTION_NONE :
					 ON_CONFLICT_ACTION_DEFAULT;
	}
	if (field->name == NULL) {
		field_def_error(fieldno, "field name is missing");
		return -1;
	}
	size_t field_name_len = strlen(field->name);
	if (field_name_len > BOX_NAME_MAX) {
		field_def_error(fieldno, "field name is too long");
		return -1;
	}
	if (identifier_check(field->name, field_name_len) != 0)
		return -1;
	if (field->type == field_type_MAX) {
		field_def_error(fieldno, "unknown field type");
		return -1;
	}
	if (field->nullable_action == on_conflict_action_MAX) {
		field_def_error(fieldno, "unknown nullable action");
		return -1;
	}
	if (!((field->is_nullable &&
	       field->nullable_action == ON_CONFLICT_ACTION_NONE) ||
	      (!field->is_nullable &&
	       field->nullable_action != ON_CONFLICT_ACTION_NONE))) {
		field_def_error(fieldno, "conflicting nullability and "
				"nullable action properties");
		return -1;
	}
	if (field->coll_id != COLL_NONE &&
	    field->type != FIELD_TYPE_STRING &&
	    field->type != FIELD_TYPE_SCALAR &&
	    field->type != FIELD_TYPE_ANY) {
		field_def_error(fieldno, "collation is reasonable only for "
				"'string', 'scalar', and 'any' fields");
		return -1;
	}
	if (field->compression_type == compression_type_MAX) {
		field_def_error(fieldno, "unknown compression type");
		return -1;
	}
	return 0;
}

int
field_def_array_decode(const char **data, struct field_def **fields,
		       uint32_t *field_count, struct region *region,
		       bool names_only)
{
	assert(mp_typeof(**data) == MP_ARRAY);
	uint32_t count = mp_decode_array(data);
	*field_count = count;
	if (count == 0) {
		*fields = NULL;
		return 0;
	}
	size_t size;
	struct field_def *region_defs =
		region_alloc_array(region, typeof(region_defs[0]), count,
				   &size);
	if (region_defs == NULL) {
		diag_set(OutOfMemory, size, "region_alloc_array",
			 "region_defs");
		return -1;
	}
	for (uint32_t i = 0; i < count; ++i) {
		if (field_def_decode(&region_defs[i], data, i, region,
				     names_only) != 0)
			return -1;
	}
	*fields = region_defs;
	return 0;
}

struct field_def *
field_def_array_dup(const struct field_def *fields, uint32_t field_count)
{
	if (field_count == 0)
		return NULL;
	struct grp_alloc all = grp_alloc_initializer();
	grp_alloc_reserve_data(&all, sizeof(*fields) * field_count);
	for (uint32_t i = 0; i < field_count; i++) {
		grp_alloc_reserve_str0(&all, fields[i].name);
		grp_alloc_reserve_data(&all, fields[i].default_value_size);
	}
	grp_alloc_use(&all, xmalloc(grp_alloc_size(&all)));
	struct field_def *copy = grp_alloc_create_data(
		&all, sizeof(*fields) * field_count);
	for (uint32_t i = 0; i < field_count; ++i) {
		copy[i] = fields[i];
		copy[i].name = grp_alloc_create_str0(&all, fields[i].name);
		if (fields[i].default_value != NULL) {
			size_t size = fields[i].default_value_size;
			char *buf = grp_alloc_create_data(&all, size);
			memcpy(buf, fields[i].default_value, size);
			copy[i].default_value = buf;
		}
		copy[i].constraint_def = tuple_constraint_def_array_dup(
			fields[i].constraint_def, fields[i].constraint_count);
	}
	assert(grp_alloc_size(&all) == 0);
	return copy;
}

void
field_def_array_delete(struct field_def *fields, uint32_t field_count)
{
	for (uint32_t i = 0; i < field_count; i++) {
		free(fields[i].constraint_def);
		TRASH(&fields[i]);
	}
	free(fields);
}
