/* Author: David Bern
 * Date: 2018-03-17
 */

#include <sys/types.h>

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
#include <stdarg.h>

#include <sys/select.h>

#define N_JOYSTICKS 1


struct Report {
	struct usb_ctl_report *buffer;

	enum {srs_uninit, srs_clean, srs_dirty} status;
	int report_id;
	size_t size;
};

/* todo:
 * start with parsing arguments. - ok
 * Open device for reading, make some debugs outputs. - 50% need to read data
 * Read changes.
 * Make binding functionality.
 */


/* Examine this ones use
 * Its an array with struct elements.
 * Used to fetch options to hid_report_size
 * Make this less magical.
 */
static struct {
	int uhid_report;
	hid_kind_t hid_kind;
	char const *name;
} const reptoparam[] = {
#define REPORT_INPUT 0
	{ UHID_INPUT_REPORT, hid_input, "input" },
#define REPORT_OUTPUT 1
	{ UHID_OUTPUT_REPORT, hid_output, "output" },
#define REPORT_FEATURE 2
	{ UHID_FEATURE_REPORT, hid_feature, "feature" }
#define REPORT_MAXVAL 2
};



unsigned int verbose;

static void usage(void)
{
  extern char * __progname;

  fprintf(stderr, "Usage: %s -f device\n", __progname);

  exit(1);
}

/* Enable debug prints by setting verbose variable */
static void debug_printf(const char *fmt, ...)
{
  va_list arglist;
  if (verbose) {
    va_start(arglist, fmt);
    vprintf(fmt, arglist);
    va_end(arglist);
  }
}

static int allocate_report(struct Report *report, size_t size)
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

static int readloop(int joystick_fd, report_desc_t report_desc, int report_id)
{
  u_char *message_buffer;
  size_t message_len, read_len;
  int retval = 0;
  struct timeval timeout;
  fd_set input_fds;
  size_t report_size = 0;
  struct Report joystick_report;

  debug_printf("Entering into readloop\n");

  /* Need to find out size of the message from Joystick */
  report_size = hid_report_size(report_desc,
                                reptoparam[REPORT_INPUT].hid_kind,
                                report_id);

  /* Allocate memory to report */
  debug_printf("Report size: %d\n", report_size);
  if (report_size <= 0) {
    errx(1, "Something went wrong with report size");
  } else {
    joystick_report.size = report_size;
    if (allocate_report(&joystick_report, report_size) <= 0) {
      errx(1, "Unable to allocate memory to buffer");
    }
  }
	message_len = joystick_report.size;
	message_buffer = joystick_report.buffer->ucr_data;


  while (retval != -1) {
    read_len = -1;

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
        read_len = read(joystick_fd, message_buffer, message_len);
        debug_printf("Got:\n%s\n", message_buffer);
      } else {
        debug_printf("Unexpected, no FD was set\n");
      }
    } else {
      debug_printf("Timeout occured\n");
    }
  }

  return 0;
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
		errx(1, "USB_GET_REPORT_DESC");
  }

  readloop(joystick_fd, report_desc, report_id);

  /* Free report description obtained by hid_get_report_desc() */
	hid_dispose_report_desc(report_desc);
  return 0;
}
