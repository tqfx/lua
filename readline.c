#include "lua.h"
#include <stdio.h>
#include <ctype.h>
#include <readline/readline.h>

static const char *str_suffix(const char *buffer, const char *sep) {
    for (; *buffer; ++buffer) {
        if (strchr(sep, *buffer)) {
            return buffer;
        }
    }
    return NULL;
}

static int char_is_luaid(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int lua_readline(lua_State *L, int ret) {
    if (ret) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, (void *)(intptr_t)lua_readline);
        return 1;
    }
    lua_rawsetp(L, LUA_REGISTRYINDEX, (void *)(intptr_t)lua_readline);
    return 0;
}

static lua_State *rl_L = NULL;
static void compentry_exec(const char *buffer, const char *suffix, const char *sep) {
    lua_State *L = rl_L;
    if (suffix == NULL) {
        suffix = buffer;
    }

    const char *result = str_suffix(suffix, ".:[");
    if (result > suffix) {
        size_t fix = 0;
        lua_Integer integer = 0;
        if (result[-1] == ']') {
            ++fix; // 1]
            if (*suffix == '\'' || *suffix == '\"') {
                ++fix; // '1' "1"
                ++suffix;
            } else if (isdigit(*suffix)) {
                integer = 1;
            }
        }
        lua_pushlstring(L, suffix, result - suffix - fix);
        if (integer) {
            integer = lua_tointeger(L, -1);
            if (integer) {
                lua_pushinteger(L, integer);
                lua_remove(L, -2); // table, string, integer
            }
        }
        sep = result;
        suffix = result + 1;
        lua_gettable(L, -2); // table, key
        if (*suffix != '.' && *suffix != ':' && *suffix != '[') {
            if (lua_type(L, -1) == LUA_TTABLE) {
                compentry_exec(buffer, suffix, sep);
            }
            if (lua_getmetatable(L, -1)) {
                lua_getfield(L, -1, "__index");
                lua_remove(L, -2); // table, value, meta, __index
                if (lua_type(L, -1) == LUA_TFUNCTION) {
                    lua_pushvalue(L, -2);
                    lua_pushstring(L, "__index");
                    lua_call(L, 2, 1);
                }
                lua_remove(L, -2); // table, value, __index
                if (lua_type(L, -1) == LUA_TTABLE) {
                    compentry_exec(buffer, suffix, sep);
                }
            }
        }
        lua_pop(L, 1);
        return;
    }

    void *ud;
    lua_Alloc alloc = lua_getallocf(L, &ud);
    size_t prefix_len = (sep ? sep : suffix) - buffer;
    char *prefix = (char *)alloc(ud, NULL, LUA_TSTRING, prefix_len + 1);
    memcpy(prefix, buffer, prefix_len);
    prefix[prefix_len] = 0;

    if (*suffix == '\'' || *suffix == '\"') {
        ++suffix; // '1 "1
    }
    size_t suffix_len = strlen(suffix);
    if (suffix_len && suffix[suffix_len - 1] == ']') {
        --suffix_len; // 1]
    }
    if (suffix_len && strchr("\'\"", suffix[suffix_len - 1])) {
        --suffix_len; // 1' 1"
    }

    lua_createtable(L, 0, 0);
    lua_readline(L, 0);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            if (sep && *sep == ':' && lua_type(L, -1) != LUA_TFUNCTION) {
                continue;
            }
            const char *key = lua_tostring(L, -2);
            if (strncmp(key, suffix, suffix_len) == 0) {
                lua_readline(L, 1);
                if (!char_is_luaid(*key) && sep && *sep == '.') {
                    if (strchr(key, '\'')) {
                        lua_pushfstring(L, "%s[\"%s\"", prefix, key);
                    } else {
                        lua_pushfstring(L, "%s[\'%s\'", prefix, key);
                    }
                } else if (!char_is_luaid(*key) && sep && *sep == '[') {
                    if (strchr(key, '\'') || sep[1] == '\"') {
                        lua_pushfstring(L, "%s[\"%s\"", prefix, key);
                    } else {
                        lua_pushfstring(L, "%s[\'%s\'", prefix, key);
                    }
                } else if (sep && *sep == '[' && sep[1] == '\"') {
                    lua_pushfstring(L, "%s[\"%s\"", prefix, key);
                } else if (sep && *sep == '[') {
                    lua_pushfstring(L, "%s[\'%s\'", prefix, key);
                } else if (sep && *sep) {
                    lua_pushlstring(L, sep, suffix - sep);
                    if (lua_type(L, -2) == LUA_TFUNCTION) {
                        lua_pushfstring(L, "%s%s%s(", prefix, lua_tostring(L, -1), key);
                    } else {
                        lua_pushfstring(L, "%s%s%s", prefix, lua_tostring(L, -1), key);
                    }
                    lua_remove(L, -2); // key, value, sep, replacement
                } else {
                    if (lua_type(L, -1) == LUA_TFUNCTION) {
                        lua_pushfstring(L, "%s%s(", prefix, key);
                    } else {
                        lua_pushfstring(L, "%s%s", prefix, key);
                    }
                }
                lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
                lua_pop(L, 1);
            }
        } else if (lua_type(L, -2) == LUA_TNUMBER) {
            lua_pushfstring(L, "%I", lua_tointeger(L, -2));
            const char *key = lua_tostring(L, -1);
            if (strncmp(key, suffix, suffix_len) == 0) {
                lua_readline(L, 1);
                lua_pushfstring(L, "%s%c%s", prefix, sep, key);
                lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }

    prefix = (char *)alloc(ud, prefix, prefix_len + 1, 0);
}

char *compentry_func(const char *text, int state) {
    lua_State *L = rl_L;
    if (state == 0) {
#if defined(LUA_RIDX_LAST)
        lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_LAST);
#else /* !LUA_RIDX_LAST */
        lua_pushvalue(L, LUA_GLOBALSINDEX);
#endif /* LUA_RIDX_LAST */
        compentry_exec(text, text, 0);
        lua_pop(L, 1);
    }
    char *ret = NULL;
    lua_readline(L, 1);
    if (lua_rawlen(L, -1) > state) {
        lua_geti(L, -1, state + 1);
        ret = strdup(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return ret;
}

char **completion_func(const char *text, int start, int end) {
    rl_completion_suppress_append = 1;
    return rl_completion_matches(text, compentry_func);
}

void lua_initline(lua_State *L) {
    rl_L = L;
    rl_readline_name = "lua";
    rl_attempted_completion_function = completion_func;
}