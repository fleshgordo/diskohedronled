#!/bin/bash

HOST=192.168.66.1
HOST2=192.168.66.10
USER2=pi
PORT=/dev/ttyUSB0

EXPECTED_ARGS=1
if [ $# -ne $EXPECTED_ARGS ]; then
  echo "Usage: `basename $0` 1 | 0"
  exit $E_BADARGS
fi

FLAG=$1

if [ $FLAG = "0" ]; then
	echo "sending 0 to "$HOST2" on port /dev/ttyUSB0"
	ssh $USER2@$HOST2 "echo 0 > /tmp/fifo"
	#ssh $USER2@$HOST2 "echo 0 > /dev/ttyUSB0"
elif [ $FLAG = "1" ];then
	echo "sending 1 to "$HOST2" on port /dev/ttyUSB0"
	ssh $USER2@$HOST2 "echo 1 > /tmp/fifo"
	#ssh $USER2@$HOST2 "echo 1 > /dev/ttyUSB0"
	#ssh $USER2@$HOST2 "stty -F /dev/ttyUSB0 -hupcl && printf '%c' $'\x31' > /dev/ttyUSB0"
fi

echo "done"