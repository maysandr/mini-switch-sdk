# Mini Switch SDK | Ethernet forwarding & register simulation in C

A small, self-contained **educational Ethernet-switch simulator** written in C11. It models the Layer 2 decisions made by a switch, together with a simplified SDK-style register interface. It is **not a physical switch, NIC driver, ASIC SDK or actual hardware register implementation**.

## Why I built it

To explore the intersection of **C systems programming, Ethernet switching, MAC learning, access VLANs, and software-controlled port configuration**. The project is a concrete prototype of concepts used in network-switch control software.

## Features

- Learns source MAC addresses and associated ingress ports in a finite forwarding database (FDB).
- Forwards known unicast frames only to their learned destination port.
- Floods unknown-unicast, multicast and broadcast frames to eligible ports, never back to ingress.
- Models per-port access VLAN IDs (PVIDs): traffic is isolated by configured VLAN.
- Provides a simulated register read/write API for enabling/disabling ports and changing PVIDs; maintains read-only counters.
- Supports MAC mobility, simple tick-based FDB aging, and bounded FDB capacity.
- Rejects invalid source MACs, invalid ingress ports and invalid register writes.
- Includes automated tests for learning, forwarding, VLAN isolation, register control, invalid inputs and aging.

## Architecture

```text
Synthetic frame (src MAC, dst MAC, ingress port)
                    |
             sw_receive()
                    |
  +-----------------+--------------------+
  |                                      |
Learning FDB                              Port register bank
(MAC, VLAN, port, age)                   (enable mask, PVID)
  |                                      |
  +------------------+-------------------+
                     |
             Forward decision
            (DROP / FLOOD / FORWARD)
                     |
         Egress port bitmask (0x0..0xF)
```

The register interface is deliberately *software-only*: `sw_write_register()` and `sw_read_register()` operate on an ordinary `MiniSwitch` structure. No memory-mapped I/O, FPGA, real Ethernet capture, physical ports, or privileged access occurs.

## Build and run

On Linux, WSL or MSYS2 with GCC and Make:

```bash
make             # Build demo and tests
make test        # Run automated tests
make run         # Run annotated switching scenario
```

Optional on Linux with compatible sanitizer runtime: `make sanitize`.

## Example result

```text
A -> B (B not learned)     | FLOOD   | VLAN 10 | egress mask 0x0A
B -> A (A is learned)      | FORWARD | VLAN 10 | egress mask 0x01
A -> B (B is learned)      | FORWARD | VLAN 10 | egress mask 0x02
C -> B (different VLAN)   | DROP    | VLAN 20 | egress mask 0x00
A -> B (port 2 disabled)  | FLOOD   | VLAN 10 | egress mask 0x08
```

Port mask mapping: `bit0 -> port1`, `bit1 -> port2`, `bit2 -> port3`, `bit3 -> port4`. Port 3 belongs to VLAN 20, while ports 1/2/4 belong to VLAN 10 in this demo.

## Design choices & tradeoffs

- Frames are **already parsed into two MAC addresses**; no CRC, FCS, Ethernet bytes or payload processing.
- Port PVIDs model *untagged access ports*. The simulator does not implement 802.1Q tagging/trunks or STP.
- The FDB is a bounded, linear-search array of 32 entries, emphasizing transparent behavior over scale.
- FDB aging advances on a simulated frame-arrival tick, not wall-clock time.
- Unknown unicast is flooded within the ingress VLAN; a stale entry for a disabled port is never forwarded directly.
- This does not model actual Layer 1 signals, physical hardware register writes, real packet I/O, switching ASIC timing or production SDK APIs.

## Source layout

`include/switch.h` public API, `src/switch.c` switching and register implementation, `src/main.c` demo, `tests/test_switch.c` assertions.

## Interview discussion prompts

Why do switches learn **source** rather than destination addresses? Why can't a switch assume that each `recv()` is one Ethernet frame? What happens when a host moves ports? Why is it important to check the enabled-port mask before forwarding? How might a real switch ASIC expose configuration to an SDK? Where would a hash table outperform the linear FDB search used here?
