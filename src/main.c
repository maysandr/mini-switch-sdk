#include "switch.h"
#include <stdio.h>

static void show(const char *step, MiniSwitch *sw, int port, Mac src, Mac dst) {
    Decision decision = sw_receive(sw, port, (Frame){src, dst});
    const char *action = decision.action == ACTION_FORWARD ? "FORWARD" :
                         decision.action == ACTION_FLOOD ? "FLOOD" : "DROP";
    printf("%-28s | %-7s | VLAN %u | egress mask 0x%02X | %s\n",
           step, action, decision.vlan, decision.egress_mask,
           sw_reason_text(decision.reason));
}

int main(void) {
    MiniSwitch sw;
    const Mac a = {{0x02, 0, 0, 0, 0, 0xAA}};
    const Mac b = {{0x02, 0, 0, 0, 0, 0xBB}};
    const Mac c = {{0x02, 0, 0, 0, 0, 0xCC}};
    sw_init(&sw);

    /* 1, 2 and 4 on VLAN 10; port 3 isolated on VLAN 20. */
    sw_write_register(&sw, REG_PORT_PVID_BASE + 0u, 10);
    sw_write_register(&sw, REG_PORT_PVID_BASE + 4u, 10);
    sw_write_register(&sw, REG_PORT_PVID_BASE + 8u, 20);
    sw_write_register(&sw, REG_PORT_PVID_BASE + 12u, 10);

    puts("Mini Switch SDK - educational software-only demonstration");
    puts("Egress mask: bit0=port1, bit1=port2, bit2=port3, bit3=port4");
    puts("--------------------------------------------------------------------------");
    show("A -> B (B not learned)", &sw, 1, a, b);
    show("B -> A (A is learned)", &sw, 2, b, a);
    show("A -> B (B is learned)", &sw, 1, a, b);
    show("C -> B (different VLAN)", &sw, 3, c, b);
    sw_write_register(&sw, REG_PORT_ENABLE, 0x0Du); /* disable port 2 */
    show("A -> B (port 2 disabled)", &sw, 1, a, b);
    sw_dump_fdb(&sw);

    uint32_t count = 0;
    if (sw_read_register(&sw, REG_FDB_COUNT, &count) == 0)
        printf("\nFDB entries: %u | Forward: %u | Flood: %u | Drop: %u\n",
               count, sw.forward_count, sw.flood_count, sw.drop_count);
    return 0;
}
