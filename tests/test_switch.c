#include "switch.h"
#include <assert.h>
#include <stdio.h>

#define MAC(last) ((Mac){{0x02, 0, 0, 0, 0, (last)}})
#define FRAME(a,b) ((Frame){MAC(a),MAC(b)})

static int count = 0;
static void ok(void) { ++count; }

static void test_initial_and_register_access(void) {
    MiniSwitch sw; sw_init(&sw); uint32_t v = 0;
    assert(sw_read_register(&sw, REG_PORT_ENABLE, &v) == 0 && v == 15);
    assert(sw_read_register(&sw, REG_PORT_PVID_BASE, &v) == 0 && v == 1);
    assert(sw_write_register(&sw, REG_PORT_ENABLE, 16) == -1);
    assert(sw_write_register(&sw, REG_PORT_PVID_BASE, 4095) == -1);
    assert(sw_write_register(&sw, REG_FDB_COUNT, 123) == -1);
    assert(sw_read_register(&sw, 0x999, &v) == -1); ok();
}

static void test_unknown_learn_known(void) {
    MiniSwitch sw; sw_init(&sw);
    Decision d = sw_receive(&sw, 1, FRAME(1,2));
    assert(d.action == ACTION_FLOOD && d.egress_mask == 0x0E && sw_fdb_count(&sw)==1); ok();
    d = sw_receive(&sw, 2, FRAME(2,1));
    assert(d.action == ACTION_FORWARD && d.egress_mask == 0x01 && sw_fdb_count(&sw)==2); ok();
    d = sw_receive(&sw, 1, FRAME(1,2));
    assert(d.action == ACTION_FORWARD && d.egress_mask == 0x02); ok();
}

static void test_same_port_and_group(void) {
    MiniSwitch sw; sw_init(&sw);
    (void)sw_receive(&sw, 1, FRAME(1,2));
    Decision d = sw_receive(&sw, 1, FRAME(2,1));
    assert(d.action == ACTION_DROP && d.reason == REASON_SAME_PORT); ok();
    const Mac broadcast = {{255,255,255,255,255,255}};
    d = sw_receive(&sw, 1, (Frame){MAC(3),broadcast});
    assert(d.action == ACTION_FLOOD && d.egress_mask == 0x0E); ok();
}

static void test_vlan_isolation_and_invalidation(void) {
    MiniSwitch sw; sw_init(&sw);
    assert(sw_write_register(&sw, REG_PORT_PVID_BASE + 8, 20) == 0);
    Decision d = sw_receive(&sw, 1, FRAME(1,2));
    assert(d.egress_mask == 0x0A && d.vlan == 1); ok();
    d = sw_receive(&sw, 3, FRAME(2,1));
    assert(d.action == ACTION_DROP && d.vlan == 20 && d.egress_mask == 0); ok();
    assert(sw_write_register(&sw, REG_PORT_PVID_BASE + 8, 1) == 0);
    assert(sw_fdb_count(&sw) == 1); ok();
}

static void test_disabled_and_invalid_ports(void) {
    MiniSwitch sw; sw_init(&sw);
    assert(sw_write_register(&sw, REG_PORT_ENABLE, 0x09) == 0);
    Decision d = sw_receive(&sw, 1, FRAME(1,2));
    assert(d.action == ACTION_FLOOD && d.egress_mask == 0x08); ok();
    d = sw_receive(&sw, 2, FRAME(2,1));
    assert(d.action == ACTION_DROP && d.reason == REASON_PORT_DISABLED); ok();
    d = sw_receive(&sw, 5, FRAME(2,1));
    assert(d.action == ACTION_DROP && d.reason == REASON_INVALID_INPUT); ok();
}

static void test_invalid_source(void) {
    MiniSwitch sw; sw_init(&sw);
    const Mac allzero = {{0,0,0,0,0,0}};
    const Mac multicast = {{0x01,0,0,0,0,1}};
    Decision d = sw_receive(&sw, 1, (Frame){allzero,MAC(2)});
    assert(d.reason == REASON_INVALID_SOURCE && sw_fdb_count(&sw)==0); ok();
    d = sw_receive(&sw, 1, (Frame){multicast,MAC(2)});
    assert(d.reason == REASON_INVALID_SOURCE && sw_fdb_count(&sw)==0); ok();
}

static void test_mac_move(void) {
    MiniSwitch sw; sw_init(&sw);
    (void)sw_receive(&sw, 1, FRAME(1,2));
    (void)sw_receive(&sw, 2, FRAME(1,3)); /* 1 moved to port 2 */
    Decision d = sw_receive(&sw, 3, FRAME(3,1));
    assert(d.action == ACTION_FORWARD && d.egress_mask==0x02 && sw_fdb_count(&sw)==2); ok();
}

static void test_ageing(void) {
    MiniSwitch sw; sw_init(&sw);
    (void)sw_receive(&sw, 1, FRAME(1,2));
    for (unsigned i=0; i<SWITCH_FDB_AGE_TICKS; ++i) {
        (void)sw_receive(&sw, 2, FRAME(2,3));
    }
    Decision d = sw_receive(&sw, 3, FRAME(3,1));
    assert(d.action == ACTION_FLOOD && d.egress_mask == 0x0B); ok();
}

static void test_counters(void) {
    MiniSwitch sw; sw_init(&sw); uint32_t v;
    (void)sw_receive(&sw,1,FRAME(1,2));
    (void)sw_receive(&sw,2,FRAME(2,1));
    (void)sw_receive(&sw,1,FRAME(1,1));
    assert(sw_read_register(&sw,REG_FLOOD_COUNT,&v)==0 && v==1);
    assert(sw_read_register(&sw,REG_FORWARD_COUNT,&v)==0 && v==1);
    assert(sw_read_register(&sw,REG_DROP_COUNT,&v)==0 && v==1); ok();
}

int main(void) {
    test_initial_and_register_access();
    test_unknown_learn_known();
    test_same_port_and_group();
    test_vlan_isolation_and_invalidation();
    test_disabled_and_invalid_ports();
    test_invalid_source();
    test_mac_move();
    test_ageing();
    test_counters();
    printf("PASS: %d assertions/scenarios across 9 test groups\n", count);
    return 0;
}
