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

static int lua_readline(lua_State *L, int ret) {
    if (ret) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, (void *)(intptr_t)lua_readline);
        return 1;
    }
    lua_rawsetp(L, LUA_REGISTRYINDEX, (void *)(intptr_t)lua_readline);
    return 0;
}

static lua_State *rl_State = NULL;
static void compentry_exec(const char *buffer, const char *suffix, int sep) {
    lua_State *L = rl_State;
    if (suffix == NULL) {
        suffix = buffer;
    }

    const char *result = str_suffix(suffix, ".:[");
    if (result) {
        lua_pushlstring(L, suffix, result - suffix);
        lua_gettable(L, -2);
        suffix = result + 1;
        sep = *result;
        char **ret = NULL;
        if (lua_istable(L, -1) && *suffix != '.' && *suffix != ':') {
            compentry_exec(buffer, suffix, sep);
        }
        lua_pop(L, 1);
        return;
    }

    void *ud;
    lua_Alloc alloc = lua_getallocf(L, &ud);
    size_t prefix_len = suffix - buffer - (sep ? 1 : 0);
    char *prefix = (char *)alloc(ud, NULL, LUA_TSTRING, prefix_len + 1);
    memcpy(prefix, buffer, prefix_len);
    prefix[prefix_len] = 0;

    size_t suffix_len = strlen(suffix);
    if (suffix_len && (suffix[suffix_len - 1] == '\'' || suffix[suffix_len - 1] == '\"')) {
        suffix_len = suffix_len - 1;
    }
    if (*suffix == '\'' || *suffix == '\"') {
        suffix = suffix + 1;
    }

    lua_createtable(L, 0, 0);
    lua_readline(L, 0);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
        size_t key_len;
        if (lua_type(L, -2) == LUA_TSTRING) {
            if (sep == ':' && lua_type(L, -1) != LUA_TFUNCTION) {
                continue;
            }
            const char *key = lua_tolstring(L, -2, &key_len);
            key_len = key_len < suffix_len ? key_len : suffix_len;
            if (strncmp(suffix, key, key_len) == 0) {
                lua_readline(L, 1);
                if (isdigit(*key) || sep == '[') {
                    lua_pushfstring(L, "%s[\'%s\'", prefix, key);
                } else if (sep) {
                    if (lua_type(L, -1) == LUA_TFUNCTION) {
                        lua_pushfstring(L, "%s%c%s(", prefix, sep, key);
                    } else {
                        lua_pushfstring(L, "%s%c%s", prefix, sep, key);
                    }
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
        } else if (lua_type(L, -2) == LUA_TNUMBER && sep == '[') {
            lua_pushfstring(L, "%I", lua_tointeger(L, -2));
            const char *key = lua_tolstring(L, -1, &key_len);
            key_len = key_len < suffix_len ? key_len : suffix_len;
            if (strncmp(suffix, key, key_len) == 0) {
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
    lua_State *L = rl_State;
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

void lua_initreadline(lua_State *L) {
    rl_State = L;
    rl_readline_name = "lua";
    rl_attempted_completion_function = completion_func;
}
