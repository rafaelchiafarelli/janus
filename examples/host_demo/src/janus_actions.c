#include "janus_actions.gen.h"

#include <stdio.h>

void janus_handle_action(janus_action_t action) {
    switch (action) {
        case JANUS_ACTION_REBOOT: printf("  -> device rebooting...\n"); break;
        default: break;
    }
}
