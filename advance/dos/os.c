/*
 * This file is part of the AdvanceMAME project.
 *
 * Copyright (C) 2001 Andrea Mazzoleni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Andrea Mazzoleni
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

#include "os.h"
#include "device.h"

#include <signal.h>
#include <process.h>
#include <conio.h>
#include <crt0.h>
#include <stdio.h>
#include <dos.h>
#include <dir.h>
#include <sys/exceptn.h>
#include <go32.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

struct mouse_context {
	int skip;
	int x;
	int y;
	unsigned buttons;
};

/* Number of universal buffer */
#define UNIVERSAL_MAX 4

/* Size of the universal buffer */
#define UNIVERSAL_SIZE OS_MAXLIST

struct os_context {
	char file_legacy[OS_MAXPATH];
	char file_home[OS_MAXPATH];

	char universal_map[UNIVERSAL_MAX][UNIVERSAL_SIZE];
	unsigned universal_mac;

	int mouse_id;
	struct mouse_context mouse[2];
	unsigned mouse_shift;
	unsigned mouse_count;

	int mouse2_button;

	int key_id;

	int joy_id;
	int joy_calibration_target;
	int joy_calibration_first;

	FILE* msg;
	int msg_sync_flag;

#ifdef USE_CONFIG_ALLEGRO_WRAPPER
	/**
	 * Enable the Allegro compatibility.
	 * This option enable the redirection of all the Allegro configuration option
	 * in the current API.
	 * You must simply set this variable at the configuration context to use
	 * before calling any Allegro functions.
	 * Link with --wrap get_config_string --wrap get_config_int --wrap set_config_string --wrap set_config_int --wrap get_config_id --wrap set_config_id.
	 */
	struct conf_context* allegro_conf;
#endif
};

static struct os_context OS;

#define KEY_TYPE_NONE 0
#define KEY_TYPE_AUTO 1

#define MOUSE_TYPE_NONE 0
#define MOUSE_TYPE_AUTO 1

/***************************************************************************/
/* Second mouse */

static int mouse2_init(void) {
	__dpmi_regs r;

	r.x.ax = 100;
        __dpmi_int(0x33, &r);
	if (r.x.ax == 100 || r.x.ax == 0)
		return -1;

	return 0;
}

static void mouse2_get(int* x, int* y) {
	__dpmi_regs r;

	r.x.ax = 103;
	__dpmi_int(0x33, &r);

	OS.mouse2_button = r.x.bx;

	r.x.ax = 111;
	__dpmi_int(0x33, &r);

	*x = (short)r.x.cx;
	*y = (short)r.x.dx;
}

/***************************************************************************/
/* Allegro configuration compatibility */

#ifdef USE_CONFIG_ALLEGRO_WRAPPER

const char* __wrap_get_config_string(const char *section, const char *name, const char *def) {
	char allegro_name[256];
	const char* result;

	os_log(("allegro: get_config_string(%s,%s,%s)\n",section,name,def));

	/* disable the emulation of the third mouse button */
	if (strcmp(name,"emulate_three")==0)
		return "no";

	if (!OS.allegro_conf)
		return def;
	sprintf(allegro_name,"allegro_%s",name);
	conf_section_set(OS.allegro_conf, 0, 0);
	if (conf_is_registered(OS.allegro_conf, allegro_name)
		&& conf_string_get(OS.allegro_conf, allegro_name, &result) == 0)
		return result;
	else
		return def;
}

int __wrap_get_config_int(const char *section, const char *name, int def) {
	char allegro_name[256];
	const char* result;

	os_log(("allegro: get_config_int(%s,%s,%d)\n",section,name,def));

	if (!OS.allegro_conf)
		return def;
	conf_section_set(OS.allegro_conf, 0, 0);
	sprintf(allegro_name,"allegro_%s",name);
	if (conf_is_registered(OS.allegro_conf, allegro_name)
		&& conf_string_get(OS.allegro_conf, allegro_name, &result) == 0)
		return atoi(result);
	else
		return def;
}

int __wrap_get_config_id(const char *section, const char *name, int def) {
	char allegro_name[256];
	const char* result;

	os_log(("allegro: get_config_id(%s,%s,%d)\n",section,name,def));

	if (!OS.allegro_conf)
		return def;
	conf_section_set(OS.allegro_conf, 0,0);
	sprintf(allegro_name,"allegro_%s",name);
	if (conf_is_registered(OS.allegro_conf, allegro_name)
		&& conf_string_get(OS.allegro_conf, allegro_name,&result) == 0)
	{
		unsigned v;
		v = ((unsigned)(unsigned char)result[3]);
		v |= ((unsigned)(unsigned char)result[2]) << 8;
		v |= ((unsigned)(unsigned char)result[1]) << 16;
		v |= ((unsigned)(unsigned char)result[0]) << 24;
		return v;
	} else
		return def;
}

