use std::io::{Listener, Acceptor};
use std::io::net::tcp::{TcpListener, TcpStream};

static EXT_PORT: u16 = 8443;
static SSH_PORT: u16 = 22;
static SSL_PORT: u16 = 9443;


fn keep_copying(mut a: TcpStream, mut b: TcpStream) {
	let mut buf = [0u8,..1024];
	loop {
		let read = match a.read(buf) {
			Err(..) => { drop(a); return; },
			Ok(r) => r
		};
		match b.write(buf.slice_to(read)) {
			Err(..) => { drop(a); return; },
			Ok(r) => r
		};
	}
}

fn start_pipe(front: TcpStream, port: u16, header: u8) {
	let mut back = match TcpStream::connect("127.0.0.1", port) {
		Err(e) => { println!("Error connecting: {}", e); return; },
		Ok(b) => b
	};
	match back.write_u8(header) {
		Err(e) => { println!("Error writing first byte: {}", e); return }
		Ok(..) => ()
	}
	let front_copy = front.clone();
	let back_copy = back.clone();

	spawn(proc() {
		keep_copying(front, back);
	});
	spawn(proc() {
		keep_copying(back_copy, front_copy);
	});
}

fn handle_new_connection(mut stream: TcpStream) {
	//stream.set_read_timeout(2000);
	let header = match stream.read_byte() {
		Err(..) => -1,
		Ok(b) => b
	};
	if header == 22 || header == 128 {
		start_pipe(stream, SSL_PORT, header);
	}
	else {
		start_pipe(stream, SSH_PORT, header);
	}
}

fn main() {
    let mut acceptor = TcpListener::bind("127.0.0.1", EXT_PORT).listen().unwrap();
    println!("listening started, ready to accept");
    for stream in acceptor.incoming() {
		match stream {
			Err(e) => { /* connection failed */ }
			Ok(stream) => spawn(proc() {
					// connection succeeded
					handle_new_connection(stream);
					})
		}
    }
	drop(acceptor);
}
