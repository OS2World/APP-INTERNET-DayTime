/* os.h
 *
 * Author:   <rommel@ars.de>
 * Created: Sat Apr 26 1997
 */

/* $Id: os.h,v 1.7 2002/05/21 06:50:50 Rommel Exp Rommel $ */

/*
 * $Log: os.h,v $
 * Revision 1.7  2002/05/21 06:50:50  Rommel
 * fixes psock_errno problem for OS/2
 *
 * Revision 1.6  1999/03/05 15:47:06  rommel
 * added logging
 *
 * Revision 1.5  1999/02/27 15:22:55  rommel
 * changes for new OS/2 toolkit
 *
 * Revision 1.4  1999/02/27 14:17:51  rommel
 * changed NT service code
 *
 * Revision 1.3  1998/10/18 17:41:50  rommel
 * fix IBM OS/2 header dependency
 *
 * Revision 1.2  1997/05/04 13:39:04  rommel
 * added NT service code
 *
 * Revision 1.1  1997/04/26 14:21:54  Rommel
 * Initial revision
 *
 */

#ifndef _OS_H
#define _OS_H

time_t get_date(char *p, void *now);
int stime(time_t *newtime);
void print_sock_errno(char *text);

#ifdef OS2

#include <sys/time.h>

#ifdef __IBMC__

#define BSD_SELECT
#include <types.h>
#include <sys/select.h>
#include <unistd.h>

#define INCL_DOS
#include <os2.h>

#ifndef sleep
#define sleep(s) DosSleep(1000 * s)
#endif

#endif

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

void print_h_errno(char *text);

#ifndef NETDB_INTERNAL
#define NETDB_INTERNAL -1
#endif
#ifndef NETDB_SUCCESS
#define NETDB_SUCCESS 0
#endif

#if defined(__EMX__) || defined(__UEL__)

#define soclose close
#define sock_init() 0

#endif

#endif

#ifdef WIN32

#include <windows.h>
#include <winsock.h>

#define soclose closesocket
#define print_h_errno print_sock_errno
int sock_init(void);

#define sleep(s) Sleep(1000 * s)

void run_as_service(char *name, int (*function)(void));
int started_as_service(void);
int install_as_service(char *name, char *title, int install);

int save_options(char *name, void *data, int size);
int restore_options(char *name, void *data, int size);

#endif

#if defined(__UEL__)
#include <unistd.h>
#define h_addr  h_addr_list[0]  /* address, for backward compatiblity */
#else
#include "getopt.h"
#endif

extern char *logfile;
int lprintf(char *format, ...);

#endif /* _OS_H */

/* end of os.h */
