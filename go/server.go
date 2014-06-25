package main
 
import (
	"net"
	"log"
	"io"
	"time"
	"strings"
)

var noDeadline = time.Time{}
var pipeTimeout = 15 * time.Minute

var (
	externalHost = net.TCPAddr{net.IPv4(0,0,0,0), 8443,""}
	sshTarget = net.TCPAddr{net.IPv4(127,0,0,1), 22,""}
	sslTarget = net.TCPAddr{net.IPv4(127,0,0,1), 9443,""}
)

const (
	RECV_BUF_LEN = 1024
	RECV_BUF_LEN_LARGE = 1024*1024
)
func main() {
	println("Starting the server")

	listener, err := net.ListenTCP("tcp", &externalHost)
	if err != nil {
		log.Fatal(err)
	}
	for {
		conn, err := listener.AcceptTCP()
		if err != nil {
			log.Fatal(err)
		}
		go handleRequest(conn)
	}
}

func handleRequest(conn *net.TCPConn) {
	buf := make([]byte, 1)
	// read for max 2 seconds
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	n, err := conn.Read(buf)
	timedOut := false
	if err != nil {
		if neterr, ok := err.(net.Error); ok && neterr.Timeout() {
			timedOut = true
		} else {
			defer conn.Close()
			log.Println("Error reading first bytes:", err.Error())
			return
		}
	}
	if n != 1 {
		log.Println("Unexpected amount of bytes read, assuming timeout alike scenario")
		timedOut = true
	}
	conn.SetReadDeadline(noDeadline)

	target := &sshTarget
	if !timedOut && (buf[0] == 22 || buf[0] == 128) {
		target = &sslTarget
	}
	connTarget, err := net.DialTCP("tcp4", nil, target)
	if err != nil {
		defer conn.Close()
		log.Println("Error connecting to server:", err.Error(), target)
		return
	}
	if !timedOut {
		_, err = connTarget.Write(buf)
		if err != nil {
			defer connTarget.Close()
			defer conn.Close()
			log.Println("Error writing to newly created connection:", err.Error(), target)
			return
		}
	}
	pipeStreams(conn, connTarget)
}

func pipeStreams(from, to *net.TCPConn) {
	fromTimedOut := make(chan bool)
	toTimedOut := make(chan bool)
	go keepCopying(from, to, fromTimedOut, toTimedOut)
	go keepCopying(to, from, toTimedOut, fromTimedOut)
}

func keepCopying(from, to *net.TCPConn, timedOut, otherTimedOut chan bool) {
	defer from.Close() // assure we always close connections
	defer to.Close()
	fullRounds := 0
	currentSize := RECV_BUF_LEN
	b := make([]byte, currentSize)
	for {
		from.SetReadDeadline(time.Now().Add(pipeTimeout))
		n, err := from.Read(b)
		iTimedOut := false
		if err != nil {
			if neterr, ok := err.(net.Error); ok && neterr.Timeout() {
				iTimedOut = true
			} else if err == io.EOF {
				// EOF is the regular way to close a channel
				// nothing special there
				return
			} else if strings.HasSuffix(err.Error(), "use of closed network connection") {
				// tends to happen since we close from two sides
				return
			} else {
				// lets log other errors
				log.Println("General read error, closing", err.Error(), from)
				if neterr, ok := err.(net.Error); ok && neterr.Temporary() {
					log.Println("\t it was temporary?")
				}
				return
			}
		}
		select {
		case other := <-otherTimedOut:
			log.Println("The other side had a timeout", from, other, iTimedOut, timedOut)
			if other {
				// non blocking send if we also timed-out
				select {
				case timedOut <- iTimedOut:
				default:
					// still a recv waiting, so lets continue
				}
			}
			if other && iTimedOut {
				return
			}
		default:
			if iTimedOut {
				log.Println("We have a timeout, lets notify the other side", from, timedOut)
				timedOut <- true
				continue
			}
			// no timeout reported, lets continue
		}
		if n == 0 {
			log.Println("Received 0 bytes..",from)
			continue
		} else if n == currentSize {
			// ideal case, we have a full buffer so we don't have to create a
			// copy just to write it.
			to.Write(b)
			fullRounds++
		} else {
			b2 := make([]byte, n)
			copy(b2, b)
			to.Write(b2)
		}
		if (fullRounds == 10 && currentSize == RECV_BUF_LEN) {
			// if our buffers are always full, and we still have a small buffer
			// size, switch to a larger buffer size, since it appears we have a
			// data intensive connection
			currentSize = RECV_BUF_LEN_LARGE
			b = make([]byte, currentSize)
		}
	}
}