void __wrap_set_config_string(const char *section, const char *name, const char *val) {
	char allegro_name[128];

	os_log(("allegro: set_config_string(%s,%s,%s)\n",section,name,val));

	if (!OS.allegro_conf)
		return;
	sprintf(allegro_name,"allegro_%s",name);
	if (val) {
		if (!conf_is_registered(OS.allegro_conf, allegro_name))
			conf_string_register(OS.allegro_conf, allegro_name);
		conf_string_set(OS.allegro_conf, "", allegro_name, val); /* ignore error */
	} else {
		conf_remove(OS.allegro_conf, "", allegro_name);
	}
}

void __wrap_set_config_int(const char *section, const char *name, int val) {
	char allegro_name[128];
	char buffer[16];

	os_log(("allegro: set_config_int(%s,%s,%d)\n",section,name,val));

	if (!OS.allegro_conf)
		return;
	sprintf(allegro_name,"allegro_%s",name);
	if (!conf_is_registered(OS.allegro_conf, allegro_name))
		conf_string_register(OS.allegro_conf, allegro_name);
	sprintf(buffer,"%d",val);
	conf_string_set(OS.allegro_conf, "", allegro_name, buffer); /* ignore error */
}

void __wrap_set_config_id(const char *section, const char *name, int val) {
	char allegro_name[128];
	char buffer[16];

	os_log(("allegro: set_config_id(%s,%s,%d)\n",section,name,val));

	if (!OS.allegro_conf)
		return;
	sprintf(allegro_name,"allegro_%s",name);
	if (!conf_is_registered(OS.allegro_conf, allegro_name))
		conf_string_register(OS.allegro_conf, allegro_name);
	buffer[3] = ((unsigned)val) & 0xFF;
	buffer[2] = (((unsigned)val) >> 8) & 0xFF;
	buffer[1] = (((unsigned)val) >> 16) & 0xFF;
	buffer[0] = (((unsigned)val) >> 24) & 0xFF;
	buffer[4] = 0;
	conf_string_set(OS.allegro_conf, "", allegro_name, buffer); /* ignore error */
}

#endif

/***************************************************************************/
/* Debug */

void os_msg_va(const char *text, va_list arg) {
	if (OS.msg) {
		vfprintf(OS.msg,text,arg);
		if (OS.msg_sync_flag) {
			fflush(OS.msg);
			sync();
		}
	}
}

void os_msg(const char *text, ...) {
	va_list arg;
	va_start(arg, text);
	os_msg_va(text, arg);
	va_end(arg);
}

int os_msg_init(const char* file, int sync_flag) {
	OS.msg_sync_flag = sync_flag;
	if (file) {
		OS.msg = fopen(file,"w");
		if (!OS.msg)
			return -1;
	}

	return 0;
}

void os_msg_done(void) {
	if (OS.msg) {
		fclose(OS.msg);
		OS.msg = 0;
	}
}

/***************************************************************************/
/* Clock */

#if !defined(NDEBUG)
#define USE_TICKER_FIXED 350000000
#endif

os_clock_t OS_CLOCKS_PER_SEC;

#ifndef USE_TICKER_FIXED
static void ticker_measure(os_clock_t* map, unsigned max) {
	clock_t start;
	clock_t stop;
	os_clock_t tstart;
	os_clock_t tstop;
	unsigned mac;

	mac = 0;
	start = clock();
	do {
		stop = clock();
	} while (stop == start);

	start = stop;
	tstart = os_clock();
	while (mac < max) {
		do {
			stop = clock();
		} while (stop == start);

		tstop = os_clock();

		map[mac] = (tstop - tstart) * CLOCKS_PER_SEC / (stop - start);
		++mac;

		start = stop;
		tstart = tstop;
	}
}

static int ticker_cmp(const void *e1, const void *e2) {
	const os_clock_t* t1 = (const os_clock_t*)e1;
	const os_clock_t* t2 = (const os_clock_t*)e2;

	if (*t1 < *t2) return -1;
	if (*t1 > *t2) return 1;
	return 0;
}
#endif

