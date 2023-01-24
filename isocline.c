#include "isocline/include/isocline.h"
#include "isocline/src/isocline.c"
#include "lua.h"

static void icGetCompletions(ic_completion_env_t *cenv, const char *buffer) {
    lua_State *L = ic_completion_arg(cenv);
#if defined(LUA_RIDX_LAST)
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_LAST);
#else /* !LUA_RIDX_LAST */
    lua_pushvalue(L, LUA_GLOBALSINDEX);
#endif /* LUA_RIDX_LAST */

    if (!strchr(buffer, '.') && !strchr(buffer, ':')) {
        size_t length = strlen(buffer);
        for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
            if (lua_type(L, -2) == LUA_TSTRING) {
                size_t len = 0;
                const char *str = lua_tolstring(L, -2, &len);
                len = len < length ? len : length;
                if (strncmp(buffer, str, len) == 0) {
                    ic_add_completion(cenv, str);
                }
            }
        }
    }

    lua_pop(L, 1);
}

static bool isMethodOrFunctionChar(const char *s, long len) {
    return len == 1 && (isalnum(*s) || *s == '.' || *s == ':' || *s == '_');
}

static void completer(ic_completion_env_t *cenv, const char *buffer) {
    ic_complete_word(cenv, buffer, icGetCompletions, isMethodOrFunctionChar);
}

static void highlighter(ic_highlight_env_t *henv, const char *input, void *arg) {
    static const char *keywords[] = {"and", "false", "function", "in", "local", "nil", "not", "or", "true", NULL};
    static const char *controls[] = {"break", "do", "else", "elseif", "end", "for", "goto", "if", "repeat", "return", "then", "until", "while", NULL};

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
        if ((tlen = ic_match_any_token(input, i, &ic_char_is_idletter, keywords)) > 0) {
            ic_highlight(henv, i, tlen, "keyword");
            i += tlen;
        } else if ((tlen = ic_match_any_token(input, i, &ic_char_is_idletter, controls)) > 0) {
            ic_highlight(henv, i, tlen, "plum");
            i += tlen;
        } else if ((tlen = ic_match_any_token(input, i, &ic_char_is_idletter, types)) > 0) {
            ic_highlight(henv, i, tlen, "type");
            i += tlen;
        } else if ((tlen = ic_is_token(input, i, &ic_char_is_digit)) > 0) {
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

    types = (const char **)alloc(ud, types, 0, 0);
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
