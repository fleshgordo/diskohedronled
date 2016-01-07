// creating globals for server, socket, 

var server = require('http').createServer();
var io = require('socket.io')(server);
var exec = require('child_process').exec,
	child;

var SerialPort = require("serialport").SerialPort;
var fs = require('fs');

//var serialPort = new SerialPort("/dev/ttyACM0", {

var serialPort = new SerialPort("/dev/ttyUSB0", {
	baudrate: 115200,
	dataBits: 8,
	parity: 'none',
	stopBits: 1,
	flowControl: false
});

var logS = "** socket.io ** ",
	logA = "[[  arduino  ]] ",
	logN = "##  node.js  ## ";

/*
 * Initializing serial port
 */
serialPort.open(function(error) {
	if (error) {
		console.log(logN + ' failed to open: ' + error);
	} else {
		console.log(logN + 'opened serialport successfully.');
		//serialPort.write('A');
	}
});

serialPort.on('data', receiveSerial);

function receiveSerial(data) {
	console.log(logA + "arduino: " + data);
}

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
		fs.writeFile("./js/presets.js", "var presets=" + JSON.stringify(presets) + ";",
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
	 * MODE 0 PARAMS
	 */
	socket.on('p1', function(_val) {
		var isOpen = serialPort.isOpen();
		if (isOpen === true) {
			serialPort.write("p," + _val + ";");
			console.log(logS + 'changing parameter: ' + _val);
		}
	});

	/*
	 * MODE 2 PARAMS 
	 */
	socket.on('p2', function(_val) {
		var isOpen = serialPort.isOpen();
		if (isOpen === true) {
			serialPort.write("p," + _val + ";");
			console.log(logS + 'changing parameter: ' + _val);
		}
	});
	/*
	 * MODE 2 PARAMS 
	 */
	socket.on('p3', function(_val) {
		var isOpen = serialPort.isOpen();
		if (isOpen === true) {
			serialPort.write("p," + _val + ";");
			console.log(logS + 'changing parameter: ' + _val);
		}
	});
});

server.listen(3003);