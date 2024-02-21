/* egg_input.h
 */
 
#ifndef EGG_INPUT_H
#define EGG_INPUT_H

/* Enable, disable, or query an input feature.
 *
 * QUERY to return the current state of the feature without changing anything.
 * DISABLE to turn it off, that is the default state for all features.
 * ENABLE to turn it on if available and return ENABLE.
 * If not available, leaves it off and returns DISABLE.
 * eg keyboard and mouse features will not enable on guiless systems that only do generic input.
 * REQUIRE to turn a feature on with platform-provided emulation if necessary.
 * Platforms are required to emulate keyboard and mouse, though not necessarily at the same time.
 * REQUIRE may return ERROR if we can't turn that on right now.
 *
 * Enabling mbutton or mmotion makes the cursor visible.
 * mbutton also enables mwheel, since they're roughly the same thing.
 *
 * jbutton enables only 2-state buttons, and may map analogue axes and hats or ignore them.
 * jbutton_all requests that all raw input from joysticks be reported with its natural values, via EGG_EVENT_TYPE_JBUTTON.
 * janalogue requests EGG_EVENT_TYPE_JANALOGUE with normalized values.
 *
 * If an accelerometer is available and enabled, it will appear as its own joystick device.
 */
int egg_input_use_key(int usage);
int egg_input_use_text(int usage);
int egg_input_use_mmotion(int usage);
int egg_input_use_mbutton(int usage);
int egg_input_use_jbutton(int usage);
int egg_input_use_jbutton_all(int usage);
int egg_input_use_janalogue(int usage);
int egg_input_use_accelerometer(int usage);

#define EGG_INPUT_USAGE_ERROR   -2
#define EGG_INPUT_USAGE_QUERY   -1
#define EGG_INPUT_USAGE_DISABLE  0
#define EGG_INPUT_USAGE_ENABLE   1
#define EGG_INPUT_USAGE_REQUIRE  2

//TODO Need hooks to read device capabilities.

#endif
