# SkyrimTogetherRelay

Lets friends join a host whose router can't be opened: no UPnP, a router that refuses it, or an internet provider that
shares one public address between customers (carrier-grade NAT, which UPnP can't open at all).

## How it works

```
friend's game ──UDP──> relay:10601 ──┐                       ┌──> 127.0.0.1:10578 (the host's game server)
                                     └─ relay:10600 <──UDP── host's server (RelayClient), which opened this connection
```

1. The host's server (`GameServer:sRelay`) sends `Allocate` to the relay's control port (10600). The relay gives it a
   port of its own (10601-10699) and the host's server logs `[Host] Public address: <relay>:<port>`, which the game
   shows as the invite address and join code.
2. Friends connect their game to that address. They need nothing special: to them the relay is the server.
3. The relay passes each friend's packets to the host's server over the connection the host opened, tagged with the
   friend's id and public address (`encoding/Relay.h`). The host's server hands them to the game from a loopback
   socket per friend and sends the answers back the same way. The server treats each friend as the address the relay
   saw (bans, the host's own kick rights, coming back after a crash).
4. The host's server sends a keepalive every 5 s; the relay frees the port after 30 s without one, and forgets a
   friend after 60 s of silence. The host's outgoing connection works through any router, so it needs no forwarding.

The host's server only uses the relay when its router port can't be opened (`PortMapper` reports it, including a
router whose own address is private), or always when `GameServer:bUPnP` is off.

## Running one

It has to run on a machine that the host and every friend can reach, with UDP 10600-10699 open there: a small VPS,
or a friend's computer with those ports forwarded.

```
SkyrimTogetherRelay [--port 10600] [--ports 10601-10699] [--password text] [--max-hosts 16] [--max-peers 16]
```

It is plain IPv4 UDP with no dependencies besides the system's sockets (`Code/common/UdpSocket.h`), so it builds for
Windows (`xmake build SkyrimTogetherRelay`) and Linux. The host puts the machine's `address:port` (and the password)
in Host, Relay, which writes `GameServer:sRelay` and `sRelayPassword`.

## What still needs a hosted machine

Everything through the relay does: the relay is that machine. Nothing in this repository runs one; the owner (or a
player) has to run it somewhere reachable and give hosts its address.

## Why not hole punching

Hole punching lets two players behind ordinary home routers talk directly. Both sides send to each other's public
address (learned from a public STUN server) so that each router lets the other's packets in. Two things stop this here:

- The game server's UDP socket belongs to GameNetworkingSockets. Punching needs that socket to send to each joiner's
  public address before the joiner's packets arrive. The library as this project builds it (without its ICE/WebRTC
  option) has no way to do that, and a second socket on the same port would steal its packets.
- The host has to learn each joiner's public address before they connect, and the joiner the host's, through some
  channel both can reach. That is a rendezvous server, a hosted machine just like the relay.

So punching would still need a hosted rendezvous, and would still fail between two "symmetric" routers (most
carrier-grade NATs), where only a relay works. Building GameNetworkingSockets with ICE (libjuice) and using the relay
as its signalling channel would add direct connections with the relay as fallback; that is not done.
