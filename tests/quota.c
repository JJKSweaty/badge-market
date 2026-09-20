/* Native lightweight SDK mock: measures Lua allocations without Lua widget mocks.
 * Desktop ABI numbers are not ESP32 free heap or firmware-native widget costs. */
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static size_t used,peak,limit=98304;
static int clock_ms=1000,widgets;
static void *alloc(void *ud,void *ptr,size_t old,size_t n) {
 (void)ud;if(!ptr)old=0;
 if(!n){free(ptr);used-=old;return NULL;}
 if(used-old+n>limit)return NULL;
 void *p=realloc(ptr,n);if(p){used=used-old+n;if(used>peak)peak=used;}return p;
}
static int noop(lua_State *L){(void)L;return 0;}
static int yes(lua_State *L){lua_pushboolean(L,1);return 1;}
static int no(lua_State *L){lua_pushboolean(L,0);return 1;}
static int zero(lua_State *L){lua_pushinteger(L,0);return 1;}
static int ms(lua_State *L){lua_pushinteger(L,clock_ms);return 1;}
static int version(lua_State *L){lua_pushliteral(L,"quota-mock");return 1;}
static int mac(lua_State *L){lua_pushliteral(L,"01:02:03:04:05:06");return 1;}
static int randfn(lua_State *L){lua_Integer n=luaL_optinteger(L,1,65535);lua_pushinteger(L,123%n);return 1;}
static int widget(lua_State *L){lua_newuserdatauv(L,8,0);luaL_setmetatable(L,"widget");widgets++;return 1;}
static int stats(lua_State *L){lua_createtable(L,0,5);
 const char *k[]={"lua_used","lua_peak","lua_limit","widgets","free_heap"};
 size_t v[]={used,peak,limit,(size_t)widgets,65536};
 for(int i=0;i<5;i++){lua_pushinteger(L,v[i]);lua_setfield(L,-2,k[i]);}return 1;}