static void os_clock_setup(void) {
#ifdef USE_TICKER_FIXED
	/* only for debugging */
	OS_CLOCKS_PER_SEC = USE_TICKER_FIXED;
#else
	os_clock_t v[7];
	double error;
	int i;

	ticker_measure(v,7);

	qsort(v,7,sizeof(os_clock_t),ticker_cmp);

	for(i=0;i<7;++i)
		os_log(("os: clock estimate %g\n",(double)v[i]));

	OS_CLOCKS_PER_SEC = v[3]; /* median value */

	if (v[0])
		error = (v[6] - v[0]) / (double)v[0];
	else
		error = 0;

	os_log(("os: select clock %g (err %g%%)\n", (double)v[3], error * 100.0));
#endif
}

/***************************************************************************/
/* Init */

int os_init(struct conf_context* context) {
	memset(&OS,0,sizeof(OS));

	OS.universal_mac = 0;

#ifdef USE_CONFIG_ALLEGRO_WRAPPER
	OS.allegro_conf = context;
#endif

	return 0;
}

void os_done(void) {
#ifdef USE_CONFIG_ALLEGRO_WRAPPER
	OS.allegro_conf = 0;
#endif
}

static void os_align(void) {
	char* m[32];
	unsigned i;

	/* align test */
	for(i=0;i<32;++i) {
		m[i] = (char*)malloc(i);
		if (((unsigned)m[i]) & 0x7)
			os_log(("ERROR: unaligned malloc ptr:%08p, size:%d\n", m[i], i));
	}

	for(i=0;i<32;++i) {
		free(m[i]);
	}
}

int os_inner_init(void) {

	os_clock_setup();

	if (allegro_init() != 0)
		return -1;

	os_align();

	/* set some signal handlers */
	signal(SIGABRT, os_signal);
	signal(SIGFPE, os_signal);
	signal(SIGILL, os_signal);
	signal(SIGINT, os_signal);
	signal(SIGSEGV, os_signal);
	signal(SIGTERM, os_signal);
	signal(SIGHUP, os_signal);
	signal(SIGKILL, os_signal);
	signal(SIGPIPE, os_signal);
	signal(SIGQUIT, os_signal);
	signal(SIGUSR1, os_signal); /* used for malloc failure */

	return 0;
}

void os_inner_done(void) {
	allegro_exit();
}

void os_poll(void) {
	if (OS.joy_id != JOY_TYPE_NONE) {
		poll_joystick();
	}

	if (keyboard_needs_poll())
		poll_keyboard();
}

/***************************************************************************/
/* Keyboard */

device OS_KEY[] = {
	{ "none", KEY_TYPE_NONE, "No keyboard" },
	{ "auto", KEY_TYPE_AUTO, "Automatic detection" },
	{ 0, 0, 0 }
};

int os_key_init(int key_id, int disable_special)
{
	OS.key_id = key_id;

	if (OS.key_id != KEY_TYPE_NONE) {
		if (install_keyboard() != 0)
			return -1;
	}

	if (disable_special) {
		/* disable BREAK (CTRL+C) and QUIT (CTRL+\) in protect mode */
		__djgpp_set_ctrl_c(0);

		/* disable BREAK in real mode */
		_go32_want_ctrl_break(1);

		/* disable the Allegro CTRL+ALT+END and CTRL+BREAK */
		three_finger_flag = 0;
	}

	return 0;
}

void os_key_done(void)
{
	if (OS.key_id != KEY_TYPE_NONE) {
		remove_keyboard();
	}

	OS.key_id = KEY_TYPE_NONE;
}

/***************************************************************************/
/* Input */

int os_input_init(void) {
	return 0;
}

void os_input_done(void) {
}

int os_input_hit(void) {
	return kbhit();
}

unsigned os_input_get(void) {
	return getkey();
}

/***************************************************************************/
/* Mouse */

device OS_MOUSE[] = {
	{ "auto", MOUSE_TYPE_AUTO, "Automatic detection" },
	{ "none", MOUSE_TYPE_NONE, "No mouse" },
	{ 0, 0, 0 }
};

int os_mouse_init(int mouse_id)
{
	OS.mouse_shift = 0;
	OS.mouse_count = 0;

	os_log(("os: mouse_init(mouse_id:%d)\n",mouse_id));

	OS.mouse_id = mouse_id;

	if (OS.mouse_id != MOUSE_TYPE_NONE) {
		int err = install_mouse();
		if (err != -1) {
			OS.mouse[OS.mouse_count].buttons = err;
			++OS.mouse_count;
			os_log(("os: Allegro mouse found\n"));
		} else {
			os_log(("os: Allegro mouse NOT found\n"));
			++OS.mouse_shift;
		}
		if (mouse2_init() == 0) {
			OS.mouse[OS.mouse_count].buttons = 2;
			++OS.mouse_count;
			os_log(("os: Secondary mouse found\n"));
		} else {
			os_log(("os: Secondary mouse NOT found\n"));
		}
	}

	return 0;
}

