# https://medium.com/@andrewklatzke/creating-a-python3-webserver-from-the-ground-up-4ff8933ecb96
# https://medium.com/@andrewklatzke/building-a-python-webserver-from-the-ground-up-part-two-c8ca336abe62

from http.server import BaseHTTPRequestHandler
from pathlib import Path
import os

class Server(BaseHTTPRequestHandler):
    def do_HEAD(self):
        print('HEAD: ', self.path)
        return

    def do_POST(self):
        print('POST: ', self.path)
        return

    def do_GET(self):
        print('GET: ', self.path)
        self.respond()

    def handle_http(self):
        status = 200
        content_type = 'text/plain'
        content = ''
        # parts = os.path.splitext(self.path)        
        # print('parts: ', parts)
        
        pages = { '/setup': 'setup.html',
                '/sensors': 'sensors.html' }
                       
        if self.path in pages:
            print(pages[self.path])
            route = pages[self.path]
            content_path = Path('html/{}'.format(route))
            if content_path.is_file():
                content_type = 'text/html'
                file = open(content_path)
                content = file.read()
        else:
            print('not found ', self.path)
            content_type = 'text/plain'
            content = 'Not Found'
            status = 404

        self.send_response(status)
        self.send_header('Content-type', content_type)
        self.end_headers()
        return bytes(content, 'UTF-8')

    def respond(self):
        content = self.handle_http()
        self.wfile.write(content)