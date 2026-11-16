# Learning guide for the follow-along build

This is a map for studying the **completed project** later, not an instruction to copy code blindly.

## 1. Know what enters and leaves the simulator

Input: a synthetic Ethernet frame containing source/destination MAC addresses and a numbered ingress port. Output: `DROP`, `FORWARD` or `FLOOD`, plus an egress-port bitmask. There is no actual Ethernet interface, frame capture, packet serialization or Layer 1 hardware.

## 2. Follow the `sw_receive()` decision path

- Reject disabled ports, invalid port numbers or invalid source MAC addresses.
- Advance the simulated tick and expire old FDB entries.
- Learn the **source** address and incoming VLAN/port.
- If destination is a broadcast or multicast address, flood to all eligible ports in the same access VLAN (except ingress).
- If destination is in the FDB, forward to its learned port if enabled and in the same VLAN. Never send back to ingress.
- Otherwise flood unknown unicast to eligible ports.

## 3. Understand the register model

`REG_PORT_ENABLE` is a four-bit enable mask; writing 0x0D disables port 2. `REG_PORT_PVID_BASE + 4*N` sets the access VLAN for zero-based port N. This API resembles register read/write control, but operates on a normal C struct, not physical hardware.

## 4. Why tests matter

The tests prove initial configuration, learning, known unicast, unknown unicast, same-port drop, broadcast flood, VLAN isolation, changing VLAN invalidates learned entries, disabled ports, invalid inputs, MAC mobility, aging and counters.

## 5. Questions a reviewer might ask

- Why is a switch's forwarding database keyed by both VLAN and MAC address?
- Why learn from the source MAC, rather than the destination?
- What changes when the destination is multicast or unknown unicast?
- What is the difference between a simulated register bank and memory-mapped device registers?
- Why does a real switch SDK program hardware rather than forward every packet in a CPU loop?
- Why is linear FDB lookup fine for a demonstration but not for production scale?

## Scope limits

No PHY or signal handling, no Layer 3 IP routing, no 802.1Q tag bytes or trunk configuration, no spanning tree, no physical register access, no chip driver. A reviewer should see the project as a thoughtful educational model, not a production SDK.
