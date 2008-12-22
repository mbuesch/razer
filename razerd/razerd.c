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
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>


#define VAR_RUN			"/var/run"
#define VAR_RUN_RAZERD		VAR_RUN "/razerd"
#define PIDFILE			VAR_RUN_RAZERD "/razerd.pid"
#define SOCKPATH		VAR_RUN_RAZERD "/socket"

#define RESCAN_INTERVAL_MSEC	1000 /* Rescan interval, in milliseconds. */

#define INTERFACE_REVISION	0

#define COMMAND_MAX_SIZE	512
#define COMMAND_HDR_SIZE	sizeof(struct command_hdr)

enum {
	COMMAND_ID_GETREV = 0,		/* Get the revision number of the socket interface. */
	COMMAND_ID_GETMICE,		/* Get a list of detected mice. */
	COMMAND_ID_GETFWVER,		/* Get the firmware rev of a mouse. */
	COMMAND_ID_SUPPFREQS,		/* Get a list of supported frequencies. */
	COMMAND_ID_SUPPRESOL,		/* Get a list of supported resolutions. */
};

struct command_hdr {
	uint8_t id;
} __attribute__((packed));

struct command {
	struct command_hdr hdr;
	char idstr[RAZER_IDSTR_MAX_SIZE];
	union {
		struct {
		} __attribute__((packed)) getfwver;
		struct {
		} __attribute__((packed)) suppfreqs;
		struct {
		} __attribute__((packed)) suppresol;
	} __attribute__((packed));
} __attribute__((packed));

#define offsetof(type, member)	((size_t)&((type *)0)->member)
#define CMD_SIZE(name)	(offsetof(struct command, name) + \
			 sizeof(((struct command *)0)->name))

struct client {
	struct client *next;
	struct sockaddr_un sockaddr;
	socklen_t socklen;
	int fd;
};

typedef _Bool bool;

/* Control socket FD. */
static int ctlsock = -1;
/* FD set we wait on in the main loop. */
static fd_set wait_fdset;
/* Linked list of connected clients. */
static struct client *clients;
/* Linked list of detected mice. */
static struct razer_mouse *mice;


static void cleanup_var_run(void)
{
	unlink(SOCKPATH);
	close(ctlsock);
	unlink(PIDFILE);
	rmdir(VAR_RUN_RAZERD);
}

