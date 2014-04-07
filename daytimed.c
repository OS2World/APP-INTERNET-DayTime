/* daytimed.c - daytime and time server daemon
 *
 * Author:  Kai Uwe Rommel <rommel@ars.muc.de>
 * Created: Sun Apr 10 1994
 *
 * This code is in the public domain.
 * Let the author know if you make improvements or fixes, though.
 */

static char *rcsid =
"$Id: daytimed.c,v 1.10 2002/05/16 17:41:23 Rommel Exp Rommel $";
/* static char *rcsrev = "$Revision: 1.10 $"; */
static char *rcsrev = "revision: 1.20";

/*
 * $Log: daytimed.c,v $
 * Revision 1.10  2002/05/16 17:41:23  Rommel
 * several minor fixes
 *
 * Revision 1.9  1999/03/05 15:47:06  rommel
 * added logging
 *
 * Revision 1.8  1999/02/27 14:17:07  rommel
 * changed NT service code
 * skip unavailable service port information
 *
 * Revision 1.7  1998/10/19 05:59:07  rommel
 * minor changes/fixes
 *
 * Revision 1.6  1998/10/18 19:36:26  rommel
 * make code time_t independent
 *
 * Revision 1.5  1998/10/01 10:06:10  rommel
 * fixes from colinw@ami.com.au
 *
 * Revision 1.4  1998/07/30 06:46:09  rommel
 * added Win32 port
 * added SNTP support
 * fixed many bugs
 * prepared for modem support
 *
 * Revision 1.3  1995/08/20 08:15:10  rommel
 * updated for new emx socket library, IBM compiler
 * fixed minor bugs
 *
 * Revision 1.2  1994/07/17 21:06:45  rommel
 * bug fix: display correct peer address
 *
 * Revision 1.1  1994/07/17 20:46:11  rommel
 * Initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "daytime.h"
#include "os.h"

struct options
{
  char logfile[256];
};

static struct options opts;

int handle_request(int sock, service serv, protocol proto)
{
  struct sockaddr_in client;
  struct linger linger;
  char buffer[64], *timestr, *service;
  time_t now;
  long ourtime;
  sntp *message;
  int bytes, size;

  if (proto == TCP)
  {
    if (serv == SNTP)
    {
      if ((bytes = recv(sock, buffer, sizeof(buffer), 0)) <= 0)
	return print_sock_errno("recv()"), 1;
    }
  }
  else
  {
    size = sizeof(client);
    if (recvfrom(sock, buffer, sizeof(buffer), 0,
		 (struct sockaddr *) &client, &size) < 0)
      return print_sock_errno("recvfrom()"), 1;
  }

  time(&now);
  ourtime = (long) now;

  /* Do NOT serve requests from invalid configured dynamic IP clients
     (client address = 0.0.0.0) (fix from Bent C. Pedersen) */
  if (*((unsigned long *) &client.sin_addr) == 0)
     return lprintf("Request from 0.0.0.0 rejected."), 0;

  switch (serv)
  {

  case DAYTIME:

    timestr = ctime(&now);
    bytes = strlen(timestr) + 1;
    service = "daytime";

    break;

  case TIME:

    /* The time service returns seconds since 1900/01/01 00:00:00 GMT, not
     * since 1970 as assumed by Unix time values.  We need to add 70
     * years' worth of seconds.  Fortunately, RFC 868 tells us that the
     * daytime value 2,208,988,800 corresponds to 00:00  1 Jan 1970 GMT. */
    ourtime += 2208988800UL;
    ourtime = htonl(ourtime);

    timestr = (char *) &ourtime;
    bytes = sizeof(ourtime);
    service = "time";

    break;

  case SNTP:

    /* same here */
    ourtime += 2208988800UL;
    ourtime = htonl(ourtime);

    timestr = buffer;
    bytes = sizeof(sntp);
    service = "SNTP";

    message = (sntp *) buffer;

    message->flags.leap_ind = 0;

    if (message->flags.version < 1 || 3 < message->flags.version)
      message->flags.version  = 3;

    message->flags.mode = (message->flags.mode == 3) ? 4 : 2;
    message->stratum = 1;
    message->precision = 0;
    message->root_delay = 0;
    message->root_dispersion = 0;
    message->reference_id = 0;

    message->reference_time.integer = ourtime;
    message->reference_time.fraction = 0;

    message->originate_time = message->transmit_time;
    message->receive_time = message->reference_time;
    message->transmit_time = message->reference_time;

    break;

  }

  if (proto == TCP)
  {
    linger.l_onoff = 1;
    linger.l_linger = 10;

    if (setsockopt(sock, SOL_SOCKET, SO_LINGER,
		   (char *) &linger, sizeof(linger)) < 0)
      return print_sock_errno("setsockopt(SO_LINGER)"), 1;

    if (send(sock, timestr, bytes, 0) < 0)
      return print_sock_errno("send()"), 1;

    size = sizeof(client);
    if (getpeername(sock, (struct sockaddr *) &client, &size) < 0)
      return print_sock_errno("getsockname()"), 1;

    soclose(sock);
  }
  else
  {
    if (sendto(sock, timestr, bytes, 0,
	       (struct sockaddr *) &client, size) < 0)
      return print_sock_errno("sendto()"), 1;
  }

  lprintf("served '%s' request from %s via '%s'",
	  service, inet_ntoa(client.sin_addr), proto == TCP ? "tcp" : "udp");

  return 0;
}

