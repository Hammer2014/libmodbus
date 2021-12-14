/*
 * Lua/C closure implementing "libmodbus" library callbacks.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lib_modbus.h"

#include "lua_utils.h"

#define _U __attribute__((unused))

extern void print_table(lua_State *L);

static const uint8_t *l_modbus_from_hexascii(const char *hex, uint8_t *rawdata, size_t *len)
{
	size_t i=0;
	size_t max=*len;
	uint8_t c,d;
	size_t _l =strlen(hex);
	if( !_l || _l & 1 ) {
		return NULL;
	}
	while(hex && *hex) {
		c = (uint8_t) *hex++;
		c = (c > 0x3f) ? (c&0x0f)+9 : c&0x0f;
		d = (uint8_t) *hex++;
		d = (d > 0x3f) ? (d&0x0f)+9 : d&0x0f;
		// Abort if buffer overflows.
		if(i >= max) {
			return NULL;
		}
		rawdata[i++] = (uint8_t)(c<<4)|d;
	}
	*len = i;
	return rawdata;
}

static int l_modbus_to_hexascii(lua_State *L, const uint8_t *raw, size_t len)
{
	int o=0;
	uint8_t x;
	char *hex = alloca(2*len+1);
	for(size_t i=0;i<len;i++) {
		x = raw[i] >> 4;
		x = (x>9)?x+0x57:x+0x30;
		hex[o++] = (char)x;
		x = raw[i] & 0xf;
		x = (x>9)?x+0x57:x+0x30;
		hex[o++] = (char)x;
	}
	hex[o] = 0;
	lua_pushstring(L,hex);
	return 0;
}

static int l_modbus_error(lua_State *L) {
	lua_pushinteger(L, errno);
	return 1;
}

static int l_modbus_errmsg(lua_State *L) {
	int code = errno;
	if( lua_type(L,1) == LUA_TNUMBER ) {
		code = (int) lua_tointeger(L,1);
	}
	lua_pushstring(L, modbus_strerror(code));
	return 1;
}

static int l_modbus_read_registers(lua_State *L) {
	int i = 1;
	modbus_session_t *session;
#ifdef MODBUS_USE_CLOSURE
	if( lua_type(L,i) == LUA_TUSERDATA ) {
		session = luaL_checkudata(L, i++, "modbus");
	} else {
		session = lua_touserdata(L, lua_upvalueindex(1));
	}
#else
	session = luaL_checkudata(L, i++, "modbus");
#endif
	int reg = (int) (double) lua_tonumber(L, i++);
	int count = (int) (double) lua_tonumber(L, i++);
	uint16_t *result;
	result = malloc( sizeof(uint16_t) * (unsigned long) count);
	int rc = modbus_read_registers(session->ctx, reg, count, result);
	if( rc < 0 ) {
		lua_pushnil(L);
		free(result);
		return 1;
	} else {
		l_modbus_to_hexascii(L, (const uint8_t *)result, (size_t) rc*2);
		free(result);
		return 1;
	}
}

static int l_modbus_raw_request(lua_State *L)
{
	int i = 1;
	modbus_session_t *session;
#ifdef MODBUS_USE_CLOSURE
	if( lua_type(L,i) == LUA_TUSERDATA ) {
		session = luaL_checkudata(L, i++, "modbus");
	} else {
		session = lua_touserdata(L, lua_upvalueindex(1));
	}
#else
	session = luaL_checkudata(L, i++, "modbus");
#endif

	const char *hex = luaL_checkstring(L, i++);

	uint8_t req[MODBUS_MAX_ADU_LENGTH];
	uint8_t rsp[MODBUS_MAX_ADU_LENGTH];
	size_t len = MODBUS_MAX_ADU_LENGTH-1;
	int rc;

	req[0]=(uint8_t)session->slave;
	if( ! l_modbus_from_hexascii(hex, req+1, &len)) {
		lua_pushstring(L, "Invalid HEXASCII input.");
		lua_error(L);
	}

#if 0
	rc = modbus_send_raw_request(session->ctx, req, (int)len+1);
	if( rc < 0 ) {
		lua_pushnil(L);
		return 1;
	}

	rc = modbus_receive_confirmation(session->ctx, rsp);
	if( rc < 0 ) {
		lua_pushnil(L);
		return 1;
	}
#else
	rc = modbus_send_raw_request_ex(session->ctx, req, (int)len+1, rsp);
#endif

	if( rc > 0 ) {
		switch(session->ctx->backend->backend_type) {
			case _MODBUS_BACKEND_TYPE_RTU:
				if(rc > 1) {
					l_modbus_to_hexascii(L, rsp+1, (size_t)rc-1);
				} else {
					lua_pushnil(L);
				}
				break;
			case _MODBUS_BACKEND_TYPE_TCP:
			case _MODBUS_BACKEND_TYPE_UDP:
				if( rc > 7) {
					l_modbus_to_hexascii(L, rsp+7, (size_t)rc-7);
				} else {
					lua_pushnil(L);
				}
				break;
			default:
				l_modbus_to_hexascii(L, rsp, (size_t)rc);
				break;
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int l_modbus_get_response_timeout(lua_State *L) {
	modbus_session_t *session;
#ifdef MODBUS_USE_CLOSURE
	if( lua_type(L,1) == LUA_TUSERDATA ) {
		session = luaL_checkudata(L, 1, "modbus");
	} else {
		session = lua_touserdata(L, lua_upvalueindex(1));
	}
#else
	session = luaL_checkudata(L, 1, "modbus");
#endif

	uint32_t to_sec,to_usec;
	modbus_get_response_timeout(session->ctx, &to_sec, &to_usec);
	lua_pushinteger(L,to_sec);
	lua_pushinteger(L,to_usec);
	return 2;
}

static int l_modbus_set_response_timeout(lua_State *L) {
	int i = 1;
	modbus_session_t *session;
#ifdef MODBUS_USE_CLOSURE
	if( lua_type(L,i) == LUA_TUSERDATA ) {
		session = luaL_checkudata(L, i++, "modbus");
	} else {
		session = lua_touserdata(L, lua_upvalueindex(1));
	}
#else
	session = luaL_checkudata(L, i++, "modbus");
#endif

	uint32_t to_sec = (uint32_t) luaL_checkinteger(L, i++);
	uint32_t to_usec = (uint32_t) luaL_checkinteger(L, i++);
	lua_pushinteger(L,modbus_set_response_timeout(session->ctx, to_sec, to_usec));
	return 1;
}

static int l_modbus_close(lua_State *L) {
	modbus_session_t *session;
#ifdef MODBUS_USE_CLOSURE
	if( lua_type(L,1) == LUA_TUSERDATA ) {
		session = luaL_checkudata(L, 1, "modbus");
	} else {
		session = lua_touserdata(L, lua_upvalueindex(1));
	}
#else
	session = luaL_checkudata(L, 1, "modbus");
#endif
	if( session && session->ctx ) {
		modbus_close(session->ctx);
		modbus_free(session->ctx);
		// Mark the session as free'd to prevent a double free during garbage collection
		session->ctx = 0L;
	}
	return 0;
}

static int  l_modbus_meta_gc(lua_State *L)
{
	modbus_session_t *session = luaL_checkudata(L,1, "modbus");
	if( session->ctx ) {
		modbus_close(session->ctx);
		modbus_free(session->ctx);
		session->ctx = 0;
	}
	return 0;
}

static const char *l_modbus_typename(unsigned int t)
{
	const char *_type[] = { "rtu","tcp","udp" };
	if( t > 1 ) {
		return "unknown";
	} else {
		return _type[t];
	}
}

static const char *l_modbus_get_connection(modbus_t *m)
{
	static char str[128];
	if( m->backend->backend_type == _MODBUS_BACKEND_TYPE_TCP ) {
		modbus_tcp_t *t = (modbus_tcp_t *) m->backend_data;
		snprintf(str,128,"id=%d tcp=%s:%d",m->slave, t->ip, t->port);
	} else {
		snprintf(str,128,"file:%d",m->s);
	}
	return str;
}

static int l_modbus_meta_tostring(lua_State *L)
{
	modbus_session_t *session = luaL_checkudata(L,1, "modbus");

	char str[512];
	snprintf(str,512,"modbus_%s (%s)",l_modbus_typename(session->ctx->backend->backend_type),l_modbus_get_connection(session->ctx));
	lua_pushstring(L,str);
	return 1;
}

static int l_modbus_get_type(lua_State *L)
{
	modbus_session_t *session = luaL_checkudata(L,1, "modbus");

	const unsigned int _t = session->ctx->backend->backend_type;
	//lua_pushinteger(L, _t);
	lua_pushstring(L, l_modbus_typename(_t));

	return 1;
}

static int l_modbus_error_recovery(lua_State *L)
{
	modbus_session_t *session = luaL_checkudata(L,1, "modbus");
	int state = luaL_optinteger(L, 2, -1);

	if(modbus_set_error_recovery(session->ctx, state)) {
		lua_pushinteger(L,errno);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int l_modbus_flush(lua_State *L)
{
	modbus_session_t *session = luaL_checkudata(L,1, "modbus");
	int rc = modbus_flush(session->ctx);
	if(rc < 0 ) {
		lua_pushnil(L);
	} else {
		lua_pushinteger(L,rc);
	}
	return 1;
}

static int l_modbus_set_debug(lua_State *L)
{
	modbus_session_t *session = luaL_checkudata(L,1, "modbus");
	int flag = (int) luaL_optinteger(L,2,1);

	modbus_set_debug(session->ctx, flag);
	return 0;
}

static const luaL_Reg modbusapi[] = {
	{"error", l_modbus_error},
	{"errmsg", l_modbus_errmsg},
	{"read_registers", l_modbus_read_registers},
	{"raw_request", l_modbus_raw_request},
	{"get_timeout", l_modbus_get_response_timeout},
	{"set_timeout", l_modbus_set_response_timeout},
	{"set_error_recovery", l_modbus_error_recovery},
	{"type", l_modbus_get_type},
	{"close", l_modbus_close},
	{"flush", l_modbus_flush},
	{"set_debug", l_modbus_set_debug},
	{NULL, NULL}
};

static const luaL_Reg modbusmetaapi[] = {
	{"__gc", l_modbus_meta_gc},
	{"__tostring", l_modbus_meta_tostring},
	{NULL, NULL}
};

static int l_modbus_bind_driver(lua_State *L, void *ctx, int slave)
{
	modbus_session_t *session = lua_newuserdata(L,sizeof(modbus_session_t));
	// Bind metadata - if luaL_getmetatable() fails, create a new table
	luaL_getmetatable(L, "modbus");
	if( lua_isnil(L,-1)) {
		// Remove nil response
		lua_pop(L,1);
		// Start a new metatable
		luaL_newmetatable(L, "modbus");
		// Push a copy of the session pointer and register the closures
#if defined(LUAJIT) || LUA_VERSION_NUM > 501
		luaL_setfuncs(L, modbusmetaapi, 0);
#else
		luaL_register(L, 0, modbusmetaapi);
#endif
		lua_newtable(L);
#ifdef MODBUS_USE_CLOSURE
#if defined(LUAJIT) || LUA_VERSION_NUM > 501
		// New way of pushing closures
		lua_pushlightuserdata(L, session);
		luaL_setfuncs(L, modbusapi, 1);
#else
		// Old way of pushing closures
		for(size_t i=0; modbusapi[i].name; i++) {
			lua_pushlightuserdata(L, session);
			lua_pushcclosure(L, modbusapi[i].func,1);
			lua_setfield(L,-2,modbusapi[i].name);
		}
#endif
#else
#if defined(LUAJIT) || LUA_VERSION_NUM > 501
		luaL_setfuncs(L, modbusapi, 0);
#else
		luaL_register(L, 0, modbusapi);
#endif
#endif
		lua_setfield(L,-2,"__index");
	}
	// Use this to set the metatable of the new usernata
	lua_setmetatable(L, -2);

	// Finish setting up the userdata context.
	session->ctx = ctx;
	session->slave = slave;
	return 1;
}

static int l_modbusinit(lua_State *L _U) {
	return 0;
}

static int l_modbus_connect_tcp(lua_State *L) {
	const char *address = luaL_checkstring(L, 1);
	int port = (int) luaL_checkinteger(L,2);
	int slave = (int) luaL_checkinteger(L,3);

	modbus_t *ctx = modbus_new_tcp( address, port );
	if( ctx ) {
		if( slave && ( modbus_set_slave(ctx, slave) < 0 )) {
			lua_pushstring(L, "Invalid Slave ID.");
			lua_error(L);
		} else
			if( modbus_connect(ctx) < 0 && errno != EINPROGRESS ) {
				perror("modbus_connect");
				modbus_free(ctx);
				lua_pushnil(L);
				lua_pushstring(L, strerror(errno));
				return 2;
			} else {
				return l_modbus_bind_driver(L, ctx, slave);
			}
	}
	lua_pushstring(L, "Failed to allocation modbus context.");
	lua_error(L);
	return 0;
}

static int l_modbus_connect_udp(lua_State *L) {
	const char *address = luaL_checkstring(L, 1);
	int port = (int) luaL_checkinteger(L,2);
	int slave = (int) luaL_checkinteger(L,3);

	modbus_t *ctx = modbus_new_udp( address, port );
	if( ctx ) {
		if( slave && ( modbus_set_slave(ctx, slave) < 0 )) {
			lua_pushstring(L, "Invalid Slave ID.");
			lua_error(L);
		} else
			if( modbus_connect(ctx) < 0 && errno != EINPROGRESS ) {
				perror("modbus_connect");
				modbus_free(ctx);
				lua_pushnil(L);
				lua_pushstring(L, strerror(errno));
				return 2;
			} else {
				return l_modbus_bind_driver(L, ctx, slave);
			}
	}
	lua_pushstring(L, "Failed to allocation modbus context.");
	lua_error(L);
	return 0;
}

static int l_modbus_connect_rtu(lua_State *L) {
	const char *port = luaL_checkstring(L, 1);
	int baud = (int) luaL_checkinteger(L,2);
	const char *parity = luaL_checkstring(L,3);
	int dbits = (int) luaL_checkinteger(L,4);
	int sbits = (int) luaL_checkinteger(L,5);
	int slave = (int) luaL_checkinteger(L,6);

	modbus_t *ctx = modbus_new_rtu( port, baud, parity[0], dbits, sbits );

	if( ctx ) {
		if( modbus_set_slave(ctx, slave) < 0 ) {
			lua_pushstring(L, "Invalid Slave ID.");
			lua_error(L);
		}
		if( modbus_connect(ctx) < 0 ) {
			modbus_free(ctx);
			lua_pushnil(L);
			lua_pushstring(L, strerror(errno));
			return 2;
		} else {
			return l_modbus_bind_driver(L, ctx, slave);
		}
	}
	lua_pushstring(L, "Failed to allocation modbus context.");
	lua_error(L);
	return 0;
}

static const luaL_Reg modbuslib[] = {
	{"init", l_modbusinit},
	{"error", l_modbus_error },
	{"errmsg", l_modbus_errmsg },
	{"connect_tcp", l_modbus_connect_tcp},
	{"connect_udp", l_modbus_connect_udp},
	{"connect_rtu", l_modbus_connect_rtu},
	{NULL, NULL}
};

int luaopen_modbus(lua_State *L)
{
	luaL_newlib(L, modbuslib);
	return 1;
}
