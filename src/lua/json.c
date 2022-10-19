#include "json.h"
#include "utils.h"
#include <string.h>

typedef struct lua_json
{
    cJSON*          json;   /**< Internal JSON object */
} lua_json_t;

#define JSON_CONSTANT_EMPTY_ARRAY   ((void*)1)

static int _json_to_table_object(lua_State* L, int idx, cJSON* src);

static int _json_gc(lua_State* L)
{
    lua_json_t* json = lua_touserdata(L, 1);
    if (json->json != NULL)
    {
        cJSON_Delete(json->json);
        json->json = NULL;
    }

    return 0;
}

static int _json_to_table_array(lua_State* L, int idx, cJSON* src)
{
    cJSON* item;
    cJSON_ArrayForEach(item, src)
    {
        switch(item->type)
        {
        case cJSON_False:
            lua_pushboolean(L, 0);
            break;

        case cJSON_True:
            lua_pushboolean(L, 1);
            break;

        case cJSON_NULL:
            lua_pushlightuserdata(L, NULL);
            break;

        case cJSON_Number:
            lua_pushnumber(L, item->valuedouble);
            break;

        case cJSON_String:
            lua_pushstring(L, item->valuestring);
            break;

        case cJSON_Array:
            lua_newtable(L);
            _json_to_table_array(L, idx + 1, item->child);
            break;

        case cJSON_Object:
            lua_newtable(L);
            _json_to_table_object(L, idx + 1, item->child);
            break;
        }

        lua_seti(L, idx, luaL_len(L, idx) + 1);
    }

    return 0;
}

static int _json_to_table_object(lua_State* L, int idx, cJSON* src)
{
    cJSON* item;
    cJSON_ArrayForEach(item, src)
    {
        switch(item->type)
        {
        case cJSON_False:
            lua_pushboolean(L, 0);
            break;

        case cJSON_True:
            lua_pushboolean(L, 1);
            break;

        case cJSON_NULL:
            lua_pushlightuserdata(L, NULL);
            break;

        case cJSON_Number:
            lua_pushnumber(L, item->valuedouble);
            break;

        case cJSON_String:
            lua_pushstring(L, item->valuestring);
            break;

        case cJSON_Array:
            lua_newtable(L);
            _json_to_table_array(L, idx + 1, item->child);
            break;

        case cJSON_Object:
            lua_newtable(L);
            _json_to_table_object(L, idx + 1, item->child);
            break;
        }

        lua_setfield(L, idx, item->string);
    }

    return 0;
}

static int _json_decode(lua_State* L)
{
    int sp = lua_gettop(L);

    size_t str_len;
    const char* str = lua_tolstring(L, 2, &str_len);

    cJSON* json_obj = cJSON_ParseWithLength(str, str_len);

    if (json_obj == NULL)
    {
        return 0;
    }

    lua_newtable(L);
    _json_to_table_object(L, sp + 1, json_obj);
    cJSON_Delete(json_obj);

    return 1;
}

/**
 *
 * @param L
 * @param idx
 * @return      Boolean.
 */
static int _is_table_array_fast(lua_State* L, int idx)
{
    int type;

    /* [1] must exist */
    type = lua_geti(L, idx, 1);
    lua_pop(L, 1);
    if (type == LUA_TNIL)
    {
        return 0;
    }

    /* [#t] must exist*/
    type = lua_geti(L, idx, luaL_len(L, idx));
    lua_pop(L, 1);
    if (type == LUA_TNIL)
    {
        return 0;
    }

    /* [#t+1] must not exist */
    type = lua_geti(L, idx, luaL_len(L, idx) + 1);
    lua_pop(L, 1);
    if (type != LUA_TNIL)
    {
        return 0;
    }

    return 1;
}

