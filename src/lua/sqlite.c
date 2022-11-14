#include <sqlite3.h>
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <csvparser.h>
#include <errno.h>
#include "sqlite.h"
#include "utils.h"

/**
 * @brief Lua userdata type of sqlite
 */
#define AUTO_LUA_SQLITE         "__auto_sqlite3"

typedef struct lua_sqlite
{
    sqlite3*        db;         /**< Database connection */

    struct
    {
        char*       filename;   /**< Database filename (UTF-8) */
    } config;
} lua_sqlite_t;

typedef struct lua_sqlite_exec_helper
{
    lua_State*      L;          /**< Lua VM. */
    lua_sqlite_t*   belong;     /**< SQLite instance. */
    int             tbsp;       /**< The SP of result table for store result. */
} lua_sqlite_exec_helper_t;

typedef struct sqlite_to_csv_helper
{
    int             line_cnt;   /**< Line counter. */
    luaL_Buffer     csv_buf;    /**< Buffer for holding CSV content. */
} sqlite_to_csv_helper_t;

static void _sqlite_lua_parse_options(lua_State* L, int idx, lua_sqlite_t* self)
{
    if (lua_getfield(L, idx, "filename") == LUA_TSTRING)
    {
        self->config.filename = auto_strdup(lua_tostring(L, -1));
    }
    else
    {
        self->config.filename = auto_strdup("");
    }
    lua_pop(L, 1);
}

static int _sqlite_lua_close(lua_State* L)
{
    lua_sqlite_t* self = lua_touserdata(L, 1);

    if (self->db != NULL)
    {
        sqlite3_close(self->db);
        self->db = NULL;
    }

    if (self->config.filename != NULL)
    {
        free(self->config.filename);
        self->config.filename = NULL;
    }

    return 0;
}

static int _sqlite_lua_gc(lua_State* L)
{
    return _sqlite_lua_close(L);
}

static int _sqlite_lua_tostring(lua_State* L)
{
    lua_sqlite_t* self = lua_touserdata(L, 1);

    cJSON* json = cJSON_CreateObject();

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%p", self->db);
        cJSON_AddStringToObject(json, "db", buf);
    }

    {
        cJSON* json_config = cJSON_CreateObject();

        if (self->config.filename != NULL)
        {
            cJSON_AddStringToObject(json_config, "filename", self->config.filename);
        }
        else
        {
            cJSON_AddNullToObject(json_config, "filename");
        }

        cJSON_AddItemToObject(json, "config", json_config);
    }

    char* data = cJSON_PrintUnformatted(json);
    lua_pushstring(L, data);
    cJSON_free(data);

    return 1;
}

static int _sqlite_lua_exec_callback(void* data, int argc, char** argv, char** azColName)
{
    lua_sqlite_exec_helper_t* helper = data;

    /* Create new result line */
    lua_newtable(helper->L);

    /* Store query results. */
    int i;
    for (i = 0; i < argc; i++)
    {
        if (argv[i] != NULL)
        {
            lua_pushstring(helper->L, argv[i]);
            lua_setfield(helper->L, -2, azColName[i]);
        }
    }

    /* Append result line to result table. */
    lua_rawseti(helper->L, helper->tbsp, luaL_len(helper->L, helper->tbsp) + 1);

    return 0;
}

static int _sqlite_lua_exec(lua_State* L)
{
    lua_sqlite_t* self = luaL_checkudata(L, 1, AUTO_LUA_SQLITE);
    const char* sql = luaL_checkstring(L, 2);

    /* Create a table for restore exec results. */
    lua_newtable(L);

    /* Create exec helper context. */
    lua_sqlite_exec_helper_t helper;
    helper.L = L;
    helper.belong = self;
    helper.tbsp = lua_gettop(L);

    /* Do sql exec */
    char* errmsg = NULL;
    if (sqlite3_exec(self->db, sql, _sqlite_lua_exec_callback, &helper, &errmsg) != SQLITE_OK)
    {
        lua_pushstring(L, errmsg);
        sqlite3_free(errmsg);
        return lua_error(L);
    }

    /* Ensure result table is at top of stack. */
    lua_settop(L, helper.tbsp);

    return 1;
}

