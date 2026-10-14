/* JoyOrbit Open Golf adapter, GPL-3.0-or-later. */
#ifndef JOYORBIT_GOLF_CONTROLS_H
#define JOYORBIT_GOLF_CONTROLS_H
#include <stdbool.h>
typedef struct sapp_event sapp_event;
void joyorbit_golf_pad_axes(float x,float y,float rx,float scroll,float hat_x,float hat_y);
bool joyorbit_golf_pad_key(int key,bool down,bool long_press);
void joyorbit_golf_control_event(const sapp_event *event);
bool joyorbit_golf_control_frame(float dt);
void joyorbit_golf_control_draw(void);
bool joyorbit_golf_is_paused(void);
void joyorbit_golf_save(void);
#endif
