#include "isocline/src/isocline.c"
#include "lua.h"

#include <ctype.h>

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

static void completion_exec(ic_completion_env_t *cenv, const char *buffer, const char *suffix, const char *sep) {
    lua_State *L = ic_completion_arg(cenv);
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
                completion_exec(cenv, buffer, suffix, sep);
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
                    completion_exec(cenv, buffer, suffix, sep);
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

    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            int type = lua_type(L, -1);
            if (sep && *sep == ':' && type != LUA_TFUNCTION) {
                continue;
            }
            const char *key = lua_tostring(L, -2);
            if (strncmp(key, suffix, suffix_len) == 0) {
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
                    if (type == LUA_TFUNCTION) {
                        lua_pushfstring(L, "%s%s%s(", prefix, lua_tostring(L, -1), key);
                    } else {
                        lua_pushfstring(L, "%s%s%s", prefix, lua_tostring(L, -1), key);
                    }
                    lua_remove(L, -2); // key, value, sep, replacement
                } else {
                    if (type == LUA_TFUNCTION) {
                        lua_pushfstring(L, "%s%s(", prefix, key);
                    } else {
                        lua_pushfstring(L, "%s%s", prefix, key);
                    }
                }
                ic_add_completion_ex(cenv, lua_tostring(L, -1), key, NULL);
                lua_pop(L, 1);
            }
        } else if (lua_type(L, -2) == LUA_TNUMBER) {
            lua_pushfstring(L, "%I", lua_tointeger(L, -2));
            const char *key = lua_tostring(L, -1);
            if (strncmp(key, suffix, suffix_len) == 0) {
                lua_pushfstring(L, "%s[%s", prefix, key);
                ic_add_completion_ex(cenv, lua_tostring(L, -1), key, NULL);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }

    prefix = (char *)alloc(ud, prefix, prefix_len + 1, 0);
}

static void completion_fun(ic_completion_env_t *cenv, const char *buffer) {
    lua_State *L = ic_completion_arg(cenv);
#if defined(LUA_RIDX_LAST)
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_LAST);
#else /* !LUA_RIDX_LAST */
    lua_pushvalue(L, LUA_GLOBALSINDEX);
#endif /* LUA_RIDX_LAST */
    completion_exec(cenv, buffer, buffer, 0);
    lua_pop(L, 1);
}

static bool char_is_block(const char *s, long len) {
    return len > 0 && (isalnum(*s) || strchr("_.:[\'\"\\]", *s));
}

static void completer(ic_completion_env_t *cenv, const char *buffer) {
    ic_complete_word(cenv, buffer, completion_fun, char_is_block);
    if (strchr(buffer, '\'') || strchr(buffer, '\"')) {
        ic_complete_filename(cenv, buffer, 0, NULL, NULL);
    }
}

static void highlighter(ic_highlight_env_t *henv, const char *input, void *arg) {
    static const char *keywords[] = {
        "and",
        "false",
        "function",
        "in",
        "local",
        "nil",
        "not",
        "or",
        "true",
        NULL,
    };
    static const char *controls[] = {
        "break",
        "do",
        "else",
        "elseif",
        "end",
        "for",
        "goto",
        "if",
        "repeat",
        "return",
        "then",
        "until",
        "while",
        NULL,
    };
    static const char *types[] = {
        NULL,
    };
    long len = (long)strlen(input);
    for (long i = 0; i < len;) {
        long tlen; // token length
        if ((tlen = ic_match_any_token(input, i, ic_char_is_idletter, keywords)) > 0) {
            ic_highlight(henv, i, tlen, "keyword");
            i += tlen;
        } else if ((tlen = ic_match_any_token(input, i, ic_char_is_idletter, controls)) > 0) {
            ic_highlight(henv, i, tlen, "plum");
            i += tlen;
        } else if ((tlen = ic_match_any_token(input, i, ic_char_is_idletter, types)) > 0) {
            ic_highlight(henv, i, tlen, "type");
            i += tlen;
        } else if ((tlen = ic_is_token(input, i, ic_char_is_digit)) > 0) {
            ic_highlight(henv, i, tlen, "number");
            i += tlen;
        } else if (ic_starts_with(input + i, "--")) { // line comment
            tlen = 2;
            while (i + tlen < len && input[i + tlen] != '\n') {
                tlen++;
            }
            ic_highlight(henv, i, tlen, "comment");
            i += tlen;
        } else {
            ic_highlight(henv, i, 1, NULL);
            i++;
        }
    }
}

static void joinpath(void *buff, const char *path, const char *name) {
    char *p = (char *)buff;
    while (*path) {
        if (*path != '\\') {
            *p++ = *path;
        } else {
            *p++ = '/';
        }
        ++path;
    }
    if (p > (char *)buff && p[-1] != '/') {
        *p++ = '/';
    }
    while (*name) {
        if (*name != '\\') {
            *p++ = *name;
        } else {
            *p++ = '/';
        }
        ++name;
    }
    *p = 0;
}

static void loadhistory(const char *name) {
    char path[260 + 1];

    const char *home = getenv("HOME");
    if (!home) {
        home = getenv("USERPROFILE");
    }
    if (home) {
        joinpath(path, home, name);
        ic_set_history(path, -1);
    }
}

void lua_initline(lua_State *L) {
    ic_set_default_completer(completer, L);
    ic_set_default_highlighter(highlighter, L);
    ic_set_prompt_marker("", "");
    loadhistory(".lua_history");
}