static int _sqlite_lua_csv_create_table(lua_State* L, lua_sqlite_t* self, const char* table_name, CsvParser* csv_parser)
{
    luaL_Buffer sql_buf;
    luaL_buffinit(L, &sql_buf);
    luaL_addstring(&sql_buf, "CREATE TABLE IF NOT EXISTS ");
    luaL_addstring(&sql_buf, table_name);

    {
        luaL_addstring(&sql_buf, "(");

        const CsvRow* header = CsvParser_getHeader(csv_parser);
        const char **headerFields = CsvParser_getFields(header);

        int i;
        for (i = 0; i < CsvParser_getNumFields(header); i++)
        {
            char* zQuoted = sqlite3_mprintf("\"%w\"", headerFields[i]);
            luaL_addstring(&sql_buf, zQuoted);
            sqlite3_free(zQuoted);
            luaL_addchar(&sql_buf, ',');
        }

        luaL_buffsub(&sql_buf, 1);
        luaL_addstring(&sql_buf, ")");
    }

    luaL_pushresult(&sql_buf);

    /* Create table */
    const char* sql_cmd = lua_tostring(L, -1);
    char* errmsg = NULL;
    int ret = sqlite3_exec(self->db, sql_cmd, NULL, NULL, &errmsg);
    lua_pop(L, 1);

    /* If there is any error, push error string on top of stack. */
    if (ret != SQLITE_OK)
    {
        lua_pushstring(L, errmsg);
        sqlite3_free(errmsg);
        return 1;
    }

    return 0;
}