void os_mouse_done(void)
{
	os_log(("os: mouse_done()\n"));

	if (OS.mouse_id != MOUSE_TYPE_NONE) {
		remove_mouse();
	}

	OS.mouse_id = MOUSE_TYPE_NONE;
}

unsigned os_mouse_count_get(void)
{
	return OS.mouse_count;
}

unsigned os_mouse_button_count_get(unsigned mouse)
{
	return OS.mouse[mouse].buttons;
}

void os_mouse_pos_get(unsigned mouse, int* x, int* y)
{
	if (OS.mouse[mouse].skip) {
		*x = OS.mouse[mouse].x;
		*y = OS.mouse[mouse].y;
		OS.mouse[mouse].skip = 0;
	} else {
		switch (mouse + OS.mouse_shift) {
		case 0 :
			get_mouse_mickeys(&OS.mouse[mouse].x,&OS.mouse[mouse].y);
			break;
		case 1 :
			mouse2_get(&OS.mouse[mouse].x,&OS.mouse[mouse].y);
			break;
		}
		*x = OS.mouse[mouse].x/2;
		*y = OS.mouse[mouse].y/2;
		OS.mouse[mouse].x -= *x;
		OS.mouse[mouse].y -= *y;
		OS.mouse[mouse].skip = 1;
	}
}

unsigned os_mouse_button_get(unsigned mouse, unsigned button)
{
	switch (mouse + OS.mouse_shift) {
		case 0 : return (mouse_b & (1 << button)) != 0;
		case 1 : return (OS.mouse2_button & (1 << button)) != 0;
	}

	return 0;
}

/***************************************************************************/
/* Joystick */