static int _table_to_json_add_item_to_array(lua_State* L, cJSON* arr, int idx)
{
    int val_type = lua_type(L, idx);
    switch (val_type)
    {
    case LUA_TNIL:
        cJSON_AddItemToArray(arr, cJSON_CreateNull());
        break;

    case LUA_TNUMBER:
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(lua_tonumber(L, idx)));
        break;

    case LUA_TBOOLEAN:
        cJSON_AddItemToArray(arr, cJSON_CreateBool(lua_toboolean(L, idx)));
        break;

    case LUA_TSTRING:
        cJSON_AddItemToArray(arr, cJSON_CreateString(lua_tostring(L, idx)));
        break;

    case LUA_TTABLE:
        cJSON_AddItemToArray(arr, auto_lua_json_from_table(L, idx));
        break;

    case LUA_TLIGHTUSERDATA:
        if (lua_touserdata(L, idx) == NULL)
        {
            cJSON_AddItemToArray(arr, cJSON_CreateNull());
        }
        break;

    default:
        break;
    }

    return 0;
}

static int _table_to_json_add_item_to_object(lua_State* L, cJSON* obj, int kidx, int vidx)
{
    if (lua_type(L, kidx) != LUA_TSTRING)
    {
        return 0;
    }
    const char* key = lua_tostring(L, kidx);

    void* luv = NULL;
    int val_type = lua_type(L, vidx);
    switch (val_type)
    {
    case LUA_TNIL:
        cJSON_AddItemToObject(obj, key, cJSON_CreateNull());
        break;

    case LUA_TNUMBER:
        cJSON_AddItemToObject(obj, key, cJSON_CreateNumber(lua_tonumber(L, vidx)));
        break;

    case LUA_TBOOLEAN:
        cJSON_AddItemToObject(obj, key, cJSON_CreateBool(lua_toboolean(L,vidx)));
        break;

    case LUA_TSTRING:
        cJSON_AddItemToObject(obj, key, cJSON_CreateString(lua_tostring(L, vidx)));
        break;

    case LUA_TTABLE:
        cJSON_AddItemToObject(obj, key, auto_lua_json_from_table(L, vidx));
        break;

    case LUA_TLIGHTUSERDATA:
        luv = lua_touserdata(L, vidx);
        if (luv == NULL)
        {
            cJSON_AddItemToObject(obj, key, cJSON_CreateNull());
        }
        else if (luv == JSON_CONSTANT_EMPTY_ARRAY)
        {
            cJSON_AddItemToObject(obj, key, cJSON_CreateArray());
        }

        break;

    default:
        break;
    }

    return 0;
}

static int _json_encode(lua_State* L)
{
    cJSON* obj = auto_lua_json_from_table(L, 2);

    char* str = cJSON_PrintUnformatted(obj);
    lua_pushstring(L, str);
    cJSON_free(str);

    return 1;
}

static void _json_set_metatable(lua_State* L)
{
    static const luaL_Reg s_json_meta[] = {
        { "__gc",       _json_gc },
        { NULL,         NULL },
    };
    static const luaL_Reg s_json_method[] = {
        { "decode",     _json_decode },
        { "encode",     _json_encode },
        { NULL,         NULL },
    };
    if (luaL_newmetatable(L, "__auto_process") != 0)
    {
        luaL_setfuncs(L, s_json_meta, 0);
        luaL_newlib(L, s_json_method);

        lua_pushlightuserdata(L, NULL);
        lua_setfield(L, -2, "null");

        lua_pushlightuserdata(L, JSON_CONSTANT_EMPTY_ARRAY);
        lua_setfield(L, -2, "empty_array");

        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
}

int auto_lua_json(lua_State* L)
{
    lua_json_t* json = lua_newuserdata(L, sizeof(lua_json_t));
    memset(json, 0, sizeof(*json));

    _json_set_metatable(L);

    return 1;
}

cJSON* auto_lua_json_from_table(lua_State* L, int idx)
{
    cJSON* obj = NULL;
    int sp = lua_gettop(L);

    if (_is_table_array_fast(L, idx))
    {
        obj = cJSON_CreateArray();

        lua_pushnil(L);
        while (lua_next(L, idx) != 0)
        {
            _table_to_json_add_item_to_array(L, obj, sp + 2);
            lua_pop(L, 1);
        }
    }
    else
    {
        obj = cJSON_CreateObject();

        lua_pushnil(L);
        while (lua_next(L, idx) != 0)
        {
            _table_to_json_add_item_to_object(L, obj, sp + 1, sp + 2);
            lua_pop(L, 1);
        }
    }

    return obj;
}
