#!/bin/bash

HOST=192.168.66.1
if [ -d "www" ]; then
    echo "downloading presets"
    wget http://192.168.66.1/js/presets.js -O www/js/presets.js
	echo "rsyncing www folder"
	# copy file via scp
	rsync -avz --exclude="node_modules" --delete www/ -e ssh root@$HOST:/var/www/
	
fi
