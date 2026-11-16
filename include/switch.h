#ifndef MINI_SWITCH_H
#define MINI_SWITCH_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define SWITCH_PORTS 4
#define SWITCH_FDB_CAPACITY 32
#define SWITCH_FDB_AGE_TICKS 20u

/* Educational register addresses, NOT physical memory-mapped registers. */
#define REG_PORT_ENABLE 0x000u
#define REG_PORT_PVID_BASE 0x100u /* + 4 * zero-based port */
#define REG_FDB_COUNT 0x200u      /* read-only */
#define REG_FORWARD_COUNT 0x204u  /* read-only */
#define REG_FLOOD_COUNT 0x208u    /* read-only */
#define REG_DROP_COUNT 0x20Cu     /* read-only */

typedef struct { uint8_t bytes[6]; } Mac;
typedef struct { Mac src, dst; } Frame;
typedef enum { ACTION_DROP, ACTION_FORWARD, ACTION_FLOOD } Action;
typedef enum {
    REASON_INVALID_INPUT, REASON_PORT_DISABLED, REASON_INVALID_SOURCE,
    REASON_SAME_PORT, REASON_NO_ELIGIBLE_PORTS, REASON_KNOWN_DESTINATION,
    REASON_UNKNOWN_DESTINATION, REASON_GROUP_DESTINATION
} DecisionReason;

typedef struct {
    Action action;
    DecisionReason reason;
    uint8_t egress_mask; /* bit 0 = port 1, bit 3 = port 4 */
    uint16_t vlan;
} Decision;

typedef struct {
    Mac mac;
    uint16_t vlan;
    uint8_t port; /* 1 .. SWITCH_PORTS */
    uint64_t last_seen_tick;
    bool occupied;
} FdbEntry;

typedef struct {
    uint32_t port_enable_bits;
    uint16_t pvid[SWITCH_PORTS];
    FdbEntry fdb[SWITCH_FDB_CAPACITY];
    uint64_t tick;
    unsigned forward_count, flood_count, drop_count;
} MiniSwitch;

void sw_init(MiniSwitch *sw);
Decision sw_receive(MiniSwitch *sw, int ingress_port, Frame frame);
int sw_write_register(MiniSwitch *sw, uint32_t address, uint32_t value);
int sw_read_register(const MiniSwitch *sw, uint32_t address, uint32_t *out);
size_t sw_fdb_count(const MiniSwitch *sw);
void sw_dump_fdb(const MiniSwitch *sw);
const char *sw_reason_text(DecisionReason reason);

#endif
