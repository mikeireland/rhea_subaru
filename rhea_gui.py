from __future__ import print_function, division
import sys
import string
import select
import socket
from PyQt4.QtGui import *
from PyQt4.QtCore import *
import numpy as np
import zlib
import pdb
import time
import zmq

class ClientSocket:
    MAX_BUFFER = 65536
#    def __init__(self,IP="133.40.162.192", Port=3001):
#    def __init__(self,IP="150.203.89.12",Port=3001):
    def __init__(self,IP="192.168.1.5",Port=3001):
        #NB See the prototype in macquarie-university-automation for a slightly neater start.
        ADS = (IP,Port)
        try:
            self.context = zmq.Context()
            self.client = self.context.socket(zmq.REQ)
            self.client.connect("tcp://"+IP+":3001")
            self.connected=True
        except: 
            print('ERROR: Could not connect to server. Please check that the server is running.')
            self.connected=False

    def send_command(self, command):
        """WARNING: currently a blocking send/recv!"""
        try: self.client.send(command)
        except Exception: return 'Error sending command, connection likely lost.'
        try: return self.client.recv(self.MAX_BUFFER)
        except Exception: return 'Error receiving response'

class RHEAGui(QWidget):
    current_image=0;
    IMAGE_WIDTH=320;
    IMAGE_HEIGHT=256;
    def __init__(self, parent=None):
        super(RHEAGui,self).__init__(parent)
        self.client_socket = ClientSocket() 
        lbl1 = QLabel('Command: ', self)
        self.lineedit = QLineEdit("")
        self.connect(self.lineedit, SIGNAL("returnPressed()"),
                        self.send_to_server)
        self.response_label = QLabel('[No Sever Response Yet]', self)
        self.response_label.setFixedWidth(320)
        self.response_label.setFixedHeight(100)
        imdata = np.zeros( (self.IMAGE_HEIGHT,self.IMAGE_WIDTH), dtype=np.uint32)
        self.image = QImage(imdata.tostring(),self.IMAGE_WIDTH,self.IMAGE_HEIGHT,QImage.Format_RGB32)
        self.image_label = QLabel("",self)
        self.image_label.setPixmap(QPixmap.fromImage(self.image))
        
        hbox1 = QHBoxLayout()
        hbox1.addWidget(lbl1)
        hbox1.addWidget(self.lineedit)
        
        layout = QVBoxLayout()
        layout.addLayout(hbox1)
        layout.addWidget(self.response_label)
        layout.addWidget(self.image_label)
        self.setLayout(layout)
        self.setWindowTitle("RHEA@Subaru Injection")
        self.stimer = QTimer()
        self.ask_for_image()

    def ask_for_image(self):
        command = "image {0:d}".format(self.current_image)
        if (self.client_socket.connected):
            response = self.client_socket.send_command(command)
            if (response.split(" ", 1)[0]=="image"):
                self.display_image(response)
            else:
                self.response_label.setText(response)
        self.stimer.singleShot(200, self.ask_for_image)

    def display_image(self, response):    
        argv = response.split(" ",3)
        current_image=int(argv[1])
        compressed_len = int(argv[2])
        if compressed_len == 0:
            return
        if (compressed_len > len(argv[3])):
            print("ERROR: Only got {0:d} of {1:d} bytes".format(len(argv[3]), compressed_len))
        else:
            imdata = zlib.decompress(argv[3][0:compressed_len])
            if (len(imdata)!=self.IMAGE_WIDTH*self.IMAGE_HEIGHT*4):
                print("ERROR: Image size {0:d} instead of {1:d}".format(len(imdata), self.IMAGE_WIDTH*self.IMAGE_HEIGHT*4))
            else:
                imdata = np.fromstring(imdata, np.float32)
                imdata = np.array(imdata).reshape( (self.IMAGE_HEIGHT,self.IMAGE_WIDTH) )
                #Autoscale for now...
                imdata = ( (imdata - np.min(imdata))/np.maximum(np.max(imdata)-np.min(imdata),1)*255).astype(np.uint32)
                imdata = imdata + (imdata<<8) + (imdata<<16)
                self.image = QImage(imdata.tostring(),self.IMAGE_WIDTH,self.IMAGE_HEIGHT,QImage.Format_RGB32)
#                pyqtRemoveInputHook(); pdb.set_trace()
                self.image_label.setPixmap(QPixmap.fromImage(self.image))

    def send_to_server(self):
        if (self.client_socket.connected):
            response = self.client_socket.send_command(str(self.lineedit.text()))
            if (response.split(" ", 1)[0]=="image"):
                self.display_image(response)
            else:
                self.response_label.setText(response)
        else:
            self.response_label.setText("*** Not Connected ***")

        self.lineedit.setText("")

app = QApplication(sys.argv)
myapp = RHEAGui()
myapp.show()
sys.exit(app.exec_())      
            