device OS_JOY[] = {
	{ "auto", JOY_TYPE_AUTODETECT, "Automatic detection" },
	{ "none", JOY_TYPE_NONE, "No joystick" },
	{ "standard", JOY_TYPE_STANDARD, "Standard joystick" },
	{ "dual", JOY_TYPE_2PADS , "Dual joysticks" },
	{ "4button", JOY_TYPE_4BUTTON, "4-button joystick" },
	{ "6button", JOY_TYPE_6BUTTON, "6-button joystick" },
	{ "8button", JOY_TYPE_8BUTTON, "8-button joystick" },
	{ "fspro", JOY_TYPE_FSPRO, "CH Flightstick Pro" },
	{ "wingex", JOY_TYPE_WINGEX, "Logitech Wingman Extreme" },
	{ "sidewinder", JOY_TYPE_SIDEWINDER, "Sidewinder" },
	{ "sidewinderag", JOY_TYPE_SIDEWINDER_AG, "Sidewinder Aggressive" },
	{ "gamepadpro", JOY_TYPE_GAMEPAD_PRO, "GamePad Pro" },
	{ "grip", JOY_TYPE_GRIP, "GrIP" },
	{ "grip4",JOY_TYPE_GRIP4, "GrIP 4-way" },
	{ "sneslpt1", JOY_TYPE_SNESPAD_LPT1, "SNESpad LPT1" },
	{ "sneslpt2", JOY_TYPE_SNESPAD_LPT2, "SNESpad LPT2" },
	{ "sneslpt3", JOY_TYPE_SNESPAD_LPT3, "SNESpad LPT3" },
/*
 *      Based on sample code by Earle F. Philhower, III. from DirectPad
 *      Pro 4.9, for use with the DirectPad Pro parallel port interface.
 *
 *      See <http://www.ziplabel.com/dpadpro> for interface details.
 *
 *
 *       -----------------------
 *      | o o o | o o o | o o o | Controller Port (looking into plug)
 *       \_____________________/
 *        1 2 3   4 5 6   7 8 9
 *
 *      Controller          Parallel
 *
 *      1 - data            10 (conport 1, 3, 4, 5, 6), 13 (conport 2)
 *      2 - command         2
 *      3 - 9V(shock)       +9V power supply terminal for Dual Shock
 *      4 - gnd             18,19 also -9V and/or -5V power supply terminal
 *      5 - V+              5, 6, 7, 8, 9 through diodes, or +5V power supply terminal
 *      6 - att             3 (conport 1, 2), 5 (conport 3), 6 (conport 4), 7 (conport 5), 8 (conport 6)
 *      7 - clock           4
 *      9 - ack             12 (conport 1, 3, 4, 5, 6), 14 (conport 2) **
 *
 *      ** There is an error in the DirectPad Pro documentation, which states that
 *         the second control port should have pin 9 connected to pin 15 on the
 *         parallel port. This should actually be pin 14 on the parallel port. To
 *         make things more confusing, this error is unlikely to prevent the
 *         interface from working properly. It's also possible that a change to the
 *         scanning code has been made in version 5.0 to prevent people from having
 *         to rewire their interfaces.
 *
 */
	{ "psxlpt1", JOY_TYPE_PSXPAD_LPT1, "PSXpad LPT1" },
	{ "psxlpt2", JOY_TYPE_PSXPAD_LPT2, "PSXpad LPT2" },
	{ "psxlpt3", JOY_TYPE_PSXPAD_LPT3, "PSXpad LPT3" },
/*
 *
 *      This driver supports upto four N64 controllers. The analog stick
 *      calibrates itself when the controller is powered up (in hardware).
 *      There is some autodetection code included, but it's unused as it's
 *      unreliable. Care to take a look?
 *
 *      This driver is for the N64 pad -> parallel port interface developed
 *      by Stephan Hans and Simon Nield, supported by DirectPad Pro 4.9.
 *
 *      See http://www.st-hans.de/N64.htm for interface information, and
 *      See http://www.ziplabel.com for DirectPad Pro information.
 */
	{ "n64lpt1", JOY_TYPE_N64PAD_LPT1, "N64pad LPT1" },
	{ "n64lpt2", JOY_TYPE_N64PAD_LPT2, "N64pad LPT2" },
	{ "n64lpt3", JOY_TYPE_N64PAD_LPT3, "N64pad LPT3" },
/*
 *
 *      Supports 2 two-button joysticks. Port 1 is compatible with Linux
 *      joy-db9 driver (multisystem 2-button), and port 2 is compatible
 *       with Atari interface for DirectPad Pro.
 *
 *      Based on joy-db9 driver for Linux by Vojtech Pavlik
 *      and on Atari interface for DirectPad Pro by Earle F. Philhower, III
 *
 *      Interface pinout
 *
 *
 *      Parallel port                           Joystick port 1
 *       1----------------------------------------------------1
 *      14----------------------------------------------------2
 *      16----------------------------------------------------3
 *      17----------------------------------------------------4
 *      11----------------------------------------------------6
 *      12----------------------------------------------------7 (button 2)
 *      18----------------------------------------------------8
 *
 *                                              Joystick port 2
 *       2----------------------------------------------------1
 *       3----------------------------------------------------2
 *       4----------------------------------------------------3
 *       5----------------------------------------------------4
 *       6----------------------------------------------------6
 *       7----------------------------------------------------7 (button 2)
 *      19----------------------------------------------------8
 *
 *      Advantages
 *
 *      * Simpler to build (no diodes required)
 *      * Autofire will work (if joystick supports it)
 *
 *      Drawbacks
 *
 *      * The parallel port must be in SPP (PS/2) mode in order for this
 *        driver to work. In Normal mode, port 2 won't work because data
 *        pins are not inputs. In EPP/ECP PS/2 mode, directions for
 *        port 1 won't work (buttons will) beacuse control pins are not
 *        inputs.
 *
 *       * The parallel port should not require pull-up resistors
 *         (however, newer ones don't)
 *
 */
	{ "db9lpt1", JOY_TYPE_DB9_LPT1, "DB9 LPT1" },
	{ "db9lpt2", JOY_TYPE_DB9_LPT2, "DB9 LPT2" },
	{ "db9lpt3", JOY_TYPE_DB9_LPT3, "DB9 LPT3" },
/*
 *
 *      Supports up to 7 joysticks, each one with up to 5 buttons.
 *
 *      Original interface and driver by Steffen Schwenke
 *      See <http://www.burg-halle.de/~schwenke/parport.html> for details
 *      on how to build the interface
 *
 *      Advantages
 *
 *      * Exploits the parallel port to its limits
 *
 *      Drawbacks
 *
 *      * Autofire will not work
 */
	{ "tgxlpt1", JOY_TYPE_TURBOGRAFX_LPT1, "TGX-LPT1" },
	{ "tgxlpt2", JOY_TYPE_TURBOGRAFX_LPT2, "TGX LPT2" },
	{ "tgxlpt3", JOY_TYPE_TURBOGRAFX_LPT3, "TGX LPT3" },
	{ "segaisa", JOY_TYPE_IFSEGA_ISA, "IF-SEGA/ISA" },
	{ "segapci", JOY_TYPE_IFSEGA_PCI, "IF-SEGA2/PCI" },
	{ "segapcifast", JOY_TYPE_IFSEGA_PCI_FAST, "IF-SEGA2/PCI (normal)" },
	{ "wingwarrior", JOY_TYPE_WINGWARRIOR, "Wingman Warrior" },
	{ 0, 0, 0 }
};

