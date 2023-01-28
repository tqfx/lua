# Lua

A powerful, efficient, lightweight, embeddable scripting language.

## option

- LUA_32BITS:BOOL
- LUA_APICHECK:BOOL
- LUA_COMPAT:BOOL
- LUA_LTO:BOOL
- LUA_PIE:BOOL
- LUA_ISOCLINE:BOOL
- LUA_READLINE:BOOL
- LUA_USER_H:FILEPATH
- LUA_VERSION:STRING

## example

```c
/* lua.config.h */
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
cmake .. -DCMAKE_INSTALL_PREFIX=install -DLUA_USER_H="lua.config.h"
cmake --build . --target install
install/bin/luac -v
```
