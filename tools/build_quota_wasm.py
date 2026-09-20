#!/usr/bin/env python3
"""Optional 32-bit ABI heap test using an existing local WASI SDK."""
from pathlib import Path
import subprocess
import sys
sdk=Path(sys.argv[1] if len(sys.argv)>1 else '.tools/wasi-sdk-25.0-arm64-macos')
names='lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio lauxlib lbaselib lmathlib lstrlib ltablib lutf8lib'.split()
src=Path('third_party/lua-5.4.8/src')
subprocess.run([str(sdk/'bin/clang'),'-O2','-mllvm','-wasm-enable-sjlj','-D_WASI_EMULATED_SIGNAL',
               '-D_WASI_EMULATED_PROCESS_CLOCKS','-I'+str(src),'tests/quota.c',
               *[str(src/(n+'.c')) for n in names],'-lm','-lwasi-emulated-signal',
               '-lwasi-emulated-process-clocks','-lsetjmp','-o','build/quota.wasm'],check=True)
