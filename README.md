# Lua

A powerful, efficient, lightweight, embeddable scripting language.

## option

- LUA_32BITS:BOOL
- LUA_APICHECK:BOOL
- LUA_COMPAT:BOOL
- LUA_LTO:BOOL
- LUA_PIE:BOOL
- LUA_READLINE:BOOL
- LUA_USER_H:FILEPATH
- LUA_VERSION:STRING

## example

```sh
cmake -Bbuild -DLUA_USER_H="lua.config.h" -DLUA_SOURCES="isocline.c"
cmake --build build --config Release
```
