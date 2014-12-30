#!/bin/bash

FILE=$1
HOST=10.42.0.56

if [ "$#" -lt "1" ]; then
    echo "No hex file provided. Usage: ./upload.sh filename.hex "
    exit 1
fi

# copy file via scp
scp $FILE root@$HOST:

# reset arduino and upload with avrdude
ssh root@$HOST "stty -F /dev/ttyUSB0 hupcl |  avrdude -c stk500v1 -b57600 -P /dev/ttyUSB0 -v -p m328p -u -U flash:w:$FILE"
