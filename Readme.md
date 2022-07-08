Purpose
-------
Make a wifi hotspot with IP/TCP traffic redirected over Socks5 Proxy.  
For example to connect a phone device behind the proxy.  
Note: DNS traffic is not redirected. IPv6 networking supported.

Building
--------
```
make
```

Example
-------
```
iptables -t nat -N VSOCKS
iptables -t nat -A VSOCKS -p tcp -j REDIRECT --to-ports 12345
iptables -t nat -A PREROUTING -s 10.42.0.0/24 -p tcp -j REDIRECT --to-ports 12345
# where 0.42.0.0/24 is wifi subnet
vsocks 0.0.0.0 12345 socks-proxy-addr socks-proxy-port
```

To setup Socks5 Server you could use another project here: axproxy
```
axproxy socks-proxy-addr:socks-proxy-port
```

Help message
------------
```
[vsck] [vsck] VSocks - ver. 1.05.1a
[vsck] usage: vsocks listen-addr:listen-port socks5-addr:socks5s-port [[+]dest-addr:dest-port]

       option -v         Enable verbose logging
       option -d         Run in background
       listen-addr       Gateway address
       listen-port       Gateway port
       socks5-addr       Socks server address
       socks5-port       Socks-5 server port
       dest-addr         Destination address override
       dest-port         Destination port override


```

