import socket
sock = socket.socket()
ip = "192.168.1.2"
port = 5000
addr = (ip, port)

sock.connect(addr)
writen_len = sock.send(b"hello\n")
buf = sock.recv(10)
print(buf)
sock.close()
