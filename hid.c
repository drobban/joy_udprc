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

/* remove... no need of this structure...  */
struct Report {
  struct usb_ctl_report *buffer;
  enum {srs_uninit, srs_clean, srs_dirty} status;
  int report_id;
  size_t size;
};

int allocate_report(struct Report *report, size_t size)
{
  int calculated_size = (sizeof(*report->buffer) -
                         sizeof(report->buffer->ucr_data) +
                         size);
  if (size) {
    report->buffer = malloc(calculated_size);
    debug_printf("Size of buffer allocated %d\n", calculated_size);
  } else {
    return size;
  }

  if (report->buffer == NULL) {
    return -1;
  }

  report->status = srs_clean;
  return calculated_size;
}

int free_report(struct Report *report)
{
  free(report->buffer);
  report->status = srs_uninit;
}

void hid_read(int j_fd, struct Report *j_rep, int r_id, report_desc_t r_desc)
{
  u_char *message_buffer;
  size_t message_len;
  int read_len = -1;
  struct hid_data *hdata;
  struct hid_item hitem;
  const char *usage_string;

  message_len = j_rep->size;
  message_buffer = j_rep->buffer->ucr_data;

  read_len = read(j_fd, message_buffer, message_len);

  /* Instead of dying, return so we can reset ardupilot comms */
  if (read_len < 0) {
    err(1, "Error: read from fd %d ", j_fd);
  }
  if (read_len != message_len) {
    errx(1, "Unexpected message len, should be: %d is %d",
         message_len, read_len);
  }

  // debug_printf("Size of read: %d\n", read_len);
  // debug_printf("Got:\n%s\n", message_buffer);
  hdata = hid_start_parse(r_desc, 1 << hid_input, r_id);
  if (hdata == NULL) {
    errx(1, "Parser failed to start");
  }

  /* Read until end of items
   * Should be changed to, only get changed data.
   * Makes no sense to iterate the entire list.
   */
  while (hid_get_item(hdata, &hitem)) {
    if (hitem.kind == 0 && hitem.usage) {
      usage_string = hid_usage_in_page(hitem.usage);
      debug_printf("Value: %d\t", hitem.report_size);
      debug_printf("kind: %d\t", hitem.kind);
      debug_printf("Current val: %d\t", hid_get_data(message_buffer, &hitem));
      debug_printf("\t\tUsage: %d \t %s\n", hitem.usage, usage_string);
    }
  }
  hid_end_parse(hdata);
}

int readloop(int joystick_fd, report_desc_t report_desc, int report_id)
{
  int retval = 0;
  struct timeval timeout;
  fd_set input_fds;
  size_t report_size = 0;
  struct Report joystick_report;

  debug_printf("Entering into readloop\n");

  /* Need to find out size of the message from Joystick */
  report_size = hid_report_size(report_desc,
                                hid_input,
                                report_id);

  /* Allocate memory to report */
  // debug_printf("Report size: %d\n", report_size);
  if (report_size <= 0) {
    errx(1, "Something went wrong with report size");
  } else {
    joystick_report.size = report_size;
    if (allocate_report(&joystick_report, report_size) <= 0) {
      errx(1, "Unable to allocate memory to buffer");
    }
  }

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
        // debug_printf("Data available on %d\n", joystick_fd);
        /* Read the available data and process it */
        hid_read(joystick_fd,
                 &joystick_report,
                 report_id,
                 report_desc);
      } else {
        debug_printf("Unexpected, no FD was set\n");
      }
    } else {
      debug_printf("Timeout occured\n");
    }
  }
  free_report(&joystick_report);
  return 0;
}

