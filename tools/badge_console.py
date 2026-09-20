#!/usr/bin/env python3
"""Badge IDE-compatible console/upload helper. Never flashes or erases firmware."""
import argparse
import fcntl
import os
from pathlib import Path
import select
import struct
import termios
import time
import tty


class Console:
    def __init__(self, port):
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.old = termios.tcgetattr(self.fd)
        tty.setraw(self.fd)
        cfg = termios.tcgetattr(self.fd)
        cfg[2] |= termios.CLOCAL | termios.CREAD
        cfg[4] = cfg[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, cfg)
        fcntl.ioctl(self.fd, termios.TIOCMBIS, struct.pack('I', termios.TIOCM_DTR))

    def close(self):
        try:
            termios.tcsetattr(self.fd, termios.TCSANOW, self.old)
        finally:
            os.close(self.fd)

    def read(self, seconds=0.2, until=None):
        data = bytearray()
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if select.select([self.fd], [], [], min(0.05, max(0, deadline-time.monotonic())))[0]:
                data.extend(os.read(self.fd, 16384))
                if until and until in data:
                    break
        if until and until not in data:
            raise RuntimeError(f'Timeout waiting for {until!r}: {data[-2000:]!r}')
        return bytes(data)

    def write(self, data):
        for i in range(0, len(data), 128):
            part = memoryview(data[i:i+128])
            while part:
                if not select.select([], [self.fd], [], 2)[1]:
                    raise RuntimeError('USB write timed out')
                part = part[os.write(self.fd, part):]
            time.sleep(0.012)

    def command(self, text, timeout=5):
        self.read()
        self.write(text.encode()+b'\r')
        return self.read(timeout, b'badge> ')

    def upload(self, directory):
        manifest = (directory/'manifest.cfg').read_text()
        slug = next(x[5:] for x in manifest.splitlines() if x.startswith('slug='))
        if slug != 'badge_market':
            raise ValueError('This helper only installs the badge_market app')
        self.command('')  # Fail before mutation if console is not ready.
        remote = '/littlefs/apps/'+slug
        self.command('mkdir '+remote)
        files = sorted(directory.glob('*.lua')) + [directory/'manifest.cfg']
        for path in files:
            data = path.read_bytes()
            if len(data)>16384:
                raise ValueError(f'{path.name} exceeds 16 KiB file cap')
            self.read()
            self.write(f'put {remote}/{path.name} {len(data)}\r'.encode())
            self.read(5, b'READY')
            self.write(data)
            self.read(20, f'OK {len(data)}'.encode())
            print(f'{path.name}: {len(data)} bytes installed', flush=True)
        print(self.command('reload', 10).decode(errors='replace'))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', default='/dev/cu.usbmodem1101')
    p.add_argument('--upload', type=Path)
    p.add_argument('--listen', type=float, default=0)
    p.add_argument('commands', nargs='*')
    args = p.parse_args()
    con = Console(args.port)
    try:
        if args.upload:
            con.upload(args.upload)
        for cmd in args.commands:
            print(con.command(cmd).decode(errors='replace'), flush=True)
        if args.listen:
            print(con.read(args.listen).decode(errors='replace'), flush=True)
    finally:
        con.close()


if __name__ == '__main__':
    main()
