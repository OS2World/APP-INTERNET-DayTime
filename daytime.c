/* daytime.c - set local time from remote daytime or time server
 *
 * Author:  Kai Uwe Rommel <rommel@ars.muc.de>
 * Created: Sun Apr 10 1994
 *
 * This code is in the public domain.
 * Let the author know if you make improvements or fixes, though.
 */

static char *rcsid =
"$Id: daytime.c,v 1.17 1999/06/13 12:00:58 rommel Exp rommel $";
/* static char *rcsrev = "$Revision: 1.17 $"; */
static char *rcsrev = "revision: 1.20";

/*
 * $Log: daytime.c,v $
 * Revision 1.17  1999/06/13 12:00:58  rommel
 * fixed SNTP bitfield alignment error
 *
 * Revision 1.16  1999/03/05 15:47:06  rommel
 * added logging
 *
 * Revision 1.15  1999/02/27 14:15:41  rommel
 * changed NT service code
 *
 * Revision 1.14  1998/10/19 05:58:55  rommel
 * minor changes/fixes
 *
 * Revision 1.13  1998/10/18 19:36:26  rommel
 * make code time_t independent
 *
 * Revision 1.12  1998/10/18 17:42:34  rommel
 * fix incompatibility with time_t being defined as double
 *
 * Revision 1.11  1998/10/01 10:06:10  rommel
 * fixes from colinw@ami.com.au
 *
 * Revision 1.10  1998/07/31 14:11:58  rommel
 * fixed small bug
 *
 * Revision 1.9  1998/07/30 06:46:09  rommel
 * added Win32 port
 * added SNTP support
 * fixed many bugs
 * prepared for modem support
 *
 * Revision 1.8  1996/12/08 11:18:04  rommel
 * added get_date prototype
 *
 * Revision 1.7  1996/12/08 11:15:07  rommel
 * don't change time within 1 second margin
 * from Ewen McNeill <ewen@naos.co.nz>
 *
 * Revision 1.6  1996/11/29 15:45:00  rommel
 * changes (maxadj) from Ewen McNeill <ewen@naos.co.nz>
 *
 * Revision 1.5  1995/08/20 08:15:10  rommel
 * updated for new emx socket library, IBM compiler
 * fixed minor bugs
 *
 * Revision 1.4  1994/07/17 21:13:26  rommel
 * fixed usage() display again
 *
 * Revision 1.3  1994/07/17 21:10:23  rommel
 * fixed usage() display
 *
 * Revision 1.2  1994/07/17 21:07:57  rommel
 * added daemon mode, continuously updating local time
 *
 * Revision 1.1  1994/07/17 20:45:55  rommel
 * Initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "daytime.h"
#include "os.h"

struct options
{
  int interval;
  int dont;
  int maxadj;
  int maxwait;
  int retries;
  int rtryint;
  long offset;
  service serv;
  protocol proto;
  int portnum;
  char host[64];
  // char port[64]; // 2010-09-27 SHL
  char logfile[256];
};

static struct options opts;

int get_modem_line(int port, char *node, char *buffer, int bytes)
{
  /* get time over modem 0531512038 ? */
  return -1;
}

