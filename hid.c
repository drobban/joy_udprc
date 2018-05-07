#include <sys/types.h>
#include <sys/select.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usbhid.h>

#include "debug.h"

void hid_read(int joy_fd, int report_id, report_desc_t report_desc, u_char *buffer, size_t size)
{
  int read_len = -1;
  struct hid_data *hdata;
  struct hid_item hitem;
  const char *usage_string;

  read_len = read(joy_fd, buffer, size);

  /* Instead of dying, return so we can reset ardupilot comms */
  if (read_len < 0) {
    err(1, "Error: read from fd %d ", joy_fd);
  }
  if (read_len != size) {
    errx(1, "Unexpected message len, should be: %lu is %d", size, read_len);
  }

  hdata = hid_start_parse(report_desc, 1 << hid_input, report_id);
  if (hdata == NULL) {
    errx(1, "Parser failed to start");
  }

  /* Read until end of items */
  while (hid_get_item(hdata, &hitem)) {
    if (hitem.kind == 0 && hitem.usage) {
      usage_string = hid_usage_in_page(hitem.usage);
      debug_printf("Value: %d\t", hitem.report_size);
      debug_printf("kind: %d\t", hitem.kind);
      debug_printf("Current val: %d\t", hid_get_data(buffer, &hitem));
      debug_printf("\t\tUsage: %d \t %s\n", hitem.usage, usage_string);
    }
  }
  hid_end_parse(hdata);
}

int readloop(int joystick_fd, report_desc_t report_desc, int report_id)
{
  struct timeval timeout;
  fd_set input_fds;
  int retval = 0;
  u_char message_buffer[1024];
  size_t report_size;

  report_size = hid_report_size(report_desc, hid_input, report_id);
  if (report_size <= 0) {
    errx(1, "Something went wrong with report size");
  }

  if (sizeof(message_buffer) < report_size) {
    errx(1, "OUT OF BUFFER - %zu\n", report_size);
  }

  debug_printf("Entering into readloop\n");

  /* Read joystick / Send ardupilot */
  while (retval != -1) {
    /* set new timeout */
    timeout = (struct timeval){.tv_sec = 2, .tv_usec = 0};

    FD_SET(joystick_fd, &input_fds);

    retval = select(joystick_fd + 1, &input_fds, NULL, NULL, &timeout);
    if (retval == -1) {
      debug_printf("Select error in readloop()\n");
    } else if (retval) {
      if (FD_ISSET(joystick_fd, &input_fds)) {
        debug_printf("Data available on %d\n", joystick_fd);
        /* Read the available data and process it */
        hid_read(joystick_fd, report_id,
                 report_desc, message_buffer, report_size);
      } else {
        debug_printf("Unexpected, no FD was set\n");
      }
    } else {
      debug_printf("Timeout occured\n");
    }
  }
  return 0;
}
