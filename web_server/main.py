import time
from http.server import HTTPServer
from server import Server


HOST_NAME = 'localhost'
PORT_NUMBER = 8000

if __name__ == '__main__':
    httpd = HTTPServer((HOST_NAME, PORT_NUMBER), Server)
    print(time.asctime(), 'Server started - %s:%s\n' % (HOST_NAME, PORT_NUMBER))
    
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
        
    http.server_close()
    print(time.asctime(), 'Server stopped - %s:%s\n' % (HOST_NAME, PORT_NUMBER))