#if !defined lua_h
#include "lua.h"
#endif /* lua_h */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

#if !defined LUA_VERSION_NUM || (LUA_VERSION_NUM <= 501)
#define lua_rawsetp(L, idx, p) (lua_pushlightuserdata(L, p), lua_insert(L, -2), lua_rawset(L, idx))
#define lua_rawgetp(L, idx, p) (lua_pushlightuserdata(L, p), lua_rawget(L, idx))
#define lua_rawlen(L, i) lua_objlen(L, i)
#undef lua_readline
#undef lua_saveline
#undef lua_freeline
#endif /* LUA_VERSION_NUM */
#define lua_initreadline(L) lua_initline(L)
#define lua_readline(L, b, p) (((b) = readline(p)) != NULL)
#if defined(LUA_VERSION_NUM) && (LUA_VERSION_NUM > 502)
#define lua_saveline(L, line) add_history(line)
#else /* !LUA_VERSION_NUM */
#define lua_saveline(L, idx)    \
    if (lua_rawlen(L, idx) > 0) \
        add_history(lua_tostring(L, idx));
#endif /* LUA_VERSION_NUM */
#define lua_freeline(L, b) free(b)

static int lua_readline_(lua_State *L, int ret)
{
    if (ret)
    {
        lua_rawgetp(L, LUA_REGISTRYINDEX, (void *)(intptr_t)lua_readline_);
        return 1;
    }
    lua_rawsetp(L, LUA_REGISTRYINDEX, (void *)(intptr_t)lua_readline_);
    return 0;
}

