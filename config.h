#include <sys/types.h>
#include "com.h"

#ifndef CONFIG_H
#define CONFIG_H

#define MAX_NAME_LEN 40

struct Config
{
	int mode_buttons[NMODES];
	int axis[AXIS_SIZE];
	int axis_conf[AXIS_SIZE];
	int axis_dir[AXIS_SIZE];
};

#endif
