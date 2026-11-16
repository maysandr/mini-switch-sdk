#include "switch.h"

#include <stdio.h>
#include <string.h>

static bool mac_equals(Mac a, Mac b) {
    return memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}

static bool is_group_address(Mac mac) {
    return (mac.bytes[0] & 0x01u) != 0u; /* broadcast is a group address */
}

static bool is_zero_mac(Mac mac) {
    const Mac zero = {{0, 0, 0, 0, 0, 0}};
    return mac_equals(mac, zero);
}

static uint8_t port_bit(unsigned port) {
    return (uint8_t)(1u << (port - 1u));
}

static bool port_is_enabled(const MiniSwitch *sw, unsigned port) {
    return (sw->port_enable_bits & port_bit(port)) != 0u;
}

static void age_fdb(MiniSwitch *sw) {
    for (size_t i = 0; i < SWITCH_FDB_CAPACITY; ++i) {
        FdbEntry *entry = &sw->fdb[i];
        if (entry->occupied && sw->tick - entry->last_seen_tick >= SWITCH_FDB_AGE_TICKS) {
            entry->occupied = false;
        }
    }
}

static FdbEntry *find_entry(MiniSwitch *sw, Mac mac, uint16_t vlan) {
    for (size_t i = 0; i < SWITCH_FDB_CAPACITY; ++i) {
        FdbEntry *entry = &sw->fdb[i];
        if (entry->occupied && entry->vlan == vlan && mac_equals(entry->mac, mac)) {
            return entry;
        }
    }
    return NULL;
}

static void learn_source(MiniSwitch *sw, Mac mac, uint16_t vlan, uint8_t port) {
    FdbEntry *entry = find_entry(sw, mac, vlan);
    if (entry == NULL) {
        size_t selected = 0;
        bool empty_found = false;
        for (size_t i = 0; i < SWITCH_FDB_CAPACITY; ++i) {
            if (!sw->fdb[i].occupied) {
                selected = i;
                empty_found = true;
                break;
            }
            if (sw->fdb[i].last_seen_tick < sw->fdb[selected].last_seen_tick) {
                selected = i;
            }
        }
        (void)empty_found; /* If full, replace the least recently learned entry. */
        entry = &sw->fdb[selected];
    }
    entry->occupied = true;
    entry->mac = mac;
    entry->vlan = vlan;
    entry->port = port;
    entry->last_seen_tick = sw->tick;
}

static uint8_t flood_mask(const MiniSwitch *sw, unsigned ingress, uint16_t vlan) {
    uint8_t mask = 0;
    for (unsigned p = 1; p <= SWITCH_PORTS; ++p) {
        if (p != ingress && port_is_enabled(sw, p) && sw->pvid[p - 1] == vlan) {
            mask = (uint8_t)(mask | port_bit(p));
        }
    }
    return mask;
}

static Decision finish(MiniSwitch *sw, Action action, DecisionReason reason, uint8_t mask, uint16_t vlan) {
    Decision d = { action, reason, mask, vlan };
    if (action == ACTION_FORWARD) ++sw->forward_count;
    else if (action == ACTION_FLOOD) ++sw->flood_count;
    else ++sw->drop_count;
    return d;
}

void sw_init(MiniSwitch *sw) {
    if (sw == NULL) return;
    memset(sw, 0, sizeof(*sw));
    sw->port_enable_bits = (1u << SWITCH_PORTS) - 1u;
    for (int i = 0; i < SWITCH_PORTS; ++i) sw->pvid[i] = 1;
}

Decision sw_receive(MiniSwitch *sw, int ingress_port, Frame frame) {
    if (sw == NULL) {
        Decision invalid = { ACTION_DROP, REASON_INVALID_INPUT, 0, 0 };
        return invalid;
    }
    if (ingress_port < 1 || ingress_port > SWITCH_PORTS)
        return finish(sw, ACTION_DROP, REASON_INVALID_INPUT, 0, 0);

    ++sw->tick;
    age_fdb(sw);
    uint16_t vlan = sw->pvid[ingress_port - 1];
    if (!port_is_enabled(sw, (unsigned)ingress_port))
        return finish(sw, ACTION_DROP, REASON_PORT_DISABLED, 0, vlan);
    if (is_zero_mac(frame.src) || is_group_address(frame.src))
        return finish(sw, ACTION_DROP, REASON_INVALID_SOURCE, 0, vlan);

    learn_source(sw, frame.src, vlan, (uint8_t)ingress_port);

    if (is_group_address(frame.dst)) {
        uint8_t mask = flood_mask(sw, (unsigned)ingress_port, vlan);
        return mask ? finish(sw, ACTION_FLOOD, REASON_GROUP_DESTINATION, mask, vlan)
                    : finish(sw, ACTION_DROP, REASON_NO_ELIGIBLE_PORTS, 0, vlan);
    }

    FdbEntry *target = find_entry(sw, frame.dst, vlan);
    if (target != NULL) {
        if (target->port == (unsigned)ingress_port)
            return finish(sw, ACTION_DROP, REASON_SAME_PORT, 0, vlan);
        if (port_is_enabled(sw, target->port) && sw->pvid[target->port - 1] == vlan)
            return finish(sw, ACTION_FORWARD, REASON_KNOWN_DESTINATION, port_bit(target->port), vlan);
        /* Stale destination entry: never forward into a disabled/different-VLAN port. */
    }
    uint8_t mask = flood_mask(sw, (unsigned)ingress_port, vlan);
    return mask ? finish(sw, ACTION_FLOOD, REASON_UNKNOWN_DESTINATION, mask, vlan)
                : finish(sw, ACTION_DROP, REASON_NO_ELIGIBLE_PORTS, 0, vlan);
}

