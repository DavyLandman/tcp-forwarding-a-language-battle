use std::io::{Listener, Acceptor};
use std::io::net::tcp::{TcpListener, TcpStream};

static EXT_PORT: int = 8443;
static SSH_PORT: int = 22;
static SSL_PORT: int = 9443;


fn keep_copying(mut a: TcpStream, mut b: TcpStream) {
	loop {
		let read = try!(a.read(buf));
		b.write(buf.slice_to(read));
	}
}

fn start_pipe(mut front: TcpStream, port: int, header: i8) {
	let mut back = TcpStream::connect("127.0.0.1", port);
	let mut front_copy = front.clone();
	let mut back_copy = back.clone();

	spawn(proc() {
		keep_copying(front, back);	
	});
	spawn(proc() {
		keep_copying(back_copy, front_copy);	
	});
}

fn handle_new_connection(mut stream: TcpStream) {
	//stream.set_read_timeout(2000);
	let header = try!(stream.read_byte());
	if header == 22 || header == 128 {
		start_pipe(stream, SSL_PORT);
	}
	else {
		start_pipe(stream, SSH_PORT);
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
					handle_new_connection(stream)
					})
		}
    }
	drop(acceptor);
}