int get_and_set_time(int port, char *node, service serv, protocol proto,
		     int dont, long offs, long maxadj, int maxwait)
{
  int sock, bytes, size, rc;
  struct sockaddr_in server;
  struct hostent *host;
  struct linger linger;
  struct timeval tv;
  fd_set fds;
  time_t remote, here;
  long newtime, ourtime;
  char buffer[128];
  char *request;
  char *timestr;
  char *p;
  sntp *message;

  switch (serv)
  {

  case DAYTIME:
  case TIME:

    bytes = 1;
    buffer[0] = 0;

    break;

  case SNTP:

    bytes = sizeof(sntp);
    memset(buffer, 0, bytes);

    message = (sntp *) buffer;

    message->flags.version = 1;
    message->flags.mode = 3;

    break;

  }

  if (proto != MODEM)
  {
    server.sin_family = AF_INET;
    server.sin_port = port;

    if (isdigit(node[0]))
      server.sin_addr.s_addr = inet_addr(node);
    else
    {
      if ((host = gethostbyname(node)) == NULL)
	return print_h_errno("gethostbyname()"), 2;

      server.sin_addr = * (struct in_addr *) (host -> h_addr);
    }
  }

  switch (proto)
  {

  case TCP:

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
      return print_sock_errno("socket(tcp)"), 2;

    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0)
      return print_sock_errno("connect()"), 2;

    if (serv == SNTP)
    {
      if (send(sock, buffer, bytes, 0) < 0)
	return print_sock_errno("send()"), 2;
    }

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    tv.tv_sec  = maxwait;
    tv.tv_usec = 0;

    if ((rc = select(FD_SETSIZE, &fds, 0, 0, &tv)) < 0)
      return print_sock_errno("select()"), 2;
    else if (rc == 0)
    {
      soclose(sock);
      lprintf("timeout waiting for answer from time server");
      return 0;
    }

    if ((bytes = recv(sock, buffer, sizeof(buffer), 0)) <= 0)
      return print_sock_errno("recv()"), 2;

    soclose(sock);

    break;

  case UDP:

    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 1)
      return print_sock_errno("socket(udp)"), 2;

    if (sendto(sock, buffer, bytes, 0,
               (struct sockaddr *) &server, sizeof(server)) < 0)
      return print_sock_errno("sendto()"), 2;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    tv.tv_sec  = maxwait;
    tv.tv_usec = 0;

    if ((rc = select(FD_SETSIZE, &fds, 0, 0, &tv)) < 0)
      return print_sock_errno("select()"), 2;
    else if (rc == 0)
    {
      soclose(sock);
      lprintf("timeout waiting for answer from time server");
      return 0;
    }

    size = sizeof(server);
    if ((bytes = recvfrom(sock, buffer, sizeof(buffer), 0,
                          (struct sockaddr *) &server, &size)) < 0)
      print_sock_errno("recvfrom()");

    soclose(sock);

    break;

  case MODEM:

    if ((bytes = get_modem_line(port, node, buffer, sizeof(buffer))) == -1)
      return perror("modem"), 2;

    break;

  }

  switch (serv)
  {

  case DAYTIME:

    buffer[bytes] = 0;
    remote = get_date(buffer, NULL);
    newtime = (long) remote;
    if (newtime <= 0)
      lprintf("unexpected response \"%s\"", buffer);

    break;

  case TIME:

    if (bytes != sizeof(long))
      return lprintf("invalid time value received"), 1;

    newtime = * (long *) buffer;
    newtime = ntohl(newtime);
    /*
     * The time service returns seconds since 1900/01/01 00:00:00 GMT, not
     * since 1970 as assumed by Unix time_t values.  We need to subtract
     * 70 years' worth of seconds.  Fortunately, RFC 868 tells us that the
     * time value 2,208,988,800 corresponds to 00:00 1 Jan 1970 GMT.
     */
    newtime -= 2208988800UL;

    break;

  case SNTP:

    if (message->flags.version < 1 || 3 < message->flags.version)
      return lprintf("incompatible protocol version on server"), 1;
    if (message->flags.leap_ind == 3)
      return lprintf("server not synchronized"), 1;
    if (message->stratum == 0)
      return lprintf("server not synchronized"), 1;

    newtime = ntohl(message->transmit_time.integer);

    if (newtime == 0)
      return lprintf("server not synchronized"), 1;

    /* same here */
    newtime -= 2208988800UL;

    break;

  }

  if (newtime <= 0)
    return lprintf("invalid time (%ld)", newtime), 1;

  newtime += offs;
  remote = (time_t) newtime;

  time(&here);
  ourtime = (long) here;

  if (maxadj)
    if (newtime < (ourtime - maxadj) || (ourtime + maxadj) < newtime)
      dont = 1;               /* too far off: just report it */

  if (abs(newtime - ourtime) <= 1)
    dont = 1;

  if (!dont && stime(&remote))
    return lprintf("invalid time set request"), 1;

  timestr = ctime(&remote);
  timestr[strlen(timestr) - 1] = 0;
  lprintf("time %s %s: (%+ld) %s", dont ? "at" : "set from",
	 node, newtime - ourtime, timestr);
  fflush(stdout);

  return 0;
}

int usage(void)
{
  printf("\nSet time from remote host, %s"
	 "\n(C) 1997 Kai-Uwe Rommel\n", rcsrev);

  printf("\nUsage: daytime [options] host\n"
         "\nOptions:"
#ifdef WIN32
         "\n  -I       install as a service"
         "\n  -U       uninstall service\n"
#endif
         "\n  -l file  write messages to logfile"
         "\n  -c int   run continuously, perform action every 'int' seconds"
         "\n  -n       do not set the local clock, display the remote time only"
         "\n  -o offs  adjust the retrieved time by offs seconds before"
         "\n           setting the local clock"
         "\n  -m secs  maximum number of seconds to adjust by"
         "\n  -w secs  maximum number of seconds to wait for the time server's answer"
         "\n  -r rtry  number of retries if a network connection error occurs (default 0)"
         "\n  -i secs  number of seconds to wait between retries (default 15)"
         "\n  -d       use the DAYTIME service (port 13)"
         "\n  -t       use the TIME service (port 37, this is the default)"
         "\n  -s       use the SNTP service (port 123), usually requires -u too"
         "\n  -u       use the UDP protocol (default is to use TCP)"
         "\n  -p port  use nonstandard port number\n"
#if 0
         "\n  -x       use serial modem to access time server"
         "\n           (in this case, the 'host' is the phone number to dial"
         "\n           and 'port' is the number of the serial port)\n");
#else
         "\n");
