# Lua

A powerful, efficient, lightweight, embeddable scripting language.
![ ](https://github.com/tqfx/lua/releases/download/assets/screenshot.gif)

## Options

- LUA_32BITS:BOOL
- LUA_APICHECK:BOOL
- LUA_COMPAT:BOOL
- LUA_IPO:BOOL
- LUA_PIE:BOOL
- LUA_ISOCLINE:BOOL
- LUA_READLINE:BOOL
- LUA_USER_H:FILEPATH
- LUA_VERSION:STRING

## Example

```c
/* lua.user.h */
#if defined(LUA_RELEASE)
#undef LUA_RELEASE
#define LUA_RELEASE "Lua 6.6.6"
#else /* !LUA_RELEASE */
#undef LUA_VERSION
#define LUA_VERSION "Lua 6.6.6"
#endif /* LUA_RELEASE */
```

```sh
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=. -DLUA_USER_H="lua.user.h"
cmake --build . --target install
bin/luac -v
```
