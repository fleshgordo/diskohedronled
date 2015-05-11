#!/usr/bin/python

import serial
ser = serial.Serial('/dev/ttyUSB0',9600, timeout=1)
output=0
lastoutput=0

while True:
	fifo = open('/tmp/fifo','r')	
	output = fifo.read()
	if (output!=lastoutput):
		print "writing to serial port: " + output
	ser.write(output)
	fifo.close()
	lastoutput = output