int serve(void)
{
  struct sockaddr_in server, client;
  struct servent *port;
  int tcp_13 = 0, tcp_37 = 0, udp_13 = 0, udp_37 = 0, tcp_123 = 0, udp_123 = 0;

  /* We set up all our services. If any of them fails to set up properly,
     it will be ignored and not be serviced. NT, for example, already
     has a daytime service for both TCP and UDP. */

  /* set up daytime (tcp) server */

  if ((port = getservbyname("daytime", "tcp")) != NULL)
  {
    if ((tcp_13 = socket(PF_INET, SOCK_STREAM, 0)) < 0)
      return print_sock_errno("socket(daytime tcp)"), 1;

    server.sin_family = AF_INET;
    server.sin_port = port -> s_port;
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp_13, (struct sockaddr *) &server, sizeof(server)) < 0)
    {
      soclose(tcp_13);
      tcp_13 = 0;
    }
    else
    {
      if (listen(tcp_13, 4) != 0)
	return print_sock_errno("listen(daytime tcp)"), 1;

      lprintf("listening for daytime requests via tcp");
    }
  }

  /* set up daytime (udp) server */

  if ((port = getservbyname("daytime", "udp")) != NULL)
  {
    if ((udp_13 = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
      return print_sock_errno("socket(daytime udp)"), 1;

    server.sin_family = AF_INET;
    server.sin_port = port -> s_port;
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_13, (struct sockaddr *) &server, sizeof(server)) < 0)
    {
      soclose(udp_13);
      udp_13 = 0;
    }
    else
      lprintf("listening for daytime requests via udp");
  }

  /* set up time (tcp) server */

  if ((port = getservbyname("time", "tcp")) != NULL)
  {
    if ((tcp_37 = socket(PF_INET, SOCK_STREAM, 0)) < 0)
      return print_sock_errno("socket(time tcp)"), 1;

    server.sin_family = AF_INET;
    server.sin_port = port -> s_port;
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp_37, (struct sockaddr *) &server, sizeof(server)) < 0)
    {
      soclose(tcp_37);
      tcp_37 = 0;
    }
    else
    {
      if (listen(tcp_37, 4) != 0)
	return print_sock_errno("listen(time tcp)"), 1;

      lprintf("listening for time requests via tcp");
    }
  }

  /* set up time (udp) server */

  if ((port = getservbyname("time", "udp")) != NULL)
  {
    if ((udp_37 = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
      return print_sock_errno("socket(time udp)"), 1;

    server.sin_family = AF_INET;
    server.sin_port = port -> s_port;
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_37, (struct sockaddr *) &server, sizeof(server)) < 0)
    {
      soclose(udp_37);
      udp_37 = 0;
    }
    else
      lprintf("listening for time requests via udp");
  }

  /* set up sntp (tcp) server, nonstandard */

  if ((port = getservbyname("ntp", "tcp")) != NULL)
  {
    if ((tcp_123 = socket(PF_INET, SOCK_STREAM, 0)) < 0)
      return print_sock_errno("socket(ntp tcp)"), 1;

    server.sin_family = AF_INET;
    server.sin_port = port -> s_port;
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp_123, (struct sockaddr *) &server, sizeof(server)) < 0)
    {
      soclose(tcp_123);
      tcp_123 = 0;
    }
    else
    {
      if (listen(tcp_123, 4) != 0)
	return print_sock_errno("listen(ntp tcp)"), 1;

      lprintf("listening for SNTP requests via tcp");
    }
  }

  /* set up sntp (udp) server */

  if ((port = getservbyname("ntp", "udp")) != NULL)
  {
    if ((udp_123 = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
      return print_sock_errno("socket(ntp udp)"), 1;

    server.sin_family = AF_INET;
    server.sin_port = port -> s_port;
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_123, (struct sockaddr *) &server, sizeof(server)) < 0)
    {
      soclose(udp_123);
      udp_123 = 0;
    }
    else
      lprintf("listening for SNTP requests via udp");
  }

  /* server loop */

  if (tcp_13 == 0 && tcp_37 == 0 && tcp_123 == 0 &&
      udp_13 == 0 && udp_37 == 0 && udp_123 == 0)
  {
    lprintf("nothing to serve");
    return 1;
  }

  for (;;)
  {
    struct timeval tv;
    fd_set fds;
    int rc, sock, sock_clt, length;
    service serv;
    protocol proto;

    /* wait for a request */

    FD_ZERO(&fds);

    if (tcp_13)
      FD_SET(tcp_13, &fds);
    if (tcp_37)
      FD_SET(tcp_37, &fds);
    if (tcp_123)
      FD_SET(tcp_123, &fds);
    if (udp_13)
      FD_SET(udp_13, &fds);
    if (udp_37)
      FD_SET(udp_37, &fds);
    if (udp_123)
      FD_SET(udp_123, &fds);

    tv.tv_sec  = 60;
    tv.tv_usec = 0;

    if ((rc = select(FD_SETSIZE, &fds, 0, 0, &tv)) < 0)
      return print_sock_errno("select()"), 1;
    else if (rc == 0)
      continue;

    /* determine type and protocol */

    if (tcp_13 && FD_ISSET(tcp_13, &fds) != 0)
    {
      sock = tcp_13;
      serv = DAYTIME;
      proto = TCP;
    }
    else if (tcp_37 && FD_ISSET(tcp_37, &fds) != 0)
    {
      sock = tcp_37;
      serv = TIME;
      proto = TCP;
    }
    else if (tcp_123 && FD_ISSET(tcp_123, &fds) != 0)
    {
      sock = tcp_123;
      serv = SNTP;
      proto = TCP;
    }
    else if (udp_13 && FD_ISSET(udp_13, &fds) != 0)
    {
      sock = udp_13;
      serv = DAYTIME;
      proto = UDP;
    }
    else if (udp_37 && FD_ISSET(udp_37, &fds) != 0)
    {
      sock = udp_37;
      serv = TIME;
      proto = UDP;
    }
    else if (udp_123 && FD_ISSET(udp_123, &fds) != 0)
    {
      sock = udp_123;
      serv = SNTP;
      proto = UDP;
    }
    else
      continue;

    /* and handle it */

    if (proto == TCP)
    {
      length = sizeof(client);
      if ((sock_clt = accept(sock, (struct sockaddr *) &client, &length)) == -1)
	return print_sock_errno("accept()"), 1;

      handle_request(sock_clt, serv, TCP);
    }
    else
      handle_request(sock, serv, UDP);
  }

  return 0;
}

