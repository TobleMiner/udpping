udpping
=======

A simple udp echo client/server with configureable payload size

Mainly written for embedded devices with no non volatile memory to spare.

# Building
Just run ```make```

# Usage
Run udpping in server mode on a device with a publically reachable IP address and no NAT.

Run udpping in client mode (-c) on another device. The IP address used must be the one of the device running the udpping server.

```
./udpping -h
USAGE: ./udpping [-c IP-ADDRESS] [-p PORT] [-i TIMEOUT] [-s PACKET_SIZE]
-c: Client mode, IP-ADDRESS is address of device running ./udpping in server mode
```

# NAT
In client mode this utility can be placed behind a nat firewall.
It listens on the source port used for outgoing packets, thus the NAT should redirect all responses to the correct port.
