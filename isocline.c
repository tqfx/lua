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

static void completion_exec(ic_completion_env_t *cenv, const char *buffer, const char *suffix, int sep) {
    lua_State *L = ic_completion_arg(cenv);
    if (suffix == NULL) {
        suffix = buffer;
    }

    const char *result = str_suffix(suffix, ".:[");
    if (result) {
        lua_pushlstring(L, suffix, result - suffix);
        lua_gettable(L, -2);
        suffix = result + 1;
        sep = *result;
        if (lua_istable(L, -1) && *suffix != '.' && *suffix != ':') {
            completion_exec(cenv, buffer, suffix, sep);
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

    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
        size_t key_len;
        if (lua_type(L, -2) == LUA_TSTRING) {
            if (sep == ':' && lua_type(L, -1) != LUA_TFUNCTION) {
                continue;
            }
            const char *key = lua_tolstring(L, -2, &key_len);
            key_len = key_len < suffix_len ? key_len : suffix_len;
            if (strncmp(suffix, key, key_len) == 0) {
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
                ic_add_completion_ex(cenv, lua_tostring(L, -1), key, NULL);
                lua_pop(L, 1);
            }
        } else if (lua_type(L, -2) == LUA_TNUMBER && sep == '[') {
            lua_pushfstring(L, "%I", lua_tointeger(L, -2));
            const char *key = lua_tolstring(L, -1, &key_len);
            key_len = key_len < suffix_len ? key_len : suffix_len;
            if (strncmp(suffix, key, key_len) == 0) {
                lua_pushfstring(L, "%s%c%s", prefix, sep, key);
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
    return len > 0 && (isalnum(*s) || *s == '_' || *s == '.' || *s == ':' || *s == '[' || *s == '\'' || *s == '\"');
}

static void completer(ic_completion_env_t *cenv, const char *buffer) {
    ic_complete_word(cenv, buffer, completion_fun, char_is_block);
    ic_complete_filename(cenv, buffer, 0, NULL, NULL);
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

    lua_State *L = (lua_State *)arg;
#if defined(LUA_RIDX_LAST)
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_LAST);
#else /* !LUA_RIDX_LAST */
    lua_pushvalue(L, LUA_GLOBALSINDEX);
#endif /* LUA_RIDX_LAST */

    size_t typen = 1;
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            ++typen;
        }
    }

    void *ud;
    lua_Alloc alloc = lua_getallocf(L, &ud);
    const char **types = (const char **)alloc(ud, NULL, 0, sizeof(char **) * typen);

    size_t typei = 0;
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            types[typei++] = lua_tostring(L, -2);
        }
    }
    types[typei] = NULL;
    lua_pop(L, 1);

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

    types = (const char **)alloc(ud, (void *)types, 0, 0);
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

void lua_initreadline(lua_State *L) {
    ic_set_default_completer(completer, L);
    ic_set_default_highlighter(highlighter, L);
    ic_set_prompt_marker("", "");
    loadhistory(".lua_history");
}
