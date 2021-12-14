
#ifndef __LUA_MODULE_MODBUS_H__
#define __LUA_MODULE_MODBUS_H__

#include "lua.h"
#include "lib_modbus.h"
#include "modbus.h"
#include "modbus-private.h"
#include "modbus-tcp-private.h"

typedef struct modbus_session_s
{
	modbus_t *ctx;
	int slave;
} modbus_session_t;

extern int luaopen_modbus(lua_State *L);

#endif
