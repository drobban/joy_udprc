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
#include "hid.h"
#include "com.h"

#define AMAX 65535.0f
#define PMIN 1100
#define PMAX 1900

#define MAX(X, Y) ((X > Y) ? X : Y)

uint16_t axistopwm(int16_t);
int      locate_items(report_desc_t, int[], hid_item_t[], size_t, int);
int      hidtopwm(int, u_char*, size_t, struct rc_packet*,
		  hid_item_t[], size_t, hid_item_t[], size_t, int[]);

/*
 * Turns Axis values between 0x0 -> 0xFFFF into a converted
 * "PWM"-value between 1100 -> 1900, inverted axis can be handled
 * by just multiplying it with -1.
 */
uint16_t
axistopwm(int16_t value)
{
	return (uint16_t)(((PMAX - PMIN) / AMAX) * (uint16_t)value + PMIN);
}

/*
 * Takes a list of usage-nums and sets a list of corresponding items for
 * each usage.
 * Returns amount of located items. should be equal to len on success.
 */
int
locate_items(report_desc_t d, int usages[], hid_item_t items[],
    size_t len, int id)
{
	int i;

	for (i = 0; i < len; i++) {
		if (hid_locate(d, usages[i], hid_input, &items[i], id) == 0)
			return 0;
	}

	return i;
}

/*
 * Turns values fetched from hid-items into "PWM"-values
 * Returns 1 on success.
 */
int
hidtopwm(int joy_fd, u_char *buffer, size_t size,
    struct rc_packet *udp_data,
    hid_item_t button_items[], size_t nbuttons,
    hid_item_t axis_items[], size_t naxis,
    int dir[])
{
	int i;
	int j;
	int read_len = -1;
	uint16_t aval;

	read_len = read(joy_fd, buffer, size);
	debug_printf("Read: %d \n", read_len);

	/* Instead of dying, return so we can reset ardupilot comms */
	if (read_len < 0) {
		memset(udp_data->pwms,0x0, sizeof(udp_data->pwms));
		return -1;
		/* err(1, "Error: read from fd %d ", joy_fd); */
		/* return error instead. make sure we clean up */
	}

	if (read_len != size) {
		errx(1, "Unexpected message len, should be: %lu is %d",
		    size, read_len);
	}

	/* Convert buttons into modes */
	for (i = 0; i < nbuttons; i++) {
		if (hid_get_data(buffer, &button_items[i]))
			udp_data->pwms[MODE] = mode_pwm_vals[i];
	}
	/* Convert axis */
	for (j = 0; j < naxis; j++) {
		aval = hid_get_data(buffer, &axis_items[j]);
		udp_data->pwms[j] = axistopwm(dir[j] * MAX(aval, 1));
	}

	return 1;
}

/*
 * Main loop,
 * Fetching values from HID.
 * transforms the data and sends it to remote via UDP
 */
int
readloop(int joystick_fd, report_desc_t report_desc,
    int report_id, struct Config *conf)
{
	int i;
	int status = 0;
	hid_item_t axis_items[AXIS_SIZE];
	hid_item_t button_items[NMODES];
	struct timeval timeout;
	fd_set input_fds;
	int retval = 0;
	u_char message_buffer[MAX_MSG_BUFF];
	size_t report_size;
	struct rc_packet udp_data;

	/* Init udp_data with default values */
	memset(&udp_data, 0x0, sizeof(udp_data));
	memcpy(udp_data.pwms, DEFAULT_PWM, sizeof(udp_data.pwms));

	/* set new timeout */
	timeout = (struct timeval){.tv_sec = 2, .tv_usec = 0};

	/*
	 * Need to implement function for sending data to remote.
	 * Also needs to keep track of timestamps and more.
	 * look at struct rc_packet definition.
	 */

	report_size = hid_report_size(report_desc, hid_input, report_id);
	if (report_size <= 0) {
		errx(1, "Something went wrong with report size");
	}

	if (sizeof(message_buffer) < report_size) {
		errx(1, "OUT OF BUFFER - %zu\n", report_size);
	}

	/* Locate all the button and axis items */
	locate_items(report_desc, conf->axis,
	    axis_items, AXIS_SIZE, report_id);
	locate_items(report_desc, conf->mode_buttons,
	    button_items, NMODES, report_id);

	/* Read joystick / Send ardupilot */
	while (retval != -1) {
		FD_SET(joystick_fd, &input_fds);

		retval = select(joystick_fd + 1, &input_fds, NULL, NULL, &timeout);
		if (retval == -1) {
			debug_printf("Select error in readloop()\n");
		} else if (retval) {
			if (FD_ISSET(joystick_fd, &input_fds)) {
				debug_printf("Data available on %d\n",
				    joystick_fd);

				status = hidtopwm(joystick_fd, message_buffer,
				    report_size, &udp_data,
				    button_items, NMODES,
				    axis_items, AXIS_SIZE,
				    conf->axis_dir);

				if (status == -1) {
					debug_printf("Joystick disconnected");
					/* Send blank "signal" to ardupilot */
					return -1;
				}
				/* Send data to ardupilot */
			} else {
				debug_printf("Unexpected, no FD was set\n");
			}
		}
		for (i = 0; i < COM_NUM_CHANNELS; i++) {
			debug_printf("%s : %d\n", CHANNEL_NAMES[i],
			    udp_data.pwms[i]);
		}
	}
	return 0;
}
