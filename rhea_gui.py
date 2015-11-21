from __future__ import print_function, division
import sys
import string
import select
import socket
import Tkinter as Tk
from PIL import Image, ImageTk
import numpy as np
import zlib
import pdb
import time

class ClientSocket:
	MAX_BUFFER = 65536
#	def __init__(self,IP="133.40.162.192", Port=3001):
        def __init__(self,IP="150.203.89.12",Port=3001):
		#NB See the prototype in macquarie-university-automation for a slightly neater start.
		ADS = (IP,Port)
		try:
			self.client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			self.client.connect(ADS)
			self.client.settimeout(0)
			#self.client.setsockopt(1, 2, 1)
			self.client.setsockopt(6, 1, 1)
			self.connected=True
		except: 
			print('ERROR: Could not connect to server. Please check that the server is running.')
			self.connected=False



	def send_command(self, command):
#sends a command to the device server and waits for a response
		self.client.settimeout(0)
		try: self.client.recv(self.MAX_BUFFER)
		except: no_data_available_yet=True
		try: self.client.send(command)
		except Exception: return 'Error sending command, connection likely lost.'
		self.client.settimeout(5)
		time.sleep(0.1)
		try: return self.client.recv(self.MAX_BUFFER)
		except Exception: return 'Error receiving response'

class RHEAGui:
	current_image=0;
	IMAGE_WIDTH=320;
	IMAGE_HEIGHT=256;
	def __init__(self):
		self.client_socket = ClientSocket()
		self.win=Tk.Tk()
		self.win.title("RHEA@Subaru Injection")
		row = Tk.Frame(self.win)
		Tk.Label(row, text='Command: ').pack(side=Tk.LEFT)
		self.command = Tk.StringVar()
		command_entry = Tk.Entry(row, textvariable=self.command)
		command_entry.pack(side=Tk.RIGHT, expand=Tk.YES)
		command_entry.bind("<Return>", self.send_to_server)
		row.pack(fill=Tk.X)
		self.response_label = Tk.Label(self.win, text="[No Server Response Yet]",\
			height=6, width=40,justify=Tk.LEFT, bg='black', fg='green', anchor='w')
		self.response_label.pack(fill=Tk.X)
		imdata = np.zeros( (self.IMAGE_HEIGHT,self.IMAGE_WIDTH), dtype=np.uint8)
		image = ImageTk.PhotoImage(Image.fromarray(imdata))
		self.image_label = Tk.Label(self.win, image=image)
		self.image_label.image=image
		self.image_label.pack(fill=Tk.X)
		self.ask_for_image()

	def ask_for_image(self):
		command = "image {0:d}".format(self.current_image)
		if (self.client_socket.connected):
			response = self.client_socket.send_command(command)
			if (response.split(" ", 1)[0]=="image"):
				self.display_image(response)
			else:
				self.response_label['text']=response
		self.win.after(100, self.ask_for_image)

	def display_image(self, response):	
		argv = response.split(" ",3)
		current_image=int(argv[1])
		compressed_len = int(argv[2])
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
				imdata = ( (imdata - np.min(imdata))/(np.max(imdata)-np.min(imdata))*255).astype(np.uint8)
				image = ImageTk.PhotoImage(Image.fromarray(imdata))
				self.image_label['image']=image
				self.image_label.image=image	

	def send_to_server(self, event):
		if (self.client_socket.connected):
			response = self.client_socket.send_command(self.command.get())
			if (response.split(" ", 1)[0]=="image"):
				self.display_image(response)
			else:
				self.response_label['text']=response
		else:
			self.response_label['text']="*** Not Connected ***"

		self.command.set("")

TheGui = RHEAGui()
TheGui.win.mainloop()		
			


