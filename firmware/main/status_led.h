#pragma once

/* Blinks the onboard LED on a timer, so a running device can be told apart
 * from a dead one without attaching a serial console. Carries no fault
 * information -- it only means the scheduler is still running. */
void status_led_start(void);
