#if defined(lua_c)

#define LUA_USE_ISOCLINE

#if defined(LUA_USE_ISOCLINE)
#define lua_readline(L, b, p) ((void)L, ((b) = ic_readline(p)) != NULL)
#define lua_saveline(L, line) ((void)L, ic_history_add(line))
#define lua_freeline(L, b) ((void)L, free(b))
char *ic_readline(const char *prompt_text);
void ic_history_add(const char *entry);
void lua_initreadline(lua_State *L);
#endif /* LUA_USE_ISOCLINE */

#endif /* lua_c */