int os_joy_init(int joy_id)
{
	OS.joy_id = joy_id;

	if (OS.joy_id != JOY_TYPE_NONE) {
		os_log(("os: joystick load calibration data\n"));
		if (load_joystick_data(0) != 0) {
			os_log(("os: joystick error loading calibration data, try reinizializing\n"));
			if (install_joystick(OS.joy_id) != 0) {
				os_log(("os: joystick initialization failed\n"));
				return -1;
			}
		}
	}

	return 0;
}

void os_joy_done(void)
{
	if (OS.joy_id != JOY_TYPE_NONE) {
		remove_joystick();
	}

	OS.joy_id = JOY_TYPE_NONE;
}

void os_joy_calib_start(void)
{
	if (OS.joy_id != JOY_TYPE_NONE) {
		os_log(("os: joystick calibration start\n"));

		remove_joystick();
		install_joystick(OS.joy_id);

		OS.joy_calibration_target = 0;
		OS.joy_calibration_first = 1;
	}
}

const char* os_joy_calib_next(void)
{
	if (OS.joy_id != JOY_TYPE_NONE) {

		while (OS.joy_calibration_target < num_joysticks) {

			if (joy[OS.joy_calibration_target].flags & JOYFLAG_CALIBRATE) {
				if (!OS.joy_calibration_first) {
					if (calibrate_joystick(OS.joy_calibration_target) != 0) {
						os_log(("os: joystick error in calibration %d\n",OS.joy_calibration_target));
						/* stop on error */
						return 0;
					}
				}

				OS.joy_calibration_first = 0;

				if (joy[OS.joy_calibration_target].flags & JOYFLAG_CALIBRATE) {
					const char* msg;

					msg = calibrate_joystick_name(OS.joy_calibration_target);
					os_log(("os: joystick calibration msg %s\n",msg));
					return msg;
				}
			} else {
				/* next joystick */
				OS.joy_calibration_target++;
				OS.joy_calibration_first = 1;
			}
		}

		os_log(("os: joystick saving calibration data\n"));

		save_joystick_data(0);
	}

	return 0;
}

/***************************************************************************/
/* Library */

void os_mode_reset(void) {
	/* Restore the default text mode */
	__dpmi_regs r;
	r.x.ax = 0x3;
	__dpmi_int(0x10, &r);
}

/**
 * Wait some time.
 * The BIOS keyboard buffer is cleared.
 */
void os_idle(void) {
#if 0 /* not required in DOS and bad sound in Windows */
	__dpmi_yield();
#endif
	/* clear the keyboard BIOS buffer */
	while (kbhit())
		getkey();
}

/**
 * Wait no more than the specified number of us.
 */
void os_usleep(unsigned us) {
	/* no wait */
}

/**
 * Play a short error sound.
 */
void os_sound_error(void) {
	sound(100);
	delay(120);
	nosound();
}

/**
 * Play a short warning sound.
 */
void os_sound_warn(void) {
	sound(900);
	delay(10);
	nosound();
}

/**
 * Play a long notify sound.
 */
void os_sound_signal(void) {
	unsigned i;
	for(i=0;i<10;++i) {
		sound(800);
		delay(10);
		nosound();
		delay(10);
	}
}

/**
 * Shutdown the system.
 * \return ==0 (or never return) if success
 */
int os_apm_shutdown(void) {
	__dpmi_regs regs;

	sync();

	/* APM detect */

	regs.x.ax = 0x5300;
	regs.x.bx = 0x0000;
	__dpmi_int(0x15,&regs);
	if ((regs.x.flags & 1)!=0 || regs.x.bx != 0x504D) {
		/* APM BIOS not found */
		return -1;
	}

	if (regs.x.ax < 0x102) {
		/* APM BIOS too old */
		return -1;
	}

	/* APM connection */
	regs.x.ax = 0x5301;
	regs.x.bx = 0x0000;
	__dpmi_int(0x15,&regs);
	if ((regs.x.flags & 1) != 0) {
		/* APM real mode connection failed */
		return -1;
	}

	/* APM notify version */
	regs.x.ax = 0x530E;
	regs.x.bx = 0x0000;
	regs.x.cx = 0x0102;
	__dpmi_int(0x15,&regs);
	if ((regs.x.flags & 1) != 0) {
		/* APM notify version failed */
	}

	/* APM off */
	regs.x.ax = 0x5307;
	regs.x.bx = 0x0001;
	regs.x.cx = 0x0003;
	__dpmi_int(0x15,&regs);

	return -1;
}

