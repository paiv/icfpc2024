#!/usr/bin/env python
import http.client
import logging
import readline
import requests
import sys
import subprocess
import time
import tomllib
from datetime import datetime
from pathlib import Path
from urllib.parse import urlparse, urljoin
from icfp import IcfpCalc


_VERBOSE = 0

def setup_logging(level):
    global _VERBOSE
    _VERBOSE = 1 if level > 0 else 0
    http.client.HTTPConnection.debuglevel = level
    logging.basicConfig()
    logging.getLogger().setLevel(level)
    requests_log = logging.getLogger("urllib3")
    requests_log.setLevel(level)
    requests_log.propagate = True


def trace(*args, **kwargs):
    if _VERBOSE > 0: print(*args, file=sys.stderr, flush=True, **kwargs)


comms_ascii_tr = str.maketrans(
    '!"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~',
    'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!"#$%&\'()*+,-./:;<=>?@[\\]^_`|~ \n')

ascii_comms_tr = str.maketrans(
    'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!"#$%&\'()*+,-./:;<=>?@[\\]^_`|~ \n',
    '!"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~')


class IcfpClient:
    API_URL = 'https://boundvariable.space/'

    def __init__(self, /, headers=None):
        self.api_url = self.API_URL
        self.headers = headers
        self.backend = None
        self.timeout = (5, 10)
        self._last_ts = datetime(1,1,1)

    def __enter__(self):
        self.backend = requests.Session()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        if (ses := self.backend) is not None:
            self.backend = None
            ses.close()

    def post_bytes(self, path, data):
        url = urljoin(self.api_url, path)
        trace('<', repr(data))
        headers = dict(self.headers or dict())
        headers['Content-Type'] = 'text/icfp'
        dt = datetime.now() - self._last_ts
        if dt.seconds < 3:
            time.sleep(3 - dt.total_seconds())
        r = self.backend.post(url, data=data, headers=self.headers, timeout=self.timeout)
        self._last_ts = datetime.now()
        r.raise_for_status()
        data = r.content
        trace('>', repr(data))
        return data

    def comms(self, arg):
        data = arg.encode()
        r = self.post_bytes('/communicate', data)
        return r.decode()

    def send_ascii(self, text):
        data = text.translate(ascii_comms_tr)
        r = self.comms('S' + data)
        return r


def read_config(config=None):
    headers = None
    if config is None:
        config = '.env'
    config = Path(config)
    if config.is_file():
        with config.open('rb') as fp:
            config = tomllib.load(fp)
            portal = config.get('portal')
            if portal:
                headers = portal.get('headers')
    return (headers,)


def run_proc(value, *args):
    p = subprocess.run(args, input=value, encoding='utf8', capture_output=True)
    if p.returncode != 0:
        print(p.stderr, file=sys.stderr)
    return p.returncode, (p.stdout or '').strip()


def run_comms(args):
    setup_logging(logging.INFO if args.verbose else logging.NOTSET)
    headers, = read_config()
    print('''
!eval
!tr
!run
!solve lambdaman
!solve spaceship
    ''')

    calc = IcfpCalc()
    value = None
    with IcfpClient(headers=headers) as cli:
        while True:
            cmd = input('> ').strip()
            match cmd[:1]:
                case 'q' | 'Q' if (cmd == 'quit' or cmd == 'Q'):
                    break
                case '!':
                    op,*args = cmd.split()
                    match op:
                        case '!eval' | '!tr':
                            value = calc.evaluate(value)
                            print(value)
                        case '!encode':
                            value = calc.encode_string(' '.join(args))
                            print(value)
                        case '!comms':
                            s = ' '.join(args) if args else value
                            value = cli.comms(s)
                            print(value)
                            if value[:1] == 'S':
                                print(calc.evaluate(value))
                        case '!echo':
                            s = ' '.join(args)
                            s = 'B. S%#(/} ' + s
                            value = cli.comms(s)
                            if value[:1] == 'S':
                                print(calc.evaluate(value))
                        case '!run':
                            res,value = run_proc(value, *args)
                            trace(f'status {res}')
                            if res == 0:
                                print(f'{value[:40]!r}')
                                if len(value) > 40:
                                    print(f'({len(value)} bytes)')
                        case '!solve':
                            if args:
                                smd = ['solve', *args, value or '']
                                value = cli.send_ascii(' '.join(smd))
                                print(value)
                                if value[:1] == 'S':
                                    print(calc.evaluate(value))
                            else:
                                print('! usage: solve <name>')
                        case '!save':
                            if args:
                                a, = args
                                fn = Path(a)
                                with fn.open('w') as fp:
                                    fp.write(value)
                            else:
                                print('! usage: save <filename>')
                        case '!fetch':
                            if args:
                                name,fn = args
                                fn = Path(fn)
                                value = cli.send_ascii(f'get {name}')
                                with fn.open('w') as fp:
                                    fp.write(value)
                                print(str(fn))
                            else:
                                print('! usage: fetch <task> <filename>')
                        case '!efetch':
                            if args:
                                name,fn = args
                                fn = Path(fn)
                                value = cli.send_ascii(f'get {name}')
                                value = calc.evaluate(value)
                                with fn.open('w') as fp:
                                    fp.write(value)
                                print(str(fn))
                            else:
                                print('! usage: efetch <task> <filename>')
                        case '!get_tasks':
                            if args:
                                name,*ps = args
                                if ps:
                                    start = int(ps[0])
                                    stop = start
                                    if len(ps) > 1:
                                        stop = int(ps[1])
                                    for pid in range(start, stop+1):
                                        ns = f'{name}{pid}'
                                        fn = Path(f'task/{ns}.icfp.txt')
                                        value = cli.send_ascii(f'get {ns}')
                                        with fn.open('w') as fp:
                                            fp.write(value)
                                        print(str(fn))
                                else:
                                    fn = Path(f'task/{name}.icfp.txt')
                                    value = cli.send_ascii(f'get {name}')
                                    with fn.open('w') as fp:
                                        fp.write(value)
                                    print(str(fn))
                            else:
                                print('! usage: get_tasks <name> [<start> [<stop>]]')
                        case _:
                            print(f'! unknown command {op!r}')
                case _:
                    value = cli.send_ascii(cmd)
                    print(value)
                    if value[:1] == 'S':
                        print(calc.evaluate(value))


if __name__ == '__main__':
    sys.setrecursionlimit(2000)
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()
    run_comms(args)
