License: GPL

Why?
----
If you want to run a wifi hotspot with traffic redirected over Socks5 Proxy.

How to?
-------
Build with make, then run:
```
iptables -t nat -N VSOCKS
iptables -t nat -A VSOCKS -p tcp -j REDIRECT --to-ports 12345
iptables -t nat -A PREROUTING -s 10.42.0.0/24 -p tcp -j REDIRECT --to-ports 12345
# where 0.42.0.0/24 is wifi subnet
vsocks 0.0.0.0 12345 socks-proxy-addr socks-proxy-port
```

Usage message:
--------------
```
usage: vsocks l-addr l-port s-addr s-port

       l-addr       Listen address
       l-port       Listen port
       s-addr       Socks address
       s-port       Socks port

```