/**
 * Put the system in standby mode.
 * \return ==0 if success
 */
int os_apm_standby(void) {
	unsigned mode;
	__dpmi_regs r;

	r.x.ax = 0x4F10;
	r.h.bl = 0x00;
	r.x.dx = 0;
	r.x.es = 0;

	__dpmi_int(0x10, &r);
	if (r.x.ax!=0x004F) {
		return -1;
	}

	if (r.h.bh & 0x4) {
		mode = 0x4; /* off */
	} else if (r.h.bh & 0x2) {
		mode = 0x2; /* suspend */
	} else if (r.h.bh & 0x1) {
		mode = 0x1; /* standby */
	} else {
		return -1;
	}

	r.x.ax = 0x4F10;
	r.h.bl = 0x01;
	r.h.bh = mode;

	__dpmi_int(0x10, &r);
	if (r.x.ax!=0x004F) {
		return -1;
	}

	return 0;
}

/**
 * Restore the system after a standby.
 * \return ==0 if success
 */
int os_apm_wakeup(void) {
	__dpmi_regs r;

	r.x.ax = 0x4F10;
	r.h.bl = 0x00;
	r.x.dx = 0;
	r.x.es = 0;

	__dpmi_int(0x10, &r);
	if (r.x.ax!=0x004F) {
		return -1;
	}

	r.x.ax = 0x4F10;
	r.h.bl = 0x01;
	r.h.bh = 0x00;

	__dpmi_int(0x10, &r);
	if (r.x.ax!=0x004F) {
		return -1;
	}

	return 0;
}

int os_system(const char* cmd) {
	int r;
	__djgpp_exception_toggle();
	r = system(cmd);
	__djgpp_exception_toggle();
	return r;
}

int os_spawn(const char* file, const char** argv) {
	int r;
	__djgpp_exception_toggle();
	r = spawnvp(P_WAIT,file,(char**)argv);
	__djgpp_exception_toggle();
	return r;
}

char os_dir_separator(void) {
	return ';';
}

char os_dir_slash(void) {
	return '\\';
}

static char* import_conv(char* dst_begin, char* dst_end, const char* begin, const char* end) {
	int need_slash;

	if (end - begin == 2 && begin[1]==':') {
		/* only drive spec */
		memcpy(dst_begin,"/dos/",5);
		dst_begin += 5;
		*dst_begin++ = tolower(begin[0]);
		begin += 2;
		need_slash = 1;
	} else if (end - begin >= 3 && begin[1]==':' && (begin[2]=='\\' || begin[2]=='/')) {
		/* absolute path */
		memcpy(dst_begin,"/dos/",5);
		dst_begin += 5;
		*dst_begin++ = tolower(begin[0]);
		begin += 3;
		need_slash = 1;
	} else if (end - begin >= 3 && begin[1]==':') {
		/* drive + relative path, assume it's an absolute path */
		memcpy(dst_begin,"/dos/",5);
		dst_begin += 5;
		*dst_begin++ = tolower(begin[0]);
		begin += 2;
		need_slash = 1;
	} else if (end - begin >= 1 && (begin[0]=='\\' || begin[0]=='/')) {
		/* absolute path on the current drive */
		memcpy(dst_begin,"/dos/",5);
		dst_begin += 5;
		*dst_begin++ = getdisk() + 'a';
		begin += 1;
		need_slash = 1;
	} else {
		/* relative path, keep it relative */
		need_slash = 0;
	}

	/* add a slash if something other is specified */
	if (need_slash && begin != end) {
		*dst_begin++ = '/';
	}

	/* copy the remaining path */
	while (begin != end) {
		if (*begin=='\\')
			*dst_begin++ = '/';
		else
			*dst_begin++ = tolower(*begin);
		++begin;
	}

	return dst_begin;
}

static char* export_conv(char* dst_begin, char* dst_end, const char* begin, const char* end) {

	if (end - begin == 6 && memcmp(begin,"/dos/",5)==0) {
		/* root dir */
		*dst_begin++ = begin[5];
		*dst_begin++ = ':';
		*dst_begin++ = '\\';
		begin += 6;
	} else if (end - begin >= 7 && memcmp(begin,"/dos/",5)==0) {
		/* absolute path */
		*dst_begin++ = begin[5];
		*dst_begin++ = ':';
		*dst_begin++ = '\\';
		begin += 7;
	} else {
		/* relative path */
	}

	/* copy the remaining path */
	while (begin != end) {
		if (*begin=='/')
			*dst_begin++ = '\\';
		else
			*dst_begin++ = *begin;
		++begin;
	}

	return dst_begin;
}

