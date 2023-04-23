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

# @profile
def loopback_test():
    a = b""
    for i in range(int(14*1024/7)):
        a = a + b"hello\r\n"

    count = 1000
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
    end = time.time_ns()
    recv_message = b''.join(recv_messages)
    print(recv_message == send_message)
    print((end - start)/1000_000_000)
    print(send_message_len*8/((end - start)/1000))
    a = input()

def data_transfer_test():
    start = time.time_ns()
    sock.send(b"hello")
    count = int(6000*14*1024/7)
    send_message = b""
    send_messages = []
    a = b"hellomm"
    for i in range(count):
        send_messages.append(a)
    send_message = b''.join(send_messages)
    send_message_len = len(send_message)
    end = time.time_ns()
    print((end - start)/1000_000_000)

    recv_message = b""
    recv_messages = []
    recv_length = 0
    start = time.time_ns()
    while (recv_length < send_message_len):
        try:
            recv = sock.recv(409600)
            recv_length += len(recv)
            recv_messages.append(recv)
        except:
            print("wrong" + str(recv_length))
            pass
    end = time.time_ns()
    sock.send(b"end!!")
    recv_message = b''.join(recv_messages)
    print(send_message_len)
    print(len(recv_message))
    print(recv_message[:send_message_len] == send_message)
    print((end - start)/1000_000_000)
    print(send_message_len*8/((end - start)/1000))
    a = input()

if __name__ == '__main__':
    # loopback_test()
    data_transfer_test()
