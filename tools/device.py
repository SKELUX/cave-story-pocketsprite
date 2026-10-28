#!/usr/bin/env python3
"""PocketSprite file-manager client. Preserves firmware, saves and unrelated files; replaces low-space project files directly."""
import argparse
import fcntl
import sys
import json
from pathlib import Path
from urllib.parse import urlencode
from urllib.request import Request, urlopen
from urllib.error import HTTPError


class Device:
    def __init__(self, url):
        self.url = url.rstrip('/')

    def request(self, endpoint, data=None, timeout=120):
        req = Request(self.url + '/' + endpoint, data=data)
        try:
            with urlopen(req, timeout=timeout) as response:
                return response.read()
        except HTTPError as error:
            detail=error.read(512).decode(errors='replace').strip()
            raise RuntimeError(f'Chooser rejected {endpoint.split("?")[0]} (HTTP {error.code}): {detail}') from error

    def index(self):
        return json.loads(self.request('fileidx.cgi',timeout=10))

    def upload(self, path):
        # All callers, including development scripts importing Device, share
        # one transaction lock. AppFS needs a temporary new-file allocation
        # until replacement completes; concurrent uploads see misleadingly
        # low free space and can otherwise interfere with readback.
        lock_path = Path(__file__).resolve().parent.parent/'build/device-upload.lock'
        lock_path.parent.mkdir(parents=True, exist_ok=True)
        with lock_path.open('a') as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                print('Another upload is running; waiting for it to finish...', flush=True)
                fcntl.flock(lock, fcntl.LOCK_EX)
            self._upload_locked(path)

    def bundle(self, directory):
        app=directory/'cavestory.app'; music=directory/'cavestory.dat'
        if not app.is_file() or not music.is_file():
            raise ValueError('Bundle must contain cavestory.app and cavestory.dat')
        if app.read_bytes()[:1] != b'\xe9' or music.read_bytes()[:4] != b'CSD1':
            raise ValueError('Invalid application or soundtrack bundle')
        lock_path=Path(__file__).resolve().parent.parent/'build/device-upload.lock'
        lock_path.parent.mkdir(parents=True,exist_ok=True)
        with lock_path.open('a') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX)
            before=self.index()
            rounded=lambda size: ((size+65535)//65536)*65536
            available=before['free']+sum(rounded(f['size']) for f in before['files'] if f['name'] in (app.name,music.name))
            needed=rounded(app.stat().st_size)+rounded(music.stat().st_size)
            if needed>available:
                raise ValueError(f'Bundle needs {needed} bytes; only {available} available replacing Cave Story files. No files deleted.')
            self._upload_locked(music)
            self._upload_locked(app)

    def _upload_locked(self, path):
        # Only this project's application/data names can be replaced.
        if path.name not in ('cavestory.app', 'cavestory.dat'):
            raise ValueError('Upload name must be cavestory.app or cavestory.dat')
        data = path.read_bytes()
        if not data:
            raise ValueError('Empty upload')
        if path.suffix == '.app' and data[0] != 0xe9:
            raise ValueError('Not an ESP32 application image')
        before = self.index()
        # Reserve complete new allocation, including appfs page rounding.
        required = ((len(data) + 65535) // 65536) * 65536
        if before['free'] < required:
            old = next((f for f in before['files'] if f['name'] == path.name), None)
            reclaim = ((old['size'] + 65535)//65536)*65536 if old else 0
            if not old or before['free'] + reclaim < required:
                raise ValueError(f'Not enough device space for {path.name}, even replacing its old copy; '
                                 f'need {required}, can provide {before["free"] + reclaim}. No files deleted.')
            # Only the exact project-owned file being replaced is eligible.
            self.request('delete.cgi?' + urlencode({'idx': old['index']}), data=b'')
            removed = self.index()
            if any(f['name'] == path.name for f in removed['files']):
                raise RuntimeError('Old project file was not removed; aborting replacement')
        print(f'Uploading {path.name} ({len(data):,} bytes)...', flush=True)
        reply = self.request('upload.cgi?' + urlencode({'name': path.name}), data)
        if reply.strip():
            raise RuntimeError(f'Device rejected upload: {reply!r}')
        after = self.index()
        matches = [f for f in after['files'] if f['name'] == path.name]
        if len(matches) != 1 or matches[0]['size'] != len(data):
            raise RuntimeError('Uploaded file missing or size mismatch')
        preserved = {f['name']: f['size'] for f in after['files']}
        for f in before['files']:
            if f['name'] != path.name and preserved.get(f['name']) != f['size']:
                raise RuntimeError(f'Unrelated file changed: {f["name"]}')
        print(f'Uploaded {path.name}: {len(data)} bytes (device file size confirmed).')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://192.168.4.1')
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('list')
    sub.add_parser('log')
    upload = sub.add_parser('upload')
    upload.add_argument('file', type=Path)
    bundle = sub.add_parser('bundle')
    bundle.add_argument('directory', type=Path)
    args = parser.parse_args()
    device = Device(args.url)
    if args.command == 'list':
        print(json.dumps(device.index(), indent=2))
    elif args.command == 'log':
        print(device.request('getlog.cgi').decode(errors='replace'))
    elif args.command == 'bundle':
        device.bundle(args.directory)
    else:
        device.upload(args.file)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError) as error:
        print(f'Upload/device error: {error}', file=sys.stderr)
        raise SystemExit(1)