static int gc(lua_State *L){lua_gc(L,LUA_GCSTEP,0);return 0;}
static int requirefn(lua_State *L){
 const char *name=luaL_checkstring(L,1);char path[256];
 lua_getfield(L,LUA_REGISTRYINDEX,"modules");lua_getfield(L,-1,name);
 if(!lua_isnil(L,-1))return 1;
 lua_pop(L,1);snprintf(path,sizeof path,"app/badge_market/%s.lua",name);
 size_t before=used;
 if(luaL_loadfilex(L,path,"t")!=LUA_OK)return lua_error(L);
 lua_call(L,0,1);lua_pushvalue(L,-1);lua_setfield(L,-3,name);
 printf("module %s: allocation delta=%zd\n",name,(ptrdiff_t)used-(ptrdiff_t)before);return 1;
}
static void f(lua_State *L,const char *k,lua_CFunction fn){lua_pushcfunction(L,fn);lua_setfield(L,-2,k);}
static void call(lua_State *L,const char *name,int a,int b,int nargs){
 lua_getglobal(L,name);if(nargs>0)lua_pushinteger(L,a);if(nargs>1)lua_pushinteger(L,b);
 if(lua_pcall(L,nargs,0,0)!=LUA_OK){fprintf(stderr,"%s: %s (used=%zu peak=%zu)\n",name,lua_tostring(L,-1),used,peak);exit(1);}
}
static void press(lua_State *L,int key){call(L,"on_button",key,1,2);call(L,"on_button",key,2,2);clock_ms+=100;call(L,"on_tick",0,0,0);}
static void checkpoint(lua_State *L,const char *name){lua_gc(L,LUA_GCCOLLECT,0);printf("%s: used=%zu peak=%zu widgets=%d\n",name,used,peak,widgets);}
static void fill_market(lua_State *L){
 /* Find the captured authority table, then fill valid empty packed records.
  * This is test-only C introspection; no debug API is exposed to badge Lua. */
 lua_getfield(L,LUA_REGISTRYINDEX,"modules");lua_getfield(L,-1,"session");lua_getfield(L,-1,"act");
 int fn=lua_gettop(L),world=0;
 for(int i=1;;i++){const char *n=lua_getupvalue(L,fn,i);if(!n)break;if(strcmp(n,"world")==0){world=lua_gettop(L);break;}lua_pop(L,1);}
 if(!world){fprintf(stderr,"test authority missing\n");exit(1);}
 lua_getfield(L,world,"p");
 for(int i=2;i<=12;i++){char p[68]={0};snprintf(p,7,"%06d",i);p[6]=(char)0xa8;p[7]=0x61;p[10]=1;p[12]=75;lua_pushlstring(L,p,sizeof p);lua_rawseti(L,-2,i);}lua_pop(L,1);
 lua_getfield(L,world,"c");
 for(int i=2;i<=16;i++){char c[27]={0};c[0]=(char)((i-1)%12+1);c[19]=1;c[21]=1;memcpy(c+22,"OLD",3);c[25]=(char)('A'+i);lua_pushlstring(L,c,sizeof c);lua_rawseti(L,-2,i);}
 lua_settop(L,0);checkpoint(L,"full market");
}
int main(int argc,char **argv){
 if(argc>1)limit=(size_t)strtoul(argv[1],NULL,10);
 lua_State *L=lua_newstate(alloc,NULL);if(!L)return 1;
 const luaL_Reg libs[]={{"_G",luaopen_base},{LUA_STRLIBNAME,luaopen_string},{LUA_TABLIBNAME,luaopen_table},{LUA_MATHLIBNAME,luaopen_math},{LUA_UTF8LIBNAME,luaopen_utf8},{NULL,NULL}};
 for(const luaL_Reg *p=libs;p->func;p++){luaL_requiref(L,p->name,p->func,1);lua_pop(L,1);}
 lua_newtable(L);lua_setfield(L,LUA_REGISTRYINDEX,"modules");lua_pushcfunction(L,requirefn);lua_setglobal(L,"require");
 luaL_newmetatable(L,"widget");lua_pushvalue(L,-1);lua_setfield(L,-2,"__index");
 f(L,"set_pos",noop);f(L,"set_size",noop);f(L,"style",noop);f(L,"set_text",noop);f(L,"set_color",noop);lua_pop(L,1);
 lua_newtable(L);
 lua_newtable(L);f(L,"label",widget);f(L,"box",widget);lua_setfield(L,-2,"ui");
 lua_newtable(L);f(L,"clear",noop);f(L,"show",noop);f(L,"set",noop);f(L,"set_all",noop);lua_setfield(L,-2,"led");
 lua_newtable(L);f(L,"ms",ms);f(L,"version",version);f(L,"stats",stats);f(L,"random",randfn);f(L,"log",noop);f(L,"gc_step",gc);lua_setfield(L,-2,"sys");
 lua_newtable(L);f(L,"accel",noop);lua_setfield(L,-2,"sensor");
 lua_newtable(L);f(L,"read",noop);f(L,"write",yes);lua_setfield(L,-2,"fs");
 lua_newtable(L);f(L,"enable",yes);f(L,"disable",noop);f(L,"send",yes);f(L,"mac",mac);f(L,"on_recv",noop);f(L,"dropped",zero);lua_setfield(L,-2,"radio");
 lua_newtable(L);f(L,"is_down",no);lua_newtable(L);
 const char *keys[]={"A","B","HOME","DOWN","LEFT","RIGHT","UP","AUX1","START"};
 for(int i=0;i<9;i++){lua_pushinteger(L,i+1);lua_setfield(L,-2,keys[i]);}
 lua_setfield(L,-2,"BUTTON");lua_newtable(L);lua_pushinteger(L,1);lua_setfield(L,-2,"PRESSED");lua_pushinteger(L,2);lua_setfield(L,-2,"RELEASED");lua_setfield(L,-2,"KIND");lua_setfield(L,-2,"input");
 lua_setglobal(L,"badge");checkpoint(L,"sdk baseline");
 if(luaL_dofile(L,"app/badge_market/main.lua")!=LUA_OK){fprintf(stderr,"load: %s used=%zu peak=%zu\n",lua_tostring(L,-1),used,peak);return 1;}
 lua_getglobal(L,"on_enter");lua_newtable(L);if(lua_pcall(L,1,0,0)!=LUA_OK){fprintf(stderr,"enter: %s\n",lua_tostring(L,-1));return 1;}
 checkpoint(L,"launcher");press(L,4);press(L,1);checkpoint(L,"host");
 press(L,4);press(L,1);press(L,9);press(L,1);press(L,1);press(L,1);checkpoint(L,"create+trade");
 fill_market(L);
 press(L,9);press(L,2);press(L,1);checkpoint(L,"all modules");
 for(int i=0;i<1000;i++){press(L,9);press(L,1);press(L,1);press(L,9);press(L,2);press(L,2);}
 checkpoint(L,"1000 cycles");call(L,"on_exit",0,0,0);lua_close(L);
 if(used){fprintf(stderr,"allocator leak: %zu\n",used);return 1;}return 0;
}
