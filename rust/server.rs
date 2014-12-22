use std::io::{Listener, Acceptor,IoError, IoErrorKind};
use std::io::net::tcp::{TcpListener, TcpStream};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

static EXT_PORT: u16 = 8443;
static SSH_PORT: u16 = 22;
static SSL_PORT: u16 = 9443;


fn keep_copying(mut a: TcpStream, mut b: TcpStream, timedOut: Arc<AtomicBool>) {
	a.set_read_timeout(Some(15*60*1000));
	let mut buf = [0u8,..1024];
	loop {
		let read = match a.read(&mut buf) {
			Err(ref err) => {
				let other = timedOut.swap(true, Ordering::AcqRel);
				if other {
					// the other side also timed-out / errored, so lets go
					drop(a);
					drop(b);
					return;
				}
				if err.kind == IoErrorKind::TimedOut {
					continue;
				}
				// normal errors, just stop
				drop(a);
				drop(b);
				return; // normal errors, stop
			},
			Ok(r) => r
		};
		timedOut.store(false, Ordering::Release);
		match b.write(buf.slice_to(read)) {
			Err(..) => {  
				timedOut.store(true, Ordering::Release);
				drop(a); 
				drop(b);
				return;
			},
			Ok(..) => ()
		};
	}
}

fn start_pipe(front: TcpStream, port: u16, header: Option<u8>) {
	let mut back = match TcpStream::connect(("127.0.0.1", port)) {
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

	let timedOut = Arc::new(AtomicBool::new(false));
	let timedOut_copy = timedOut.clone();

	spawn(move|| {
		keep_copying(front, back, timedOut);
	});
	spawn(move|| {
		keep_copying(back_copy, front_copy, timedOut_copy);
	});
}

fn handle_new_connection(mut stream: TcpStream) {
	stream.set_read_timeout(Some(3000));
	let header: Option<u8> = match stream.read_byte() {
		Err(..) => None,
		Ok(b) => Some(b)
	};
	if header.is_some() && (header.unwrap() == 22 || header.unwrap() == 128) {
		start_pipe(stream, SSL_PORT, header);
	}
	else {
		start_pipe(stream, SSH_PORT, header);
	}
}

fn main() {
    let mut acceptor = TcpListener::bind(("127.0.0.1", EXT_PORT)).listen().unwrap();
    println!("listening started, ready to accept");
    for stream in acceptor.incoming() {
		match stream {
			Err(e) => println!("Strange connection broken: {}", e),
			Ok(stream) => spawn(move|| {
					// connection succeeded
					handle_new_connection(stream);
					})
		}
    }
	drop(acceptor);
}
