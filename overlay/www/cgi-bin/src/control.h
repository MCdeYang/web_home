// control.h
#ifndef CONTROL_H
#define CONTROL_H

int parse_state_from_json(const char *json_body);

int set_light_state(int on);
int set_aircon_state(int on);
int set_washing_machine_state(int on);
int set_fan_state(int on);
int set_door_state(int unlocked);


#endif // CONTROL_H