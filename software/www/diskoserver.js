// creating globals for server, socket, 

var server = require('http').createServer();
var io = require('socket.io')(server);
var exec = require('child_process').exec,
	child;

var SerialPort = require("serialport").SerialPort;


var serialPort = new SerialPort("/dev/ttyUSB0", {
	baudrate: 9600,
	dataBits: 8,
	parity: 'none',
	stopBits: 1,
	flowControl: false
});



serialPort.open(function(error) {
	if (error) {
		console.log('** failed to open: ' + error);
	} else {
		console.log('** opened serialport successfully.');
		//serialPort.write('A');
	}
});

serialPort.on('data', receiveSerial);

function receiveSerial(data) {
	console.log("arduino: " + data);
}

io.sockets.on('connection', function(socket) {
	console.log('** socket connected');

	// callback for client disconnect
	socket.on('disconnect', function() {
		console.log('** socket disconnected');
	});

	// shutdown entire board
	socket.on('halt', function() {
		child = exec('shutdown -h now');
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
			console.log('### switch lamp to mode: ' + _status);
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
			console.log('### changing brightness to: ' + _val);
		}
	});

	/*
	 * MODE 1 PARAMS
	 */
	socket.on('p1Brightness', function(_val) {
		var isOpen = serialPort.isOpen();
		if (isOpen === true) {
			serialPort.write("p," + _val + ";");
			console.log('### changing parameter: ' + _val);
		}
	});
	/*
	 * MODE 2 PARAMS
	 */
	socket.on('p1Brightness', function(_val) {
		var isOpen = serialPort.isOpen();
		if (isOpen === true) {
			serialPort.write("p," + _val + ";");
			console.log('### changing parameter: ' + _val);
		}
	});
});

server.listen(3003);