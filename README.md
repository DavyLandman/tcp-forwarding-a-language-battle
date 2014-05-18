# TCP Tunneling: a language battle

This repository contains the implementations of the same program in different languages/frameworks. The goal is simple: programming fun. I found this simple "problem" and it has some complications making it just difficult enough to hit the rougher edges of a language (or framework).

## The "problem"
### Context
Forwarding the port `443` to your ssh server is a very good way of making sure you can [escape most hotel/airport/company versions of internet](http://www.reddit.com/r/linux/comments/1aok5j/does_everyone_use_ssh_over_port_443_to_bypass/).

However, I was already using port `443` to host some personal HTTPS sites.
Not wanting to loose those, I required a special kind of port forwarding: based on the first packet it would have to redirect everything to either the ssh server or the https server. Much later I found out that [sslh](http://www.rutschle.net/tech/sslh.shtml) does exactly this. But that was after my programmer mind was triggered. So I wrote the first version of the program in a few lines of nodejs, and the program slowly evolved.

### Requirements
#### Basic functionality

- Acccept connections on a port (for example 8443)
- Check the first byte of a new connection, if it has the value `0x16` or `0x80` redirect it to the https server (for example port 9443) else to the ssh server (port 22)
- After redirecting, only forward the bytes along in both directions.
- If either the client or the server closes the connection, close the other end.

This is all there is needed to get a first working version of the program.

#### Timeouts
There are however two features which are quite important especially for resource consumption and they are both related to timeouts. Normally the application server has some timeout/connection-lost semantics, but we break that.

- If a new connection does not send anything for 3 seconds, redirect it to the ssh server (Some SSH clients wait for the server banner).
- If __both__ client and server have not send anything for 15 minutes, close both the connections.

## Testing/benchmarking
### Testing
- __basic forwarding__: I use `scp` to transfer a big file and check if the sum is correct
- __timeout__: Open a connection to the program using `netcat` and after 3 seconds it should be connected to the SSH server.
- __multiple clients__: Apache `ab` is a very good tool to show multi-threadings/asynchronous errors. `-c 200 -n 2000` tends to show even the stranges kind of bugs.

### Benchmarking
In general, it is not the idea to compare programming languages/framework using this problem, since it also depends on a lot of other factors in the pipe. However, I did some rough benchmarks to see the overhead of the tunneling, or the choice of buffers.

- __one-stream performance__: `ssh -c arcfour -C -p 8443 localhost 'cat /dev/zero' | dd of=/dev/null bs=1m count=400` will stream 400MB trough ssh and report the speed (which you should compare with `-p 22`). Alternativly you could put a big file on the http host and download it with curl/wget.
- __multiple clients__: Again, apache `ab` is a very nice tool to analyse how the forwarder performs under stress of multiple connections. This is again limited how well the underlying server can handle the connections.

## Related & Larger approaches

- Ralf Lämmel et al.'s [101companies](http://101companies.org/) contains a lot of different implementations of the same system. [101companies/101repo](https://github.com/101companies/101repo)
- Cristina Videira Lopes's [Exercises in Programming Style](http://www.amazon.co.uk/Exercises-Programming-Style-Cristina-Videira/dp/1482227371%3FSubscriptionId%3DAKIAILSHYYTFIVPWUY6Q%26tag%3Dduc08-21%26linkCode%3Dxm2%26camp%3D2025%26creative%3D165953%26creativeASIN%3D1482227371) which compares different programming styles for the same program. [crista/exercises-in-programming-style](https://github.com/crista/exercises-in-programming-style)

