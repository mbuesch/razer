/*
 *   Razer daemon
 *   Daemon to keep track of Razer device state.
 *
 *   Copyright (C) 2008 Michael Buesch <mb@bu3sch.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include "librazer.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>


#define VAR_RUN			"/var/run"
#define VAR_RUN_RAZERD		VAR_RUN "/razerd"
#define PIDFILE			VAR_RUN_RAZERD "/razerd.pid"
#define CTLPIPE			VAR_RUN_RAZERD "/control.pipe"


/* Filedescriptor of the open control pipe. */
static int ctlpipe = -1;


static void cleanup_var_run(void)
{
	close(ctlpipe);
	ctlpipe = -1;
	unlink(CTLPIPE);
	unlink(PIDFILE);
	rmdir(VAR_RUN_RAZERD);
}

static int setup_var_run(void)
{
	int fd, err;
	ssize_t ssize;
	char buf[32] = { 0, };

	/* Create /var/run subdirectory. */
	err = mkdir(VAR_RUN_RAZERD, 0755);
	if (err && errno != EEXIST) {
		fprintf(stderr, "Failed to create directory %s: %s\n",
			VAR_RUN_RAZERD, strerror(errno));
		return err;
	}

	/* Create PID-file */
	fd = open(PIDFILE, O_WRONLY | O_CREAT | O_TRUNC, 0444);
	if (fd == -1) {
		fprintf(stderr, "Failed to create pidfile %s: %s\n",
			PIDFILE, strerror(errno));
		cleanup_var_run();
		return err;
	}
	snprintf(buf, sizeof(buf), "%lu\n", (unsigned long)getpid());
	ssize = write(fd, buf, strlen(buf));
	close(fd);
	if (ssize != strlen(buf)) {
		fprintf(stderr, "Failed to write to pidfile %s: %s\n",
			PIDFILE, strerror(errno));
		cleanup_var_run();
		return err;
	}

	/* Create control pipe. */
	err = mkfifo(CTLPIPE, 0666);
	if (err && errno != EEXIST) {
		fprintf(stderr, "Failed to create control pipe %s: %s\n",
			CTLPIPE, strerror(errno));
		cleanup_var_run();
		return err;
	}
	ctlpipe = open(CTLPIPE, O_RDWR);
	if (ctlpipe == -1) {
		fprintf(stderr, "Failed to open control pipe %s: %s\n",
			CTLPIPE, strerror(errno));
		cleanup_var_run();
		return err;
	}

	return 0;
}

static int setup_environment(void)
{
	int err;

	err = razer_init();
	if (err) {
		fprintf(stderr, "librazer initialization failed. (%d)\n", err);
		return err;
	}
	err = setup_var_run();
	if (err) {
		razer_exit();
		return err;
	}

	return 0;
}

static void cleanup_environment(void)
{
	cleanup_var_run();
	razer_exit();
}

static void signal_handler(int signum)
{
	switch (signum) {
	case SIGINT:
	case SIGTERM:
		printf("Terminating razerd.\n");
		cleanup_environment();
		exit(0);
		break;
	default:
		fprintf(stderr, "Received unknown signal %d\n", signum);
	}
}

static void setup_sighandler(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = signal_handler;

	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
}

static void mainloop(void)
{
	struct razer_mouse *mice, *mouse;

	while (1) {
		mice = razer_rescan_mice();
		for (mouse = mice; mouse; mouse = mouse->next) {
			printf("Have mouse: %s\n", mouse->idstr);
		}
		printf("\n");
		sleep(1);
	}
}

int main(int argc, char **argv)
{
	int err;

	setup_sighandler();
	err = setup_environment();
	if (err)
		return 1;
	mainloop();
}
