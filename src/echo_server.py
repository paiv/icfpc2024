#!/usr/bin/env python
import http.server
import io
import json
import sys
import tempfile
import urllib.parse


class HttpEchoHandler (http.server.BaseHTTPRequestHandler):
    
    def do_GET(self):
        res = self._echo_response()
        body = json.dumps(res.body, separators=',:').encode('utf8')
        self.send_response(res.code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', len(body))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        content_type = self.headers['content-type']
        if not content_type.startswith('application/json'):
            self.send_error(406)
            return
        content_len = self.headers['content-length']
        content_len = int(content_len)
        with tempfile.TemporaryFile() as fp:
            while content_len > 0:
                data = self.rfile.read(content_len)
                if data:
                    fp.write(data)
                    content_len -= len(data)
                else:
                    break
            fp.seek(0)
            jbody = json.load(fp)
        res = self._echo_response(jbody)
        body = json.dumps(res.body, separators=',:').encode('utf8')
        self.send_response(res.code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', len(body))
        self.end_headers()
        self.wfile.write(body)

    class _EchoResponse:
        def __init__(self):
            self.code = 200
            self.body = dict()

    def _echo_response(self, content=None):
        res = self._EchoResponse()
        u = urllib.parse.urlparse(self.path)
        if content is not None:
            res.body.update(content)
        meta = dict()
        meta['client'] = dict()
        meta['client']['address'] = self.client_address
        meta['request'] = dict()
        meta['request']['line'] = self.requestline
        meta['request']['method'] = self.command
        meta['request']['path'] = u.path
        meta['request']['version'] = self.request_version
        meta['request']['path_params'] = u.params
        meta['request']['query'] = u.query
        meta['request']['headers'] = list(self.headers.items())
        res.body['_meta'] = meta
        return res


def serve():
    http.server.test(HttpEchoHandler, http.server.HTTPServer)


if __name__ == '__main__':
    serve()