static int _sqlite_lua_csv_import_data(lua_State* L, lua_sqlite_t* self, const char* table_name, CsvParser* csv_parser)
{
    /* Parse CSV */
    CsvRow* row = NULL;
    while ((row = CsvParser_getRow(csv_parser)) != NULL)
    {
        luaL_Buffer sql_buf;
        luaL_buffinit(L, &sql_buf);
        luaL_addstring(&sql_buf, "INSERT INTO ");
        luaL_addstring(&sql_buf, table_name);
        luaL_addstring(&sql_buf, " VALUES(");

        const char** rowFields = CsvParser_getFields(row);

        int i;
        for (i = 0; i < CsvParser_getNumFields(row); i++)
        {
            char* zQuoted = sqlite3_mprintf("\"%w\"", rowFields[i]);
            luaL_addstring(&sql_buf, zQuoted);
            sqlite3_free(zQuoted);

            luaL_addchar(&sql_buf, ',');
        }

        CsvParser_destroy_row(row);

        luaL_buffsub(&sql_buf, 1);
        luaL_addstring(&sql_buf, ")");
        luaL_pushresult(&sql_buf);

        const char* sql_cmd = lua_tostring(L, -1);

        char* errmsg = NULL;
        int ret = sqlite3_exec(self->db, sql_cmd, NULL, NULL, &errmsg);
        lua_pop(L, 1);

        if (ret != SQLITE_OK)
        {
            lua_pushstring(L, errmsg);
            sqlite3_free(errmsg);
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Parse CSV into SQL table.
 * @param[in] L             Lua VM.
 * @param[in] self          SQLite instance.
 * @param[in] table_name    SQL table name.
 * @param[in] csv_parser    A CSV parser. The ownership is taken.
 * @return                  Always 0.
 */
static int _sqlite_lua_from_csv_parser(lua_State* L, lua_sqlite_t* self,
    const char* table_name, CsvParser* csv_parser)
{
    int ret;

    if (csv_parser == NULL)
    {
        return luaL_error(L, "create CSV parser failed");
    }

    /* Create table */
    if ((ret = _sqlite_lua_csv_create_table(L, self, table_name, csv_parser)) != 0)
    {
        goto finish;
    }

    /* Import CSV data */
    if ((ret = _sqlite_lua_csv_import_data(L, self, table_name, csv_parser)) != 0)
    {
        goto finish;
    }

finish:
    /* CSV parser is no longer needed. */
    CsvParser_destroy(csv_parser);
    return ret != 0 ? lua_error(L) : 0;
}

static int _sqlite_lua_from_csv(lua_State* L)
{
    /* Get parameters */
    lua_sqlite_t* self = luaL_checkudata(L, 1, AUTO_LUA_SQLITE);
    const char* table_name = luaL_checkstring(L, 2);
    const char* csv_data = luaL_checkstring(L, 3);
    int is_header = !(lua_type(L, 4) == LUA_TBOOLEAN && !lua_toboolean(L, 4));

    /* Create CSV parser */
    CsvParser* csv_parser = CsvParser_new_from_string(csv_data, NULL, is_header);

    return _sqlite_lua_from_csv_parser(L, self, table_name, csv_parser);
}

static int _sqlite_lua_from_csv_file(lua_State* L)
{
    /* Get parameters */
    lua_sqlite_t* self = luaL_checkudata(L, 1, AUTO_LUA_SQLITE);
    const char* table_name = luaL_checkstring(L, 2);
    const char* csv_file = luaL_checkstring(L, 3);
    int is_header = !(lua_type(L, 4) == LUA_TBOOLEAN && !lua_toboolean(L, 4));

    /* Create CSV parser */
    CsvParser* csv_parser = CsvParser_new(csv_file, NULL, is_header);

    return _sqlite_lua_from_csv_parser(L, self, table_name, csv_parser);
}

static int _sqlite_lua_to_csv_callback(void* arg, int argc, char** argv, char** azColName)
{
    int i;
    sqlite_to_csv_helper_t* helper = arg;
    int orig_line_cnt = helper->line_cnt++;

    /* Add header */
    if (orig_line_cnt == 0)
    {
        for (i = 0; i < argc; i++)
        {
            luaL_addstring(&helper->csv_buf, azColName[i]);
            luaL_addchar(&helper->csv_buf, ',');
        }
        luaL_buffsub(&helper->csv_buf, 1);
        luaL_addstring(&helper->csv_buf, "\r\n");
    }

    /* Add data */
    for (i = 0; i < argc; i++)
    {
        luaL_addstring(&helper->csv_buf, argv[i]);
        luaL_addchar(&helper->csv_buf, ',');
    }
    luaL_buffsub(&helper->csv_buf, 1);
    luaL_addstring(&helper->csv_buf, "\r\n");

    return 0;
}

static int _sqlite_lua_to_csv(lua_State* L)
{
    /* Get parameters */
    lua_sqlite_t* self = luaL_checkudata(L, 1, AUTO_LUA_SQLITE);
    const char* table_name = luaL_checkstring(L, 2);

    /* Construct SQL command. */
    lua_pushfstring(L, "SELECT * FROM %s", table_name);
    const char* sql_cmd = lua_tostring(L, -1);

    sqlite_to_csv_helper_t helper;
    helper.line_cnt = 0;
    luaL_buffinit(L, &helper.csv_buf);

    /* Do SQL command */
    char* errmsg = NULL;
    int ret = sqlite3_exec(self->db, sql_cmd, _sqlite_lua_to_csv_callback, &helper, &errmsg);

    /* There were extra line wrapper that need to delete. */
    luaL_buffsub(&helper.csv_buf, 2);
    luaL_pushresult(&helper.csv_buf);

    /* Remove SQL command. */
    lua_remove(L, -2);

    if (ret != SQLITE_OK)
    {
        lua_pushstring(L, errmsg);
        sqlite3_free(errmsg);
        return lua_error(L);
    }

    return 1;
}

static int _sqlite_lua_to_csv_file(lua_State* L)
{
    /* Get parameters. */
    const char* file_path = luaL_checkstring(L, 3);

    const char* mode = "a";
    if (lua_type(L, 4) == LUA_TSTRING)
    {
        mode = lua_tostring(L, 4);
        if (strcmp(mode, "a") != 0 && strcmp(mode, "w") != 0)
        {
            return luaL_error(L, "invalid mode: %s", mode);
        }
    }

    int errcode;
    FILE* csv_file;
#if defined(_MSC_VER)
    errcode = fopen_s(&csv_file, file_path, mode);
#else
    csv_file = fopen(file_path, mode);
    errcode = errno;
#endif

    if (csv_file == NULL)
    {
        char buf[1024];
        return luaL_error(L, "%s", auto_strerror(errcode, buf, sizeof(buf)));
    }

    /* Call sqlite:to_csv(). */
    lua_pushcfunction(L, _sqlite_lua_to_csv);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    if (lua_pcall(L, 2, 1, 0) != LUA_OK)
    {
        fclose(csv_file);
        return lua_error(L);
    }

    /* Write to file. */
    size_t csv_data_len = 0;
    const char* csv_data = luaL_tolstring(L, -1, &csv_data_len);
    size_t write_size = fwrite(csv_data, csv_data_len, 1, csv_file);
    fclose(csv_file);

    if (write_size != 1)
    {
        return luaL_error(L, "write to %s failed", file_path);
    }

    return 0;
}

static void _sqlite_init_metatable(lua_State* L)
{
    static const luaL_Reg s_sqlite_meta[] = {
        { "__gc",           _sqlite_lua_gc },
        { "__tostring",     _sqlite_lua_tostring },
        { NULL,             NULL },
    };
    static const luaL_Reg s_sqlite_method[] = {
        { "close",          _sqlite_lua_close },
        { "exec",           _sqlite_lua_exec },
        { "from_csv",       _sqlite_lua_from_csv },
        { "from_csv_file",  _sqlite_lua_from_csv_file },
        { "to_csv",         _sqlite_lua_to_csv },
        { "to_csv_file",    _sqlite_lua_to_csv_file },
        { NULL,             NULL },
    };
    if (luaL_newmetatable(L, AUTO_LUA_SQLITE) != 0)
    {
        luaL_setfuncs(L, s_sqlite_meta, 0);
        luaL_newlib(L, s_sqlite_method);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
}

int auto_lua_sqlite(lua_State* L)
{
    lua_sqlite_t* self = lua_newuserdata(L, sizeof(lua_sqlite_t));

    memset(self, 0, sizeof(*self));
    _sqlite_init_metatable(L);

    _sqlite_lua_parse_options(L, 1, self);

    if (sqlite3_open(self->config.filename, &self->db) != SQLITE_OK)
    {
        return luaL_error(L, "%s", sqlite3_errmsg(self->db));
    }

    return 1;
}
