/* Author: David Bern
 * Date: 2018-03-17
 * Compile: gcc main.c -o joy_udprc -lusbhid
 *
 * todo:
 * start with parsing arguments. - ok
 * Open device for reading, make some debugs outputs. - ok
 * Read changes. Store all relevant inputs into int array - 10%
 * Make binding functionality. - related to above - To be added in hid.c
 *
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

  fprintf(stderr, " -v{vv}\t\tlog output. Control verbosity with number of v\n");
  fprintf(stderr, " -f [FILE]\tpath or name of uhid\n");
  fprintf(stderr, "Usage example: %s -f device\n", __progname);

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
  struct Config conf;

  /*
    Loglevel,
    1 - Error/Warnings
    2 - Info
    3 - Debug
  */
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


  /* These values should be read from config as names
   * and converted nums in the future
   *
   *axis_dir = -1 make inverted
   */
  conf = (struct Config){
	  .mode_buttons = {(0x9 << 16)|0x1, (0x9 << 16)|0x2,
			   (0x9 << 16)|0x4, (0x9 << 16)|0x5,
			   (0x9 << 16)|0x7, (0x9 << 16)|0x8},
	  .axis = {(0x1 << 16)|0x32, (0x1 << 16)|0x35,
		   (0x2 << 16)|0xC4, (0x1 << 16)|0x30},
	  .axis_dir = {1, -1, 1, 1}};

  /*
   * Implement function for setting up UDP-connection to remote.
   * need to pass remote-connection into readloop for processing.
   */

  readloop(joystick_fd, report_desc, report_id, &conf);

  /* Free report description obtained by hid_get_report_desc() */
  hid_dispose_report_desc(report_desc);
  return 0;
}
