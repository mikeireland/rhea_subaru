from __future__ import absolute_import, division, print_function
import zmq

context = zmq.Context()

#  Socket to talk to server
print("Connecting to server...")
socket = context.socket(zmq.REQ)
socket.connect("tcp://localhost:3001")

while True:
    command = raw_input() #!!! Not python 3 ready !!!
    socket.send(command)
    message = socket.recv()
    print(message)