const char* os_import(const char* path) {
	char* buffer = OS.universal_map[OS.universal_mac];
	const char* begin = path;
	char* dst_begin = buffer;
	char* dst_end = buffer + UNIVERSAL_SIZE;

	OS.universal_mac = (OS.universal_mac + 1) % UNIVERSAL_MAX;

	while (*begin) {
		const char* end = strchr(begin,';');
		if (!end)
			end = begin + strlen(begin);
		if (dst_begin != buffer)
			*dst_begin++ = ':';
		dst_begin = import_conv(dst_begin, dst_end, begin, end);
		begin = end;
		if (*begin)
			++begin;
	}

	*dst_begin = 0;

	return buffer;
}

const char* os_export(const char* path) {
	char* buffer = OS.universal_map[OS.universal_mac];
	const char* begin = path;
	char* dst_begin = buffer;
	char* dst_end = buffer + UNIVERSAL_SIZE;

	OS.universal_mac = (OS.universal_mac + 1) % UNIVERSAL_MAX;

	while (*begin) {
		const char* end = strchr(begin,':');
		if (!end)
			end = begin + strlen(begin);
		if (dst_begin != buffer)
			*dst_begin++ = ';';
		dst_begin = export_conv(dst_begin, dst_end, begin, end);
		begin = end;
		if (*begin)
			++begin;
	}

	*dst_begin = 0;

	return buffer;
}

/***************************************************************************/
/* Files */

const char* os_config_file_root(const char* file) {
	return 0;
}

const char* os_config_file_home(const char* file) {
	sprintf(OS.file_home,"%s",file);
	return OS.file_home;
}

const char* os_config_file_legacy(const char* file) {
	sprintf(OS.file_legacy,"%s",file);
	return OS.file_legacy;
}

const char* os_config_dir_multidir(const char* tag) {
	return tag;
}

const char* os_config_dir_singledir(const char* tag) {
	return tag;
}

const char* os_config_dir_singlefile(void) {
	return ".";
}

/***************************************************************************/
/* Main */

void os_default_signal(int signum)
{
	os_log(("os: signal %d\n",signum));

#if defined(USE_VIDEO_SVGALINE) || defined(USE_VIDEO_VBELINE) || defined(USE_VIDEO_VBE)
	os_log(("os: video_abort\n"));
	{
		extern void video_abort(void);
		video_abort();
	}
#endif

#if defined(USE_SOUND_SEAL) || defined(USE_SOUND_ALLEGRO)
	os_log(("os: sound_abort\n"));
	{
		extern void sound_abort(void);
		sound_abort();
	}
#endif

	os_mode_reset();

	os_log(("os: close log\n"));

	os_msg_done();

	if (signum == SIGINT) {
		cprintf("Break pressed\n\r");
		exit(EXIT_FAILURE);
	} else if (signum == SIGQUIT) {
		cprintf("Quit pressed\n\r");
		exit(EXIT_FAILURE);
	} else if (signum == SIGUSR1) {
		cprintf("Low memory\n\r");
		_exit(EXIT_FAILURE);
	} else {
		cprintf("AdvanceMAME signal %d.\n", signum);
		cprintf("%s, %s\n\r", __DATE__, __TIME__);

		if (signum == SIGILL) {
			cprintf("Are you using the correct binary ?\n\r");
		}

		/* stack traceback */
		__djgpp_traceback_exit(signum);

		_exit(EXIT_FAILURE);
	}
}

static int os_fixed(void)
{
	__dpmi_regs r;
	void* t;

	/* Fix an alignment problem of the DJGPP Library 2.03 (refresh) */
	t = sbrk(0);
	if (((unsigned)t & 0x7) == 4) {
		sbrk(4);
	}

	/* Don't allow NT */
	r.x.ax = 0x3306;
	__dpmi_int(0x21, &r);
	if (r.x.bx == ((50 << 8) | 5)) {
		cprintf("Windows NT/2000/XP not supported. Please upgrade to Linux.\n\r");
		return -1;
	}

	return 0;
}

/* Lock all the DATA, BSS and STACK segments. (see the DJGPP faq point 18.9) */
int _crt0_startup_flags = _CRT0_FLAG_NONMOVE_SBRK | _CRT0_FLAG_LOCK_MEMORY | _CRT0_FLAG_NEARPTR;

int main(int argc, char* argv[])
{
	/* allocate the HEAP in the pageable memory */
	_crt0_startup_flags &= ~_CRT0_FLAG_LOCK_MEMORY;

	if (os_fixed() != 0)
		return EXIT_FAILURE;

	if (os_main(argc,argv) != 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

