// WASI32 is an ABI-size test, not an ESP32 emulator or firmware heap measurement.
import { WASI } from 'node:wasi';
import { readFile } from 'node:fs/promises';
const wasi = new WASI({version:'preview1', args:['quota',process.argv[2] || '98304'],
  preopens:{'.':process.cwd()}});
const module = await WebAssembly.compile(await readFile('build/quota.wasm'));
const instance = await WebAssembly.instantiate(module,{wasi_snapshot_preview1:wasi.wasiImport});
process.exitCode = wasi.start(instance) || 0;