int usage(void)
{
  printf("\nTime Server, %s"
	 "\n(C) 1997 Kai-Uwe Rommel\n", rcsrev);

#ifdef OS2
  printf("\nUsage: daytimed [-l file] [-S] [[-dtsu] socket]\n"
         "\n  -l  write messages to file"
         "\n  -S  run as a standalone server (not from inetd)\n"
         "\n  -d  serve DAYTIME request on socket"
         "\n  -t  serve TIME request on socket"
         "\n  -s  serve SNTP request on socket"
         "\n  -u  use UDP instead of TCP to serve request\n"
	 "\nThe -d, -t, -s and -u options are for use with inetd only.\n");
#endif

#ifdef WIN32
  printf("\nUsage: daytimed [-l file] [-SIU] \n"
         "\n  -l  write messages to file"
         "\n  -S  run as a standalone server (not as a service)"
         "\n  -I  install as a service"
         "\n  -U  uninstall service\n");
#endif

  return 1;
}

int main(int argc, char **argv)
{
  int opt, rc, isrv = 0;
  service serv = TIME;
  protocol proto = TCP;

  tzset();

  if (sock_init())
    return print_sock_errno("sock_init()"), 1;

#ifdef WIN32
#define SERVICE_NAME "daytimed"
#define SERVICE_TITLE "Daytime Server"
  if (started_as_service() && argc == 1)
  {
    restore_options(SERVICE_NAME, &opts, sizeof(opts));
    logfile = opts.logfile;
    run_as_service(SERVICE_NAME, serve);
  }
#endif

  while ((opt = getopt(argc, argv, "?l:dtsuSIU")) != EOF)
    switch (opt)
    {
    case 'l':
      strcpy(opts.logfile, optarg);
      logfile = opts.logfile;
      break;
#ifdef OS2
    case 'd':
      serv = DAYTIME;
      break;
    case 't':
      serv = TIME;
      break;
    case 's':
      serv = SNTP;
      break;
    case 'u':
      proto = UDP;
      break;
#endif
    case 'S':
      return serve();
#ifdef WIN32
    case 'I':
      isrv = 1;
      break;
    case 'U':
      if ((rc = install_as_service(SERVICE_NAME, NULL, 0)) != 0)
	printf("deinstallation of service failed\n");
      return rc;
#endif
    default:
      return usage();
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

  if (optind == argc)
    return usage();

  return handle_request(atoi(argv[optind]), serv, proto);
}

/* end of daytimed.c */
