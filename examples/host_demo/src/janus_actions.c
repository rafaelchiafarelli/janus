#include "janus_actions.gen.h"
#include "janus_bindings.gen.h"

#include <stdio.h>

/* Host-only demo: flips the relay's own bound state and reports it, in
 * place of the real hardware write ArduinoIHM's own MultiplexedBus driver
 * does for the same action names (toggle_relay_0.. toggle_relay_7 —
 * relay.screen.yaml's on_press, one per relay.screen.yaml toggle widget). */
static void toggle_relay(int *field, int index) {
    *field = !*field;
    printf("  -> relay %d %s\n", index, *field ? "ON" : "OFF");
}

/* Same for the PWM screen's toggles (pwm.screen.yaml's on_press,
 * toggle_pwm_<field>): flips the bound field in place of the real
 * MultiOutput channel reconfiguration. */
static void toggle_pwm(int *field, const char *name) {
    *field = !*field;
    printf("  -> pwm %s %s\n", name, *field ? "ON" : "OFF");
}

void janus_handle_action(janus_action_t action) {
    switch (action) {
        case JANUS_ACTION_TOGGLE_RELAY_0: toggle_relay(&relay_instance.relay_0, 0); break;
        case JANUS_ACTION_TOGGLE_RELAY_1: toggle_relay(&relay_instance.relay_1, 1); break;
        case JANUS_ACTION_TOGGLE_RELAY_2: toggle_relay(&relay_instance.relay_2, 2); break;
        case JANUS_ACTION_TOGGLE_RELAY_3: toggle_relay(&relay_instance.relay_3, 3); break;
        case JANUS_ACTION_TOGGLE_RELAY_4: toggle_relay(&relay_instance.relay_4, 4); break;
        case JANUS_ACTION_TOGGLE_RELAY_5: toggle_relay(&relay_instance.relay_5, 5); break;
        case JANUS_ACTION_TOGGLE_RELAY_6: toggle_relay(&relay_instance.relay_6, 6); break;
        case JANUS_ACTION_TOGGLE_RELAY_7: toggle_relay(&relay_instance.relay_7, 7); break;
        case JANUS_ACTION_TOGGLE_PWM_CH0_ENABLED: toggle_pwm(&pwm_instance.ch0_enabled, "ch0_enabled"); break;
        case JANUS_ACTION_TOGGLE_PWM_CH0_INVERTING: toggle_pwm(&pwm_instance.ch0_inverting, "ch0_inverting"); break;
        case JANUS_ACTION_TOGGLE_PWM_CH1_ENABLED: toggle_pwm(&pwm_instance.ch1_enabled, "ch1_enabled"); break;
        case JANUS_ACTION_TOGGLE_PWM_CH1_INVERTING: toggle_pwm(&pwm_instance.ch1_inverting, "ch1_inverting"); break;
        case JANUS_ACTION_TOGGLE_PWM_CH2_A_ENABLED: toggle_pwm(&pwm_instance.ch2_a_enabled, "ch2_a_enabled"); break;
        case JANUS_ACTION_TOGGLE_PWM_CH2_B_ENABLED: toggle_pwm(&pwm_instance.ch2_b_enabled, "ch2_b_enabled"); break;
        case JANUS_ACTION_TOGGLE_PWM_CH2_C_ENABLED: toggle_pwm(&pwm_instance.ch2_c_enabled, "ch2_c_enabled"); break;
        case JANUS_ACTION_TOGGLE_PWM_CH3_A_ENABLED: toggle_pwm(&pwm_instance.ch3_a_enabled, "ch3_a_enabled"); break;
        case JANUS_ACTION_TOGGLE_PWM_CH3_B_ENABLED: toggle_pwm(&pwm_instance.ch3_b_enabled, "ch3_b_enabled"); break;
        case JANUS_ACTION_TOGGLE_PWM_CH3_C_ENABLED: toggle_pwm(&pwm_instance.ch3_c_enabled, "ch3_c_enabled"); break;
        default: break;
    }
}