#endif         	
  return 1;
}

int endless(void)
{
  int rc = 0;
  int rtry = 0;

  for(;;)
  {
    rc = get_and_set_time(opts.portnum, opts.host, opts.serv, opts.proto,
			  opts.dont, opts.offset, opts.maxadj, opts.maxwait);
    if (rc == 2 && opts.retries && rtry < opts.retries) {
        sleep(opts.rtryint);
        rtry++;
    }
    else
        sleep(opts.interval);
  }
  return rc;
}

int main(int argc, char **argv)
{
  int opt, rc, isrv = 0;
  struct servent *port;

  tzset();

  if (sock_init())
    return print_sock_errno("sock_init()"), 1;

#ifdef WIN32
#define SERVICE_NAME "daytime"
#define SERVICE_TITLE "Daytime Client"
  if (started_as_service() && argc == 1)
  {
    restore_options(SERVICE_NAME, &opts, sizeof(opts));
    logfile = opts.logfile;
    run_as_service(SERVICE_NAME, endless);
    return 0;
  }
#endif

  opts.serv = TIME;
  opts.proto = TCP;
  opts.portnum = -1;
  opts.maxwait = 60;
  opts.retries = 0;
  opts.rtryint = 10;

  while ((opt = getopt(argc, argv, "?l:IUndtsuo:p:c:m:w:xr:i:")) != EOF)
    switch (opt)
    {
    case 'l':
      strcpy(opts.logfile, optarg);
      logfile = opts.logfile;
      break;
    case 'n':
      opts.dont = 1;
      break;
    case 'd':
      opts.serv = DAYTIME;
      break;
    case 't':
      opts.serv = TIME;
      break;
    case 's':
      opts.serv = SNTP;
      break;
    case 'u':
      opts.proto = UDP;
      break;
    case 'o':
      opts.offset = atol(optarg);
      break;
    case 'p':
      opts.portnum = htons(atoi(optarg));
      break;
    case 'c':
      opts.interval = atol(optarg);
      break;
    case 'm':
      opts.maxadj = atol(optarg);
      break;
    case 'w':
      opts.maxwait = atol(optarg);
      break;
    case 'r':
      opts.retries = atol(optarg);
      break;
    case 'i':
      opts.rtryint = atol(optarg);
      break;
    case 'x':
      opts.proto = MODEM;
      opts.serv = DAYTIME;
      break;
#ifdef WIN32
    case 'I':
      isrv = 1;
      break;
    case 'U':
      if ((rc = install_as_service(SERVICE_NAME, 0, 0)) != 0)
	printf("deinstallation of service failed\n");
      return rc;
#endif
    default:
      return usage();
    }

  if (optind == argc)
    return usage();
  else
    strcpy(opts.host, argv[optind]);

  if (opts.portnum == -1)
  {
    char *s_service = opts.serv == SNTP ? "ntp" :
                      opts.serv == DAYTIME ? "daytime" : "time";
    char *s_proto = opts.proto == TCP ? "tcp" : "udp";

#ifndef OS2
    if ((port = getservbyname(s_service, s_proto)) == NULL)
      return print_sock_errno("getservbyname()"), 1;
#else
    // 2010-11-05 SHL Some versions of TCP/IP stack appear to have broken getservbyname
    port = getservbyname(s_service, s_proto);
    if (port == NULL) {
	// 2010-11-05 SHL Fallback and do our own search
	endservent();
	while ((port = getservent()) != NULL) {
	  if (strcmp(port->s_name, s_service) == 0 &&
	      strcmp(port->s_proto, s_proto) == 0) {
	      break;
	  }
	}
	endservent();
    }
    if (port == NULL)
      return print_sock_errno("getservbyname()"), 1;
#endif
    opts.portnum = port -> s_port;
  }

#ifdef WIN32
  if (isrv)
  {
    if ((rc = install_as_service(SERVICE_NAME, SERVICE_TITLE, 1)) != 0)
      return printf("installation as service failed\n"), rc;
    else
      return save_options(SERVICE_NAME, &opts, sizeof(opts));
  }
#endif

  if (opts.interval == 0) {
    int tries = 0;
    for (rc = 2; rc == 2 && tries <= opts.retries; tries++) {
        rc = get_and_set_time(opts.portnum, opts.host, opts.serv, opts.proto,
                              opts.dont, opts.offset, opts.maxadj, opts.maxwait);
        if (rc == 2 && opts.retries)
            sleep(opts.rtryint);
    }
    return rc;
  }
  else
    return endless();
}

/* end of daytime.c */