static lua_State *rl_readline_L = NULL;
static int is_id(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
static void compentry_exec(char const *buffer, char const *suffix, char const *sep)
{
    char const *result = NULL;
    lua_State *L = rl_readline_L;
    if (suffix == NULL) { suffix = buffer; }
    for (char const *p = suffix; *p; ++p)
    {
        if (*p == '.' || *p == ':' || *p == '[')
        {
            result = p;
            break;
        }
    }
    if (result > suffix)
    {
        unsigned int fix = 0;
        lua_Integer integer = 0;
        if (result[-1] == ']')
        {
            ++fix; // 1]
            if (*suffix == '\'' || *suffix == '\"')
            {
                ++fix; // '1' "1"
                ++suffix;
            }
            else if (isdigit(*suffix))
            {
                integer = 1;
            }
        }
        lua_pushlstring(L, suffix, (size_t)(result - suffix) - fix);
        if (integer)
        {
            integer = lua_tointeger(L, -1);
            if (integer)
            {
                lua_pushinteger(L, integer);
                lua_remove(L, -2); // table, string, integer
            }
        }
        sep = result;
        suffix = result + 1;
        lua_gettable(L, -2); // table, key
        if (*suffix != '.' && *suffix != ':' && *suffix != '[')
        {
            if (lua_type(L, -1) == LUA_TTABLE)
            {
                compentry_exec(buffer, suffix, sep);
            }
            if (lua_getmetatable(L, -1))
            {
                lua_getfield(L, -1, "__index");
                lua_remove(L, -2); // table, value, meta, __index
                if (lua_type(L, -1) == LUA_TFUNCTION)
                {
                    if (strncmp(suffix, "__index", sizeof("__index") - 1) == 0)
                    {
                        int c = (int)suffix[sizeof("__index") - 1];
                        if (c == '.' || c == ':' || c == '[')
                        {
                            suffix += sizeof("__index");
                            sep += sizeof("__index");
                        }
                    }
                    lua_pushvalue(L, -2);
                    lua_pushstring(L, "__index");
                    lua_call(L, 2, 1);
                }
                lua_remove(L, -2); // table, value, __index
                if (lua_type(L, -1) == LUA_TTABLE)
                {
                    compentry_exec(buffer, suffix, sep);
                }
            }
        }
        lua_pop(L, 1);
        return;
    }

    void *ud;
    lua_Alloc alloc = lua_getallocf(L, &ud);
    size_t prefix_len = (size_t)((sep ? sep : suffix) - buffer);
    char *prefix = (char *)alloc(ud, NULL, LUA_TSTRING, prefix_len + 1);
    memcpy(prefix, buffer, prefix_len);
    prefix[prefix_len] = 0;

    if (*suffix == '\'' || *suffix == '\"')
    {
        ++suffix; // '1 "1
    }
    size_t suffix_len = strlen(suffix);
    if (suffix_len)
    {
        if (suffix[suffix_len - 1] == ']')
        {
            --suffix_len; // 1]
        }
        if (suffix[suffix_len - 1] == '\'' || suffix[suffix_len - 1] == '\"')
        {
            --suffix_len; // 1' 1"
        }
    }

    lua_createtable(L, 0, 0);
    lua_readline_(L, 0);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
    {
        if (lua_type(L, -2) == LUA_TSTRING)
        {
            int type = lua_type(L, -1);
            if (sep && *sep == ':' && type != LUA_TFUNCTION) { continue; }
            char const *key = lua_tostring(L, -2);
            if (strncmp(key, suffix, suffix_len) == 0)
            {
                union
                {
                    char const *p;
                    char *o;
                } s;
                lua_readline_(L, 1);
                if ((sep && *sep == '[') || (!is_id(*key) && sep && *sep))
                {
                    lua_pushfstring(L, "%s%s", key, key);
                    char const *k2 = lua_tostring(L, -1);
                    char const *k1 = key;
                    for (s.p = k2; *k1;)
                    {
                        if (*k1 == '\"') { *s.o++ = '\\'; }
                        *s.o++ = *k1++;
                    }
                    *s.o = 0;
                    lua_pushfstring(L, "%s[\"%s\"", prefix, k2);
                    lua_remove(L, -2); // key, value, k2, replacement
                }
                else if (sep && *sep)
                {
                    lua_pushlstring(L, sep, (size_t)(suffix - sep));
                    if (type == LUA_TFUNCTION)
                    {
                        lua_pushfstring(L, "%s%s%s(", prefix, lua_tostring(L, -1), key);
                    }
                    else
                    {
                        lua_pushfstring(L, "%s%s%s", prefix, lua_tostring(L, -1), key);
                    }
                    lua_remove(L, -2); // key, value, sep, replacement
                }
                else
                {
                    if (type == LUA_TFUNCTION)
                    {
                        lua_pushfstring(L, "%s%s(", prefix, key);
                    }
                    else
                    {
                        lua_pushfstring(L, "%s%s", prefix, key);
                    }
                }
                lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
                lua_pop(L, 1);
            }
        }
        else if (lua_type(L, -2) == LUA_TNUMBER)
        {
#if defined(LUA_VERSION_NUM) && (LUA_VERSION_NUM > 502)
            lua_pushfstring(L, "%I", lua_tointeger(L, -2));
#else /* !LUA_VERSION_NUM */
            lua_pushfstring(L, "%d", lua_tointeger(L, -2));
#endif /* LUA_VERSION_NUM */
            char const *key = lua_tostring(L, -1);
            if (strncmp(key, suffix, suffix_len) == 0)
            {
                lua_readline_(L, 1);
                lua_pushfstring(L, "%s[%s", prefix, key);
                lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }

    prefix = (char *)alloc(ud, prefix, prefix_len + 1, 0);
}

static char *compentry_func(char const *text, int state)
{
    lua_State *L = rl_readline_L;
    if (state == 0)
    {
#if defined(LUA_RIDX_LAST)
        lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_LAST);
#else /* !LUA_RIDX_LAST */
        lua_pushvalue(L, LUA_GLOBALSINDEX);
#endif /* LUA_RIDX_LAST */
        compentry_exec(text, text, 0);
        lua_pop(L, 1);
    }
    char *str = rl_filename_completion_function(text, state);
    lua_readline_(L, 1);
    if (lua_rawlen(L, -1) > (size_t)state)
    {
        lua_pushinteger(L, state + 1);
        lua_gettable(L, -2);
        str = strdup(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return str;
}

static char **completion_func(char const *text, int start, int end)
{
    (void)start, (void)end;
    rl_completion_append_character = 0;
    return rl_completion_matches(text, compentry_func);
}

static void lua_initline(lua_State *L)
{
    rl_readline_L = L;
    rl_readline_name = "lua";
    rl_attempted_completion_function = completion_func;
    rl_basic_word_break_characters = " \t\n`@$><=;|&{(";
}
