#!/bin/bash

FILE=$1
#HOST=10.42.0.56
HOST=192.168.66.1
PORT=/dev/ttyACM0

if [ "$#" -lt "1" ]; then
    echo "No hex file provided. Usage: ./upload.sh filename.hex "
    exit 1
fi

# copy file via scp
scp $FILE root@$HOST:

if [[ "$FILE" == *\/* ]] || [[ "$FILE" == *\\* ]]
then
  FILE=`basename $FILE`
fi
# echo $FILE
# reset arduino and upload with avrdude
ssh root@$HOST "stty -F $PORT hupcl && avrdude -c stk500v1 -b57600 -P $PORT -p m328p -u -U flash:w:$FILE"
