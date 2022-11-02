#include <math.h>
#include <inttypes.h>
#include "int64.h"
#include "lua/api.h"

#define INT64_TYPE  "__auto_int64"

#define INT64_MATH_TEMPLATE(L, idx1, idx2, F)   \
    int64_t v1 = _int64_get_value_fast(L, idx1);\
    int64_t v2 = _int64_get_value_fast(L, idx2);\
    return _int64_push_value(L, F(v1, v2))

typedef struct atd_int64
{
    int64_t value;
} atd_int64_t;

static int _int64_push_value(lua_State *L, int64_t value);

static int64_t _int64_get_value_fast(lua_State* L, int idx)
{
    atd_int64_t* v = lua_touserdata(L, idx);
    if (v != NULL)
    {
        return v->value;
    }
    return lua_tointeger(L, idx);
}

static int _int64_add_compat(lua_State* L)
{
#define OP_ADD(a, b) ((a) + (b))
    INT64_MATH_TEMPLATE(L, 1, 2, OP_ADD);
#undef OP_ADD
}

static int _int64_sub_compat(lua_State* L)
{
#define OP_SUB(a, b) ((a) - (b))
    INT64_MATH_TEMPLATE(L, 1, 2, OP_SUB);
#undef OP_SUB
}

static int _int64_mul_compat(lua_State* L)
{
#define OP_MUL(a, b) ((a) * (b))
    INT64_MATH_TEMPLATE(L, 1, 2, OP_MUL);
#undef OP_MUL
}

static int _int64_div_compat(lua_State* L)
{
#define OP_DIV(a, b) ((a) / (b))
    INT64_MATH_TEMPLATE(L, 1, 2, OP_DIV);
#undef OP_DIV
}

static int _int64_mod_compat(lua_State* L)
{
#define OP_MOD(a, b)    ((a) / (b))
    INT64_MATH_TEMPLATE(L, 1, 2, OP_MOD);
#undef OP_MOD
}

static int _int64_pow_compat(lua_State* L)
{
#define OP_POW(a, b) pow(a, b)
    INT64_MATH_TEMPLATE(L, 1, 2, OP_POW);
#undef OP_POW
}

static int _int64_unm(lua_State* L)
{
    atd_int64_t* v = lua_touserdata(L, 1);
    return _int64_push_value(L, -(v->value));
}

static int _int64_bor(lua_State* L)
{
#define OP_BOR(a, b)    ((a) | (b))
    INT64_MATH_TEMPLATE(L, 1, 2, OP_BOR);
#undef OP_BOR
}

static int _int64_band(lua_State* L)
{
#define OP_BAND(a, b)   ((a) & (b))
    INT64_MATH_TEMPLATE(L, 1, 2, OP_BAND);
#undef OP_BAND
}

static int _int64_bxor(lua_State* L)
{
#define OP_BXOR(a, b)   ((a) ^ (b))
    INT64_MATH_TEMPLATE(L, 1, 2, OP_BXOR);
#undef OP_BXOR
}

static int _int64_bnot(lua_State* L)
{
    atd_int64_t* v = lua_touserdata(L, 1);
    return _int64_push_value(L, ~(v->value));
}

static int _int64_eq_compat(lua_State* L)
{
    int64_t v1 = _int64_get_value_fast(L, 1);
    int64_t v2 = _int64_get_value_fast(L, 2);
    lua_pushboolean(L, v1 == v2);
    return 1;
}

static int _int64_lt_compat(lua_State* L)
{
    int64_t v1 = _int64_get_value_fast(L, 1);
    int64_t v2 = _int64_get_value_fast(L, 2);
    lua_pushboolean(L, v1 < v2);
    return 1;
}

static int _int64_le_compat(lua_State* L)
{
    int64_t v1 = _int64_get_value_fast(L, 1);
    int64_t v2 = _int64_get_value_fast(L, 2);
    lua_pushboolean(L, v1 <= v2);
    return 1;
}

static int _int64_tostring(lua_State* L)
{
    atd_int64_t* v1 = lua_touserdata(L, 1);
    lua_pushfstring(L, "%" PRId64, v1->value);
    return 1;
}

static int _int64_push_value(lua_State *L, int64_t value)
{
    atd_int64_t* v = lua_newuserdata(L, sizeof(atd_int64_t));
    v->value = value;

    static const luaL_Reg s_int64_meta[] = {
        { "__add",      _int64_add_compat },
        { "__sub",      _int64_sub_compat },
        { "__mul",      _int64_mul_compat },
        { "__div",      _int64_div_compat },
        { "__mod",      _int64_mod_compat },
        { "__pow",      _int64_pow_compat },
        { "__unm",      _int64_unm },
        { "__bor",      _int64_bor },
        { "__band",     _int64_band },
        { "__bxor",     _int64_bxor },
        { "__bnot",     _int64_bnot },
        { "__eq",       _int64_eq_compat },
        { "__lt",       _int64_lt_compat },
        { "__le",       _int64_le_compat },
        { "__tostring", _int64_tostring },
        { NULL,         NULL },
    };

    if (luaL_newmetatable(L, INT64_TYPE) != 0)
    {
        luaL_setfuncs(L, s_int64_meta, 0);
    }
    lua_setmetatable(L, -2);

    return 1;
}

static int _int64_get_value(lua_State *L, int idx, int64_t* value)
{
    atd_int64_t* v = luaL_testudata(L, idx, INT64_TYPE);
    if (v == NULL)
    {
        *value = 0;
        return 0;
    }

    *value = v->value;
    return 1;
}

const auto_api_int64_t api_int64 = {
    _int64_push_value,               /* .int64.push_value */
    _int64_get_value,                /* .int64.get_value */
};