size_t sw_fdb_count(const MiniSwitch *sw) {
    size_t count = 0;
    if (sw != NULL) {
        for (size_t i = 0; i < SWITCH_FDB_CAPACITY; ++i)
            if (sw->fdb[i].occupied) ++count;
    }
    return count;
}

int sw_write_register(MiniSwitch *sw, uint32_t address, uint32_t value) {
    if (sw == NULL) return -1;
    if (address == REG_PORT_ENABLE) {
        if ((value & ~((1u << SWITCH_PORTS) - 1u)) != 0u) return -1;
        sw->port_enable_bits = value;
        return 0;
    }
    if (address >= REG_PORT_PVID_BASE && address < REG_PORT_PVID_BASE + 4u * SWITCH_PORTS &&
        ((address - REG_PORT_PVID_BASE) % 4u) == 0u) {
        if (value < 1u || value > 4094u) return -1;
        unsigned port = (address - REG_PORT_PVID_BASE) / 4u + 1u;
        sw->pvid[port - 1u] = (uint16_t)value;
        /* Invalidate FDB entries learned on a port when its access VLAN changes. */
        for (size_t i = 0; i < SWITCH_FDB_CAPACITY; ++i)
            if (sw->fdb[i].occupied && sw->fdb[i].port == port)
                sw->fdb[i].occupied = false;
        return 0;
    }
    return -1;
}

int sw_read_register(const MiniSwitch *sw, uint32_t address, uint32_t *out) {
    if (sw == NULL || out == NULL) return -1;
    if (address == REG_PORT_ENABLE) *out = sw->port_enable_bits;
    else if (address == REG_FDB_COUNT) *out = (uint32_t)sw_fdb_count(sw);
    else if (address == REG_FORWARD_COUNT) *out = sw->forward_count;
    else if (address == REG_FLOOD_COUNT) *out = sw->flood_count;
    else if (address == REG_DROP_COUNT) *out = sw->drop_count;
    else if (address >= REG_PORT_PVID_BASE && address < REG_PORT_PVID_BASE + 4u * SWITCH_PORTS &&
             ((address - REG_PORT_PVID_BASE) % 4u) == 0u)
        *out = sw->pvid[(address - REG_PORT_PVID_BASE) / 4u];
    else return -1;
    return 0;
}

void sw_dump_fdb(const MiniSwitch *sw) {
    if (sw == NULL) return;
    puts("\nForwarding Database (MAC | VLAN | Port)");
    puts("---------------------------------------");
    for (size_t i = 0; i < SWITCH_FDB_CAPACITY; ++i) {
        const FdbEntry *e = &sw->fdb[i];
        if (!e->occupied) continue;
        for (unsigned j = 0; j < 6; ++j) {
            printf("%02X%s", e->mac.bytes[j], j == 5 ? "" : ":");
        }
        printf(" | %4u | %u\n", e->vlan, e->port);
    }
}

const char *sw_reason_text(DecisionReason reason) {
    switch (reason) {
        case REASON_INVALID_INPUT: return "invalid input";
        case REASON_PORT_DISABLED: return "ingress port disabled";
        case REASON_INVALID_SOURCE: return "invalid source MAC";
        case REASON_SAME_PORT: return "destination on ingress port";
        case REASON_NO_ELIGIBLE_PORTS: return "no eligible egress ports";
        case REASON_KNOWN_DESTINATION: return "known destination";
        case REASON_UNKNOWN_DESTINATION: return "unknown unicast destination";
        case REASON_GROUP_DESTINATION: return "broadcast/multicast destination";
        default: return "unknown";
    }
}
