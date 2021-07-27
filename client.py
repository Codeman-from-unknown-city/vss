import socket
import struct
from threading import Thread, Condition

import numpy
import cv2

MAX_DGRAM = 2 ** 16
PCKT_SIZE = MAX_DGRAM - 64
HOST      = '172.20.10.7' #'192.168.0.30' #'localhost' 
PORT      = 8080
SERVADDR  = (HOST, PORT)


def recvfromall(sock, size):
    data = b''
    nrecvd = 0
    while nrecvd < size:
        chunk, _ = sock.recvfrom(size - nrecvd)
        data += chunk
        nrecvd += len(chunk)
    return data


class SharedMem:
    def __init__(self, sock):
        self.data_obsolete = True
        self.sock = sock
        self.cond = Condition()
        self.jpg = b''


    def recv_jpg(self):
        while True:
            pckt = recvfromall(self.sock, PCKT_SIZE)
            jpg_size = struct.unpack('!H', pckt[1:3])[0]
            with self.cond:
                if self.data_obsolete:
                    self.jpg = pckt[3:jpg_size+2]
                    self.data_obsolete = False
                    self.cond.notify()


    def handle_jpg(self): 
        while True:
            with self.cond:
                if self.data_obsolete:
                    self.cond.wait()
                jpg = bytearray(self.jpg) # copy jpg
                self.data_obsolete = True
            try:
                img = cv2.imdecode(numpy.frombuffer(jpg, dtype=numpy.uint8), cv2.IMREAD_UNCHANGED)
                cv2.imshow('pi', img)
                cv2.waitKey(10) 
            except cv2.error as err:
                print(err)


conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
peer = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
conn.connect(SERVADDR)
peer.sendto(b"h", SERVADDR)
shared_mem = SharedMem(peer)
producer = Thread(target=shared_mem.recv_jpg)
consumer = Thread(target=shared_mem.handle_jpg)
producer.start()
consumer.start()

