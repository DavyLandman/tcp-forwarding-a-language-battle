# TCP forwarding: a language battle

This repository contains the implementations of the same program in different languages/frameworks. The goal is simple: fun. I found this simple "problem" and it has some complications making it just difficult enough to hit the rougher edges of a language (or framework). Also, I found it provides a nice way of learning a new language.

## The "problem"
### Context
Forwarding the port `443` to your ssh server is a very good way of making sure you can [escape most hotel/airport/company versions of internet](http://www.reddit.com/r/linux/comments/1aok5j/does_everyone_use_ssh_over_port_443_to_bypass/).

However, I was already using port `443` to host some personal HTTPS sites.
Not wanting to lose those, I required a special kind of port forwarding: based on the first packet it would have to redirect everything to either the ssh server or the https server. Much later I found out that [sslh](http://www.rutschle.net/tech/sslh.shtml) already solves hit problem. But my [programmer mind was triggered](http://xkcd.com/1319/). After writing the first version in nodejs, I started experimenting with other languages :).

### Requirements
#### Basic functionality

- Acccept connections on a port (for example 8443)
- Check the first byte of a new connection, if it has the value `0x16` or `0x80` redirect it to the https server (for example port 9443) else to the ssh server (port 22)
- After redirecting, just forward the bytes along in both directions.
- If either the client or the server closes the connection, close the other end.

This is all for a first working version of the program.

#### Timeouts
There are however two features which are quite important especially for resource consumption and compatibility. They are both related to timeouts. Normally the application server has some timeout/connection-lost semantics, but we break that.

- If a new connection does not send anything for 3 seconds, redirect it to the ssh server (Some SSH clients wait for the server banner).
- If __both__ client and server have not send anything for 15 minutes, close both the connections.

## Testing/benchmarking
### Testing
- __basic forwarding__: use `scp` to transfer a big file and check if the md5sum is correct
- __timeout__: Open a connection to the program using `netcat` and after 3 seconds it should be connected to the SSH server.
- __multiple clients__: Apache `ab` is a very good tool to show multi-threadings/asynchronous errors. `-c 200 -n 2000` tends to show even the strangest kind of bugs.

### Benchmarking
The goal is not to compare programming languages/framework using this problem. However, I did some rough benchmarks to see the overhead of the tunneling, or the choice of buffers.

- __one-stream performance__: `ssh -c arcfour -C -p 8443 localhost 'cat /dev/zero' | dd of=/dev/null bs=1m count=400` will stream 400MB trough ssh and report the speed (which you should compare with `-p 22`). Alternativly you could put a big file on the http host and download it with curl/wget.
- __multiple clients__: Again, apache `ab` is a very nice tool to analyse how the forwarder performs under stress of multiple connections. Limited of course by the underlying server to handle concurrent the connections.

## Related & Larger approaches
- [TodoMVC](http://todomvc.com/): Compares MVC frameworks based on a todo app use case. [tastejs/todomvc](https://github.com/tastejs/todomvc)
- [The Computer Language Benchmarks Game](http://benchmarksgame.alioth.debian.org/): Comparing languages for different algorithms/challenges. (Same toy program, same computer, same workload)
- [101companies](http://101companies.org/): Different implementations of a HRM system. [101companies/101repo](https://github.com/101companies/101repo)
- [Exercises in Programming Style](http://www.amazon.co.uk/Exercises-Programming-Style-Cristina-Videira/dp/1482227371%3FSubscriptionId%3DAKIAILSHYYTFIVPWUY6Q%26tag%3Dduc08-21%26linkCode%3Dxm2%26camp%3D2025%26creative%3D165953%26creativeASIN%3D1482227371):  Comparing programming styles for the same program. [crista/exercises-in-programming-style](https://github.com/crista/exercises-in-programming-style)

