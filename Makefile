LUA_DIR := third_party/lua-5.4.8/src
LUA := $(LUA_DIR)/lua
.PHONY: test lua package clean test32
lua:
	$(MAKE) -C $(LUA_DIR) generic MYCFLAGS='-DLUA_USE_POSIX' -j4
build/quota: tests/quota.c $(LUA_DIR)/liblua.a
	@mkdir -p build
	$(CC) -std=c11 -Wall -Wextra -Werror -I$(LUA_DIR) $< $(LUA_DIR)/liblua.a -lm -o $@
test: lua build/quota
	$(LUA_DIR)/luac -p app/badge_market/*.lua
	$(LUA) tests/market_test.lua
	$(LUA) tests/app_test.lua
	$(LUA) tests/reliability_test.lua
	./build/quota 147456
package:
	python3 tools/package.py
test32:
	python3 tools/build_quota_wasm.py
	node tools/quota_wasm.mjs
clean:
	rm -f build/quota
