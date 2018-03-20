/* Author: David Bern
 * Date: 2018-03-17
 * Compile: gcc main.c -o joy_udprc -lusbhid
 *
 * todo:
 * start with parsing arguments. - ok
 * Open device for reading, make some debugs outputs. - ok
 * Read changes. Store all relevant inputs into int array - 10%
 * Make binding functionality. - related to above
 * Wrap error handling with cleanup routine
 */

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

#include "hid.h"
#include "debug.h"

/* Future feature.... wait with this one
 * #define N_JOYSTICKS 1
 */

void usage()
{
  extern char * __progname;

  fprintf(stderr, "Usage: %s -f device\n", __progname);

  exit(1);
}

int main(int argc, char **argv)
{
  char const *dev = NULL;
  char const *table = NULL;
  char name_buf[PATH_MAX];
  int ch;
  int joystick_fd;
  report_desc_t report_desc;
  static int report_id = 0;

  verbose = 0;

  while ((ch = getopt(argc, argv, "hf:v")) != -1) {
    switch (ch) {
    case 'f':
      dev = optarg;
      break;
    case 'v':
      verbose++;
      break;
    case 'h':
    default:
      usage();
    }
  }

  /* Check if optarg had some to give */
  if (dev == NULL) {
    usage();
  }

  /* Init with default table */
  if (hid_start(table) == -1) {
    errx(1, "hid_init");
  }

  /* lets fill in the blanks for the lazy */
	if (dev[0] != '/') {
    snprintf(name_buf, sizeof(name_buf), "/dev/%s%s",
             isdigit((unsigned char)dev[0]) ? "uhid" : "", dev);
    dev = name_buf;
  }

  /* In the future in would be nice to have some sort of force feedback
   * When that happens, we need to open the device for writing as well
   */
  joystick_fd = open(dev, O_RDONLY);
  if (joystick_fd < 0) {
    err(1, "%s", dev);
  }


  if (ioctl(joystick_fd, USB_GET_REPORT_ID, &report_id) < 0) {
    report_id = -1;
  }

  debug_printf("report ID=%d\n", report_id);

	report_desc = hid_get_report_desc(joystick_fd);
  if (report_desc == 0) {
    errx(1, "Unable to obtain USB report description");
  }

  readloop(joystick_fd, report_desc, report_id);

  /* Free report description obtained by hid_get_report_desc() */
  hid_dispose_report_desc(report_desc);
  return 0;
}
