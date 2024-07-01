#!/usr/bin/env python
import http.client
import logging
import json
import requests
import sys
import tomllib
from pathlib import Path
from urllib.parse import urlparse, urljoin


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


class Client:
    API_URL = 'https://api.icfpcontest.com'

    def __init__(self, /, headers=None):
        self.api_url = self.API_URL
        self.headers = headers
        self.backend = None
        self.timeout = (0.3, 10)

    def __enter__(self):
        self.backend = requests.Session()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        if (ses := self.backend) is not None:
            self.backend = None
            ses.close()

    def get_bytes(self, path):
        url = urljoin(self.api_url, path)
        r = self.backend.get(url, headers=self.headers, timeout=self.timeout)
        r.raise_for_status()
        return r.content

    def get_json(self, path):
        url = urljoin(self.api_url, path)
        r = self.backend.get(url, headers=self.headers, timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def post_json(self, path, body):
        url = urljoin(self.api_url, path)
        r = self.backend.post(url, json=body, headers=self.headers, timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def get_problems_count(self):
        r = self.get_json('/problems')
        return r['number_of_problems']

    def get_problem(self, pid):
        r = self.get_json(f'/problem?problem_id={pid}')
        s = r['Success']
        return s

    def post_solution(self, pid, file):
        with file.open('r') as fp:
            body = json.load(fp)
        r = self.post_json(f'/solution?problem_id={pid}', body)


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


def handle_fetch(args):
    setup_logging(logging.INFO if args.verbose else logging.NOTSET)
    headers, = read_config(args.config_file)
    dest = Path(args.directory or '.')
    with Client(headers=headers) as cli:
        pid_range = None
        if args.pid == 'all':
            n = cli.get_problems_count()
            pid_range = range(1, n+1)
        else:
            pid_range = range(int(args.pid), int(args.last or args.pid)+1)
        for pid in pid_range:
            prob = cli.get_problem(pid)
            dest.mkdir(parents=True, exist_ok=True)
            fn = dest / f'{pid}.json'
            with fn.open('w') as fp:
                json.dump(prob, fp)
            trace(str(fn))


def handle_submit(args):
    setup_logging(logging.INFO if args.verbose else logging.NOTSET)
    headers, = read_config(args.config_file)
    with Client(headers=headers) as cli:
        for file in args.file:
            pid = args.pid
            file = Path(file)
            if pid is None:
                pid = file.stem
            cli.post_solution(pid, file)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(title='commands', required=True)
    parser.add_argument('-s', '--config-file', help='configuration variables')
    parser.add_argument('-v', '--verbose', action='store_true', help='print verbose logs')

    fetch = commands.add_parser('fetch', help='download tasks')
    fetch.set_defaults(handler=handle_fetch)
    fetch.add_argument('-d', '--directory', help='destination directory')
    fetch.add_argument('pid', default='all', help='problem id, or "all"')
    fetch.add_argument('last', nargs='?', help='id range, from pid to last')

    submit = commands.add_parser('submit', help='submit solution')
    submit.set_defaults(handler=handle_submit)
    submit.add_argument('pid', nargs='?', help='problem id')
    submit.add_argument('file', nargs='+', help='solution file')

    args = parser.parse_args()
    args.handler(args)
