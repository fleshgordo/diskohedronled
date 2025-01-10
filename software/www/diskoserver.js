// creating globals for server, socket, 

var server = require('http').createServer(),
	io = require('socket.io')(server),
	exec = require('child_process').exec,
	child,
	SerialPort = require("serialport").SerialPort,
	fs = require('fs'),
	logS = "** socket.io ** ",
	logA = "[[  arduino  ]] ",
	logN = "##  node.js  ## ";


/*
 * Initializing serial port
 */


var serialPort = new SerialPort({
	path: "/dev/ttyUSB0",
	baudRate: 9600,
	dataBits: 8,
	parity: 'none',
	stopBits: 1,
	flowControl: false
},false);

serialPort.on('open', () => {
    console.log('Serial port opened successfully.');
});

serialPort.on('error', (error) => {
    console.log('Error opening serial port: ' + error.message);
});

/*
 * Receive data callback
 */
serialPort.on('data', function receiveSerial(data) {
	console.log(logA + data);
});

/*
 * Initializing socket.io and listeners
 */
io.sockets.on('connection', function(socket) {
	console.log(logS + 'socket connected');

	// callback for client disconnect
	socket.on('disconnect', function() {
		console.log(logS + 'socket disconnected');
	});

	// shutdown entire board
	socket.on('halt', function() {
		child = exec('shutdown -h now');
	});

	// save presets and write to file
	socket.on('savePresets', function(presets) {
		fs.writeFile("/var/www/js/presets.js", "var presets=" + JSON.stringify(presets) + ";",
			function(err) {
				if (err) {
					return console.log(err);
				}
				console.log(logS + "File was saved ...");
			});
	});
	// switch lamp on/off
	socket.on('lampStatus', function(_status) {
		console.log('#### switch lamp to ' + _status);
		var isOpen = serialPort.isOpen();
		_status === "off" && isOpen === true ?
			serialPort.write('0') : serialPort.write('1');
	});

	// switch lamp modus
	socket.on('lampMode', function(_status) {
		var isOpen = serialPort.isOpen();
		if (isOpen === true) {
			serialPort.write("m," + _status + ";");
			console.log(logS + 'switch lamp to mode: ' + _status);
		}
	});

	/*
	 * PARAMS GENERAL
	 */

	// Brightness
	socket.on('pBrightness', function(_val) {
		var isOpen = serialPort.isOpen();
		if (isOpen === true) {
			serialPort.write("p," + _val + ";");
			console.log(logS + 'changing brightness to: ' + _val);
		}
	});

	/*
	 * CHANGING PARAMS
	 */
	socket.on('p', function(_val) {
		var isOpen = serialPort.isOpen();
		if (isOpen === true) {
			serialPort.write("p," + _val + ";");
			console.log(logS + 'changing parameter: ' + _val);
		}
	});

});

server.listen(3003, '192.168.178.115', () => {
    console.log('Server is running on http://0.0.0.0:3003');
});
