import sys
import socket
import errno
from time import sleep
import time

# @profile
def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ip = "192.168.1.2"
    port = 5000
    addr = (ip, port)

    sock.connect(addr)
    sock.settimeout(1)

    a = b""
    for i in range(int(14*1024/7)):
        a = a + b"hello\r\n"

    count = 10000
    send_message = b""
    for i in range(count):
        send_message = send_message + a
    send_message_len = len(send_message)
    recv_message = b""
    recv_messages = []
    recv_length = 0
    start = time.time_ns()
    for i in range(count):
        writen_len = sock.send(a)
        recv = sock.recv(writen_len)
        recv_length += len(recv)
        recv_messages.append(recv)
    while (recv_length < send_message_len):
        recv = sock.recv(writen_len)
        recv_length += len(recv)
        recv_messages.append(recv)
    recv_message = b''.join(recv_messages)
    end = time.time_ns()
    print(recv_message == send_message)
    print((end - start)/1000_000_000)
    print(send_message_len*8/((end - start)/1000))
    a = input()

if __name__ == '__main__':
    main()
