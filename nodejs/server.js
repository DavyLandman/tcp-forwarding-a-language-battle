var http = require('http'),
	fs = require('fs'),
	net = require('net');

net.createServer(httpHttpsSwitch).listen(8080);
net.createServer(httpsSshSwitch).listen(8443);

function httpHttpsSwitch(conn) {
	conn.once('data', function(buf) {
		// A TLS handshake record starts with byte 22.
		var address = (buf[0] === 22) ? 9443 : 9080;
		var proxy = net.createConnection(address, function() {
			proxy.write(buf);
			conn.pipe(proxy).pipe(conn);
		});
	});
}
function httpsSshSwitch(conn) {
	var allreadyPiped = false;
	var sshServer = setTimeout(function() {
		allreadyPiped = true;
		var proxy = net.createConnection(22, function() {
		//var proxy = net.createConnection(22, "192.168.3.11", function() {
			conn.pipe(proxy).pipe(conn);
		});
	}, 2000);

	conn.once('data', function(buf) {
		clearTimeout(sshServer);
		if (allreadyPiped) return;
		// A TLS handshake record starts with byte 22.
		var address = (buf[0] === 22 || buf[0] === 128) ? 9443 : 22;
		var proxy = net.createConnection(address, function() {
			proxy.write(buf);
			conn.pipe(proxy).pipe(conn);
		});
		proxy.on('error', function (e) {
			if (e.code === 'ECONNRESET' || e.code === 'EPIPE') {
				conn.destroy();
			}
			else {
				console.log("Strange error" + e);
				console.log(e.code);
				throw e;
			}
		});
		conn.on('error', function (e) {
			if (e.code === 'ECONNRESET' || e.code === 'EPIPE') {
				proxy.destroy();
			}
			else {
				console.log("Strange error" + e);
				console.log(e.code);
				throw e;
			}
		});
	});
}
