import serial, time, os

ser = serial.Serial('/dev/ttyUSB0', 9600, timeout=1)
output=0
while True:
	time.sleep(11)
	print "writing 1 to white, 0 to black"
	ser.write("0")
	os.system("ssh pi@192.168.66.10 \"echo 0 > /tmp/fifo\"")
	time.sleep(11)
	print "writing 0 to white, 1 to black"
	ser.write("1")
	os.system("ssh pi@192.168.66.10 \"echo 1 > /tmp/fifo\"")
