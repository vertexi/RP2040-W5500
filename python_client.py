import sys
import socket
import errno
from time import sleep
import time

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ip = "192.168.1.2"
port = 5000
addr = (ip, port)

sock.connect(addr)
sock.settimeout(1)

a = b""
for i in range(int(7*512/7)):
    a = a + b"hello\r\n"

count = 100
send_message = b""
for i in range(count):
    send_message = send_message + a
send_message_len = len(send_message)
recv_message = b""
start = time.time_ns()
for i in range(count):
    writen_len = sock.send(a)
    recv_message = recv_message + sock.recv(writen_len)
while (len(recv_message) < send_message_len):
    recv_message = recv_message + sock.recv(writen_len)
end = time.time_ns()
print(recv_message == send_message)
print(send_message_len*8/((end - start)/1000))
a = input()
