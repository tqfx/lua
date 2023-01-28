#if !defined LUA_VERSION_NUM || (LUA_VERSION_NUM <= 501)
#define lua_rawsetp(L, idx, p) (lua_pushlightuserdata(L, p), lua_insert(L, -2), lua_rawset(L, idx))
#define lua_rawgetp(L, idx, p) (lua_pushlightuserdata(L, p), lua_rawget(L, idx))
#define lua_rawlen(L, i) lua_objlen(L, i)
#endif /* LUA_VERSION_NUM */

#if defined(lua_c)

#if defined(LUA_ISOCLINE)
#if !defined LUA_VERSION_NUM || (LUA_VERSION_NUM <= 501)
#undef lua_readline
#undef lua_saveline
#undef lua_freeline
#endif /* LUA_VERSION_NUM */
void lua_initline(lua_State *L);
void ic_history_add(const char *entry);
char *ic_readline(const char *prompt_text);
#define lua_initreadline(L) lua_initline(L)
#define lua_readline(L, b, p) ((void)L, ((b) = ic_readline(p)) != NULL)
#if defined(LUA_VERSION_NUM) && (LUA_VERSION_NUM > 502)
#define lua_saveline(L, line) ((void)L, ic_history_add(line))
#else /* !LUA_VERSION_NUM */
#define lua_saveline(L, idx)    \
    if (lua_rawlen(L, idx) > 0) \
        ic_history_add(lua_tostring(L, idx));
#endif /* LUA_VERSION_NUM */
#define lua_freeline(L, b) ((void)L, free(b))
#endif /* LUA_ISOCLINE */

#if defined(LUA_READLINE)
#include <readline/readline.h>
#include <readline/history.h>
#if !defined LUA_VERSION_NUM || (LUA_VERSION_NUM <= 501)
#undef lua_readline
#undef lua_saveline
#undef lua_freeline
#endif /* LUA_VERSION_NUM */
void lua_initline(lua_State *L);
#define lua_initreadline(L) lua_initline(L)
#define lua_readline(L, b, p) ((void)L, ((b) = readline(p)) != NULL)
#if defined(LUA_VERSION_NUM) && (LUA_VERSION_NUM > 502)
#define lua_saveline(L, line) ((void)L, add_history(line))
#else /* !LUA_VERSION_NUM */
#define lua_saveline(L, idx)    \
    if (lua_rawlen(L, idx) > 0) \
        add_history(lua_tostring(L, idx));
#endif /* LUA_VERSION_NUM */
#define lua_freeline(L, b) ((void)L, free(b))
#endif /* LUA_READLINE */

#endif /* lua_c */
