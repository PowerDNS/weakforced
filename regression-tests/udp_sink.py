import socket
import os

UDP_IP = "127.0.0.1"
UDP_PORT = 4501

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))

logfile = open('/tmp/udp-sink.log', 'w', 0)
logfile.write("This is to ensure the file isn't empty\n")
mypid = os.getpid()

while True:
    data, addr = sock.recvfrom(2048) # buffer size is 2048 bytes
    log_msg = "[%s] Received report=%s\n" % (mypid, data)
    logfile.write(log_msg)
    print log_msg;
