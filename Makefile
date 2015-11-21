all: rhea_inject

thor_usb.o: thor_usb.c thor_usb.h
	gcc -O2 -c -g thor_usb.c

uicoms.o: uicoms.c uicoms.h
	gcc -c -g uicoms.c

zaber_control.o: zaber_control.c zaber.h
	gcc -c -g  zaber_control.c

rhea_inject.o: rhea_inject.c
	gcc -c -g rhea_inject.c

rhea_inject: rhea_inject.o zaber_control.o uicoms.o thor_usb.o
	gcc -o rhea_inject rhea_inject.o zaber_control.o uicoms.o thor_usb.o -lueye_api -lcfitsio -lz -lpthread -lm

clean: 
	rm *.o
	rm rhea_inject
