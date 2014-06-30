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
			Ok(..) => ()
		};
	}
}

fn start_pipe(front: TcpStream, port: u16, header: Option<u8>) {
	let mut back = match TcpStream::connect("127.0.0.1", port) {
		Err(e) => { 
			println!("Error connecting: {}", e); 
			drop(front);
			return;
		},
		Ok(b) => b
	};
	if header.is_some() {
		match back.write_u8(header.unwrap()) {
			Err(e) => { 
				println!("Error writing first byte: {}", e); 
				drop(back); 
				drop(front); 
				return;
			},
			Ok(..) => ()
		}
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
	let header: Option<u8> = match stream.read_byte() {
		Err(..) => None,
		Ok(b) => Some(b)
	};
	if header.is_some() && header.unwrap() == 22 || header.unwrap() == 128 {
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
			Err(e) => println!("Strange connection broken: {}", e),
			Ok(stream) => spawn(proc() {
					// connection succeeded
					handle_new_connection(stream);
					})
		}
    }
	drop(acceptor);
}
