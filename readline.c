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
static char const *keywords[] = {
    "and", "break", "do", "else", "elseif", "end",
    "false", "for", "function", "goto", "if", "in",
    "local", "nil", "not", "or", "repeat", "return",
    "then", "true", "until", "while", NULL};
static lua_State *rl_readline_L = NULL;
static int is_id(char const *str)
{
    int c = (int)*str;
    if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && c != '_') { return 0; }
    for (++str, c = (int)*str; c; ++str, c = (int)*str)
    {
        if ((c < '0' || c > '9') && (c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && c != '_') { return 0; }
    }
    return 1;
}
static int match(char const *str, char const *sub, size_t const len)
{
    char const *const suf = sub + len;
    if (str[0] == '_' && str[1] == '_')
    {
        if (sub[0] != '_') { return 0; }
        if (sub[1] == '_' && str[2] && sub[2] && str[2] != sub[2]) { return 0; }
    }
    if (str[0] && sub[0] && str[0] != sub[0]) { return 0; }
    for (; *str && sub < suf; ++sub)
    {
        if (*str != *sub)
        {
            for (;;)
            {
                if (*++str == 0) { return 0; }
                if (*str == *sub)
                {
                    ++str;
                    break;
                }
            }
        }
        else { ++str; }
    }
    return sub == suf;
}
static void compentry_exec(char const *buffer, char const *suffix, char const *sep)
{
    char const *subfix = NULL;
    lua_State *L = rl_readline_L;
    if (suffix == NULL) { suffix = buffer; }
    for (char const *s = suffix; *s; ++s)
    {
        if (*s == '.' || *s == ':' || *s == '[')
        {
            subfix = s;
            break;
        }
    }
    if (subfix > suffix)
    {
        unsigned int fix = 0;
        lua_Integer integer = 0;
        if (subfix[-1] == ']')
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
        size_t key_len = (size_t)(subfix - suffix);
        if (key_len >= fix) { key_len -= fix; }
        lua_pushlstring(L, suffix, key_len);
        if (integer)
        {
            integer = lua_tointeger(L, -1);
            if (integer)
            {
                lua_pushinteger(L, integer);
                lua_remove(L, -2); // table, string, integer
            }
        }
        sep = subfix;
        suffix = subfix + 1;
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
                    lua_pcall(L, 2, 1, 0);
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
    lua_rawsetp(L, LUA_REGISTRYINDEX, (void *)&rl_readline_L);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
    {
        if (lua_type(L, -2) == LUA_TSTRING)
        {
            int type = lua_type(L, -1);
            if (sep && *sep == ':' && type != LUA_TFUNCTION) { continue; }
            char const *key = lua_tostring(L, -2);
            if (match(key, suffix, suffix_len))
            {
                union
                {
                    char const *p;
                    char *o;
                } s;
                lua_rawgetp(L, LUA_REGISTRYINDEX, (void *)&rl_readline_L);
                if ((sep && *sep == '[') || (!is_id(key) && sep && *sep))
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
            if (match(key, suffix, suffix_len))
            {
                lua_rawgetp(L, LUA_REGISTRYINDEX, (void *)&rl_readline_L);
                lua_pushfstring(L, "%s[%s", prefix, key);
                lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }

    if (!sep)
    {
        lua_rawgetp(L, LUA_REGISTRYINDEX, (void *)&rl_readline_L);
        for (char const **s = keywords; *s; ++s)
        {
            if (match(*s, suffix, suffix_len))
            {
                lua_pushstring(L, *s);
                lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
            }
        }
        lua_pop(L, 1);
    }

    alloc(ud, prefix, prefix_len + 1, 0);
}

static char *compentry_func(char const *text, int state)
{
    lua_State *L = rl_readline_L;
    if (state == 0)
    {
#if defined(LUA_RIDX_GLOBALS)
        lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
#else /* !LUA_RIDX_GLOBALS */
        lua_pushvalue(L, LUA_GLOBALSINDEX);
#endif /* LUA_RIDX_GLOBALS */
        compentry_exec(text, text, 0);
        lua_pop(L, 1);
    }
    char *str = rl_filename_completion_function(text, state);
    lua_rawgetp(L, LUA_REGISTRYINDEX, (void *)&rl_readline_L);
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
