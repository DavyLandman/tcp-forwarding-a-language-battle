# TCP Tunneling: a language battle

This repository contains the implementations of the same program in different languages/frameworks. The goal is simple: programming fun. I found this simple "problem" and it has some complications making it just difficult enough to hit the rougher edges of a language (or framework).

## Related & Larger approaches

- Ralf Lämmel et al.'s [101companies](http://101companies.org/) contains a lot of different implementations of the same system. [101companies/101repo](https://github.com/101companies/101repo)
- Cristina Videira Lopes's [Exercises in Programming Style](http://www.amazon.co.uk/Exercises-Programming-Style-Cristina-Videira/dp/1482227371%3FSubscriptionId%3DAKIAILSHYYTFIVPWUY6Q%26tag%3Dduc08-21%26linkCode%3Dxm2%26camp%3D2025%26creative%3D165953%26creativeASIN%3D1482227371) which compares different programming styles for the same program. [crista/exercises-in-programming-style](https://github.com/crista/exercises-in-programming-style)

## The "problem"
### Context
Forwarding the port `443` to your ssh server is a very good way of making sure you can [escape most hotel/airport/company versions of internet](http://www.reddit.com/r/linux/comments/1aok5j/does_everyone_use_ssh_over_port_443_to_bypass/).

However, I was already using port `443` to host some personal HTTPS sites.
Not wanting to loose those, I required a special kind of port forwarding: based on the first packet it would have to redirect everything to either the ssh server or the https server. Much later I found out that [sslh](http://www.rutschle.net/tech/sslh.shtml) does exactly this. But that was after my programmer mind was triggered. So I wrote the first version of the program in a few lines of nodejs, and the program slowly evolved.

### Requirements
...
