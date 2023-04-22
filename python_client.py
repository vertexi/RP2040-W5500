import sys
import socket
import errno
from time import sleep

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ip = "192.168.1.2"
port = 5000
addr = (ip, port)

sock.connect(addr)
sock.settimeout(1)

# writen_len = sock.send(b"hello\r\n")
while True:
    try:
        msg = sock.recv(2048)
    except socket.timeout as e:
        err = e.args[0]
        # this next if/else is a bit redundant, but illustrates how the
        # timeout exception is setup
        if err == 'timed out':
            sleep(1)
            print('recv timed out, retry later')
            continue
        else:
            print(e)
            sys.exit(1)
    except socket.error as e:
        # Something else happened, handle error, exit, etc.
        print(e)
        sys.exit(1)
    else:
        if len(msg) == 0:
            print('orderly shutdown on server end')
            sys.exit(0)
        else:
            print(msg)
