#!/bin/bash

FILE=$1
HOST=192.168.66.1
HOST2=192.168.66.10
USER2=pi

if [ "$#" -lt "1" ]; then
    echo "No hex file provided. Usage: ./upload.sh filename.hex "
    exit 1
fi

FILENAME=`basename $FILE`

# copy file via cat-ing it through a one-liner ssh tunnel
cat $FILE | ssh root@$HOST 'cat | ssh '$USER2'@'$HOST2' "cat > /home/'$USER2'/'$FILENAME'"'
	
if [[ "$FILE" == *\/* ]] || [[ "$FILE" == *\\* ]]
then
  FILE=`basename $FILE`
fi

# reset arduino and upload with avrdude
ssh root@$HOST 'ssh '$USER2'@'$HOST2' "stty -F /dev/ttyUSB0 hupcl && avrdude -c stk500v1 -b57600 -P /dev/ttyUSB0 -p m328p -u -U flash:w:'$FILE'"'