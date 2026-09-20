#!/usr/bin/env python3
"""Local development simulator: actual app Lua, mock SDK, two virtual badges."""
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path
import json
import subprocess
import os

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
lua = subprocess.Popen([str(ROOT/'third_party/lua-5.4.8/src/lua'), 'simulator/driver.lua'],
                       stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
state = lua.stdout.readline()


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path=='/state':
            body=state.encode();mime='application/json'
        elif self.path=='/':
            body=(ROOT/'simulator/index.html').read_bytes();mime='text/html'
        else:
            self.send_error(404);return
        self.send_response(200);self.send_header('Content-Type',mime)
        self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(body)

    def do_POST(self):
        global state
        size=int(self.headers.get('Content-Length','0'))
        if self.path!='/event' or size>100:
            self.send_error(400);return
        data=json.loads(self.rfile.read(size))
        op=data.get('op');device=int(data.get('device',1))
        if op=='tick': command='tick 100'
        elif op=='loss':command='loss 3'
        elif op=='key' and device in (1,2) and data.get('key') in ('A','B','UP','DOWN','LEFT','RIGHT','START','AUX1'):
            command=f'key {device} {data["key"]}'
        elif op=='hold' and device in (1,2):command=f'hold {device} '+('on' if data.get('down') else 'off')
        else:self.send_error(400);return
        lua.stdin.write(command+'\n');lua.stdin.flush();state=lua.stdout.readline()
        self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(state.encode())

    def log_message(self,*args):pass


if __name__=='__main__':
    print('Two-badge Lua simulator: http://127.0.0.1:8765',flush=True)
    try:HTTPServer(('127.0.0.1',8765),Handler).serve_forever()
    finally:lua.terminate();lua.wait(timeout=5)
