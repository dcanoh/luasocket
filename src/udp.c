/*=========================================================================*\
* UDP object 
*
* RCS ID: $Id$
\*=========================================================================*/
#include <string.h> 

#include <lua.h>
#include <lauxlib.h>

#include "luasocket.h"

#include "aux.h"
#include "inet.h"
#include "udp.h"

/*=========================================================================*\
* Internal function prototypes
\*=========================================================================*/
static int udp_global_create(lua_State *L);
static int udp_meth_send(lua_State *L);
static int udp_meth_sendto(lua_State *L);
static int udp_meth_receive(lua_State *L);
static int udp_meth_receivefrom(lua_State *L);
static int udp_meth_getsockname(lua_State *L);
static int udp_meth_getpeername(lua_State *L);
static int udp_meth_setsockname(lua_State *L);
static int udp_meth_setpeername(lua_State *L);
static int udp_meth_close(lua_State *L);
static int udp_meth_timeout(lua_State *L);

/* udp object methods */
static luaL_reg udp[] = {
    {"setpeername", udp_meth_setpeername},
    {"setsockname", udp_meth_setsockname},
    {"getsockname", udp_meth_getsockname},
    {"getpeername", udp_meth_getpeername},
    {"send",        udp_meth_send},
    {"sendto",      udp_meth_sendto},
    {"receive",     udp_meth_receive},
    {"receivefrom", udp_meth_receivefrom},
    {"timeout",     udp_meth_timeout},
    {"close",       udp_meth_close},
    {NULL,          NULL}
};

/* functions in library namespace */
static luaL_reg func[] = {
    {"udp", udp_global_create},
    {NULL, NULL}
};

/*-------------------------------------------------------------------------*\
* Initializes module
\*-------------------------------------------------------------------------*/
void udp_open(lua_State *L)
{
    /* create classes */
    aux_newclass(L, "udp{connected}", udp);
    aux_newclass(L, "udp{unconnected}", udp);
    /* create class groups */
    aux_add2group(L, "udp{connected}", "udp{any}");
    aux_add2group(L, "udp{unconnected}", "udp{any}");
    /* define library functions */
    luaL_openlib(L, LUASOCKET_LIBNAME, func, 0); 
    lua_pop(L, 1);
}

/*=========================================================================*\
* Lua methods
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Send data through connected udp socket
\*-------------------------------------------------------------------------*/
static int udp_meth_send(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkclass(L, "udp{connected}", 1);
    p_tm tm = &udp->tm;
    size_t count, sent = 0;
    int err;
    const char *data = luaL_checklstring(L, 2, &count);
    tm_markstart(tm);
    err = sock_send(&udp->sock, data, count, &sent, tm_get(tm));
    if (err == IO_DONE) lua_pushnumber(L, sent);
    else lua_pushnil(L);
    error_push(L, err);
    return 2;
}

/*-------------------------------------------------------------------------*\
* Send data through unconnected udp socket
\*-------------------------------------------------------------------------*/
static int udp_meth_sendto(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkclass(L, "udp{unconnected}", 1);
    size_t count, sent = 0;
    const char *data = luaL_checklstring(L, 2, &count);
    const char *ip = luaL_checkstring(L, 3);
    ushort port = (ushort) luaL_checknumber(L, 4);
    p_tm tm = &udp->tm;
    struct sockaddr_in addr;
    int err;
    memset(&addr, 0, sizeof(addr));
    if (!inet_aton(ip, &addr.sin_addr)) 
        luaL_argerror(L, 3, "invalid ip address");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    tm_markstart(tm);
    err = sock_sendto(&udp->sock, data, count, &sent, 
            (SA *) &addr, sizeof(addr), tm_get(tm));
    if (err == IO_DONE) lua_pushnumber(L, sent);
    else lua_pushnil(L);
    error_push(L, err == IO_CLOSED ? IO_REFUSED : err);
    return 2;
}

/*-------------------------------------------------------------------------*\
* Receives data from a UDP socket
\*-------------------------------------------------------------------------*/
static int udp_meth_receive(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkgroup(L, "udp{any}", 1);
    char buffer[UDP_DATAGRAMSIZE];
    size_t got, count = (size_t) luaL_optnumber(L, 2, sizeof(buffer));
    int err;
    p_tm tm = &udp->tm;
    count = MIN(count, sizeof(buffer));
    tm_markstart(tm);
    err = sock_recv(&udp->sock, buffer, count, &got, tm_get(tm));
    if (err == IO_DONE) lua_pushlstring(L, buffer, got);
    else lua_pushnil(L);
    error_push(L, err);
    return 2;
}

