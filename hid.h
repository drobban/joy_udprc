#include "config.h"

#ifndef MAX_MSG_BUFF
#define MAX_MSG_BUFF 1024
#endif

#ifndef MAX_IO_BUFF
#define MAX_IO_BUFF 256
#endif

#ifndef HID_H
#define HID_H
int hid_read(int joy_fd, struct hid_data *hdata, u_char *buffer, size_t size);
int readloop(int joystick_fd, report_desc_t report_desc, int report_id, struct Config *conf);
#endif