static int setup_var_run(void)
{
	struct sockaddr_un sockaddr;
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
		return -1;
	}
	snprintf(buf, sizeof(buf), "%lu\n", (unsigned long)getpid());
	ssize = write(fd, buf, strlen(buf));
	close(fd);
	if (ssize != strlen(buf)) {
		fprintf(stderr, "Failed to write to pidfile %s: %s\n",
			PIDFILE, strerror(errno));
		cleanup_var_run();
		return -1;
	}

	/* Create the control socket. */
	ctlsock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ctlsock == -1) {
		fprintf(stderr, "Failed to create socket %s: %s\n",
			SOCKPATH, strerror(errno));
		cleanup_var_run();
		return -1;
	}
	err = fcntl(ctlsock, F_SETFL, O_NONBLOCK);
	if (err) {
		fprintf(stderr, "Failed to set O_NONBLOCK on socket %s: %s\n",
			SOCKPATH, strerror(errno));
		cleanup_var_run();
		return err;
	}
	sockaddr.sun_family = AF_UNIX;
	strncpy(sockaddr.sun_path, SOCKPATH, sizeof(sockaddr.sun_path) - 1);
	err = bind(ctlsock, (struct sockaddr *)&sockaddr, SUN_LEN(&sockaddr));
	if (err) {
		fprintf(stderr, "Failed to bind socket to %s: %s\n",
			SOCKPATH, strerror(errno));
		cleanup_var_run();
		return err;
	}
	err = chmod(SOCKPATH, 0666);
	if (err) {
		fprintf(stderr, "Failed to set %s socket permissions: %s\n",
			SOCKPATH, strerror(errno));
		cleanup_var_run();
		return err;
	}
	err = listen(ctlsock, 10);
	if (err) {
		fprintf(stderr, "Failed to listen on socket %s: %s\n",
			SOCKPATH, strerror(errno));
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

static void timeval_add_msec(struct timeval *tv, unsigned int msec)
{
	unsigned int seconds, usec;

	seconds = msec / 1000;
	msec = msec % 1000;
	usec = msec * 1000;

	tv->tv_usec += usec;
	while (tv->tv_usec >= 1000000) {
		tv->tv_sec++;
		tv->tv_usec -= 1000000;
	}
	tv->tv_sec += seconds;
}

/* Returns true, if a is after b. */
static bool timeval_after(const struct timeval *a, const struct timeval *b)
{
	if (a->tv_sec > b->tv_sec)
		return 1;
	if ((a->tv_sec == b->tv_sec) && (a->tv_usec > b->tv_usec))
		return 1;
	return 0;
}

static void free_client(struct client *client)
{
	free(client);
}

static struct client * new_client(const struct sockaddr_un *sockaddr,
				  socklen_t socklen, int fd)
{
	struct client *client;

	client = malloc(sizeof(*client));
	if (!client)
		return NULL;
	memset(client, 0, sizeof(*client));

	memcpy(&client->sockaddr, sockaddr, sizeof(client->sockaddr));
	client->socklen = socklen;
	client->fd = fd;

	return client;
}

static void client_list_add(struct client **base, struct client *new_entry)
{
	struct client *i;

	new_entry->next = NULL;
	if (!(*base)) {
		*base = new_entry;
		return;
	}
	for (i = *base; i->next; i = i->next)
		;
	i->next = new_entry;
}

static void client_list_del(struct client **base, struct client *del_entry)
{
	struct client *i;

	if (del_entry == *base) {
		*base = (*base)->next;
		return;
	}
	for (i = *base; i && (i->next != del_entry); i = i->next)
		;
	if (i)
		i->next = del_entry->next;
}

static void check_control_socket(void)
{
	socklen_t socklen;
	struct client *client;
	struct sockaddr_un remoteaddr;
	int fd;

	socklen = sizeof(remoteaddr);
	fd = accept(ctlsock, (struct sockaddr *)&remoteaddr, &socklen);
	if (fd == -1)
		return;
	/* Connected */
	client = new_client(&remoteaddr, socklen, fd);
	if (!client) {
		close(fd);
		return;
	}
	client_list_add(&clients, client);
printf("connected\n");
}

static void disconnect_client(struct client *client)
{
	client_list_del(&clients, client);
	free_client(client);
printf("Disconnected\n");
}

static inline uint32_t cpu_to_be32(uint32_t v)
{
#ifdef BIG_ENDIAN_HOST
	return v;
#else
	return swap32(v);
#endif
}

static int send_u32(struct client *client, uint32_t v)
{
	v = cpu_to_be32(v);
	return send(client->fd, &v, sizeof(v), 0);
}

static int send_string(struct client *client, const char *str)
{
	return send(client->fd, str, strlen(str) + 1, 0);
}

static void command_getmice(struct client *client, const struct command *cmd, unsigned int len)
{
	unsigned int count;
	char str[RAZER_IDSTR_MAX_SIZE + 1];
	struct razer_mouse *mouse;

	count = 0;
	razer_for_each_mouse(mouse, mice)
		count++;
	send_u32(client, count);
	razer_for_each_mouse(mouse, mice) {
		snprintf(str, sizeof(str), "%s", mouse->idstr);
		send_string(client, str);
	}
}

static void command_getfwver(struct client *client, const struct command *cmd, unsigned int len)
{
	struct razer_mouse *mouse;
	uint32_t fwver = 0xFFFFFFFF;
	int err;

	if (len < CMD_SIZE(getfwver))
		goto out;
	mouse = razer_mouse_list_find(mice, cmd->idstr);
	if (!mouse)
		goto out;
	err = mouse->claim(mouse);
	if (err)
		goto out;
	fwver = mouse->get_fw_version(mouse);
	mouse->release(mouse);
out:
	send_u32(client, fwver);
}

static void command_suppfreqs(struct client *client, const struct command *cmd, unsigned int len)
{
	struct razer_mouse *mouse;
	enum razer_mouse_freq *freq_list;
	int i, count;

	if (len < CMD_SIZE(suppfreqs))
		goto error;
	mouse = razer_mouse_list_find(mice, cmd->idstr);
	if (!mouse)
		goto error;
	count = mouse->supported_freqs(mouse, &freq_list);
	if (count <= 0)
		goto error;

	send_u32(client, count);
	for (i = 0; i < count; i++)
		send_u32(client, freq_list[i]);
	razer_free_freq_list(freq_list, count);

	return;
error:
	count = 0;
	send_u32(client, count);
}

static void command_suppresol(struct client *client, const struct command *cmd, unsigned int len)
{
	struct razer_mouse *mouse;
	enum razer_mouse_res *res_list;
	int i, count;

	if (len < CMD_SIZE(suppresol))
		goto error;
	mouse = razer_mouse_list_find(mice, cmd->idstr);
	if (!mouse)
		goto error;
	count = mouse->supported_resolutions(mouse, &res_list);
	if (count <= 0)
		goto error;

	send_u32(client, count);
	for (i = 0; i < count; i++)
		send_u32(client, res_list[i]);
	razer_free_resolution_list(res_list, count);

	return;
error:
	count = 0;
	send_u32(client, count);
}

static void handle_received_command(struct client *client, const char *_cmd, unsigned int len)
{
	const struct command *cmd = (const struct command *)_cmd;

	if (len < COMMAND_HDR_SIZE)
		return;
	switch (cmd->hdr.id) {
	case COMMAND_ID_GETREV:
		send_u32(client, INTERFACE_REVISION);
		break;
	case COMMAND_ID_GETMICE:
		command_getmice(client, cmd, len);
		break;
	case COMMAND_ID_GETFWVER:
		command_getfwver(client, cmd, len);
		break;
	case COMMAND_ID_SUPPFREQS:
		command_suppfreqs(client, cmd, len);
		break;
	case COMMAND_ID_SUPPRESOL:
		command_suppresol(client, cmd, len);
		break;
	default:
		/* Unknown command. */
		break;
	}
}

static void check_client_connections(void)
{
	char command[COMMAND_MAX_SIZE + 1] = { 0, };
	int nr;
	struct client *client, *next;

	for (client = clients; client; ) {
		next = client->next;
		nr = recv(client->fd, command, COMMAND_MAX_SIZE, 0);
		if (nr < 0)
			goto next_client;
		if (nr == 0) {
			disconnect_client(client);
			goto next_client;
		}
		handle_received_command(client, command, nr);
  next_client:
		client = next;
	}
}

static void mainloop(void)
{
	struct timeval now, next_rescan, select_timeout;
	struct client *client;

	mice = razer_rescan_mice();
	gettimeofday(&now, NULL);
	memcpy(&next_rescan, &now, sizeof(now));
	timeval_add_msec(&next_rescan, RESCAN_INTERVAL_MSEC);

	while (1) {
		FD_ZERO(&wait_fdset);
		FD_SET(ctlsock, &wait_fdset);
		for (client = clients; client; client = client->next)
			FD_SET(client->fd, &wait_fdset);
		select_timeout.tv_sec = (RESCAN_INTERVAL_MSEC + 100) / 1000;
		select_timeout.tv_usec = ((RESCAN_INTERVAL_MSEC + 100) % 1000) * 1000;
		select(FD_SETSIZE, &wait_fdset, NULL, NULL, &select_timeout);

		gettimeofday(&now, NULL);
		if (timeval_after(&now, &next_rescan)) {
			mice = razer_rescan_mice();
			memcpy(&next_rescan, &now, sizeof(now));
			timeval_add_msec(&next_rescan, RESCAN_INTERVAL_MSEC);
		}

		check_control_socket();
		check_client_connections();
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