/*-------------------------------------------------------------------------*\
* Receives data and sender from a UDP socket
\*-------------------------------------------------------------------------*/
static int udp_meth_receivefrom(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkclass(L, "udp{unconnected}", 1);
    struct sockaddr_in addr;
    size_t addr_len = sizeof(addr);
    char buffer[UDP_DATAGRAMSIZE];
    size_t got, count = (size_t) luaL_optnumber(L, 2, sizeof(buffer));
    int err;
    p_tm tm = &udp->tm;
    tm_markstart(tm);
    count = MIN(count, sizeof(buffer));
    err = sock_recvfrom(&udp->sock, buffer, count, &got, 
            (SA *) &addr, &addr_len, tm_get(tm));
    if (err == IO_DONE) {
        lua_pushlstring(L, buffer, got);
        lua_pushstring(L, inet_ntoa(addr.sin_addr));
        lua_pushnumber(L, ntohs(addr.sin_port));
        return 3;
    } else {
        lua_pushnil(L);
        error_push(L, err);
        return 2;
    }
}

/*-------------------------------------------------------------------------*\
* Just call inet methods
\*-------------------------------------------------------------------------*/
static int udp_meth_getpeername(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkclass(L, "udp{connected}", 1);
    return inet_meth_getpeername(L, &udp->sock);
}

static int udp_meth_getsockname(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkgroup(L, "udp{any}", 1);
    return inet_meth_getsockname(L, &udp->sock);
}

/*-------------------------------------------------------------------------*\
* Just call tm methods
\*-------------------------------------------------------------------------*/
static int udp_meth_timeout(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkgroup(L, "udp{any}", 1);
    return tm_meth_timeout(L, &udp->tm);
}

/*-------------------------------------------------------------------------*\
* Turns a master udp object into a client object.
\*-------------------------------------------------------------------------*/
static int udp_meth_setpeername(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkclass(L, "udp{unconnected}", 1);
    const char *address =  luaL_checkstring(L, 2);
    int connecting = strcmp(address, "*");
    unsigned short port = connecting ? 
        (ushort) luaL_checknumber(L, 3) : (ushort) luaL_optnumber(L, 3, 0);
    const char *err = inet_tryconnect(&udp->sock, address, port);
    if (err) {
        lua_pushnil(L);
        lua_pushstring(L, err);
        return 2;
    }
    /* change class to connected or unconnected depending on address */
    if (connecting) aux_setclass(L, "udp{connected}", 1);
    else aux_setclass(L, "udp{unconnected}", 1);
    lua_pushnumber(L, 1);
    return 1;
}

/*-------------------------------------------------------------------------*\
* Closes socket used by object 
\*-------------------------------------------------------------------------*/
static int udp_meth_close(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkgroup(L, "udp{any}", 1);
    sock_destroy(&udp->sock);
    return 0;
}

/*-------------------------------------------------------------------------*\
* Turns a master object into a server object
\*-------------------------------------------------------------------------*/
static int udp_meth_setsockname(lua_State *L)
{
    p_udp udp = (p_udp) aux_checkclass(L, "udp{master}", 1);
    const char *address =  luaL_checkstring(L, 2);
    unsigned short port = (ushort) luaL_checknumber(L, 3);
    const char *err = inet_trybind(&udp->sock, address, port, -1);
    if (err) {
        lua_pushnil(L);
        lua_pushstring(L, err);
        return 2;
    }
    lua_pushnumber(L, 1);
    return 1;
}

/*=========================================================================*\
* Library functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Creates a master udp object 
\*-------------------------------------------------------------------------*/
int udp_global_create(lua_State *L)
{
    /* allocate udp object */
    p_udp udp = (p_udp) lua_newuserdata(L, sizeof(t_udp));
    /* set its type as master object */
    aux_setclass(L, "udp{unconnected}", -1);
    /* try to allocate a system socket */
    const char *err = inet_trycreate(&udp->sock, SOCK_DGRAM);
    if (err) {
        /* get rid of object on stack and push error */
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, err);
        return 2;
    }
    /* initialize timeout management */
    tm_init(&udp->tm, -1, -1);
    return 1;
}
