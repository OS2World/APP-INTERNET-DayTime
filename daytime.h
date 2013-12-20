/* daytime.h
 *
 * Author:   <rommel@ars.de>
 * Created: Sun May 04 1997
 *
 * This code is in the public domain.
 * Let the author know if you make improvements or fixes, though.
 */

/* $Id: daytime.h,v 1.6 1999/06/13 12:44:29 rommel Exp rommel $ */

/*
 * $Log: daytime.h,v $
 * Revision 1.6  1999/06/13 12:44:29  rommel
 * *** empty log message ***
 *
 * Revision 1.5  1999/06/13 12:00:58  rommel
 * fixed SNTP bitfield alignment error
 *
 * Revision 1.4  1999/02/27 14:16:23  rommel
 * renamed type
 *
 * Revision 1.3  1998/10/01 10:04:37  rommel
 * fixes from colinw@ami.com.au
 *
 * Revision 1.2  1998/07/30 06:46:09  rommel
 * added Win32 port
 * added SNTP support
 * fixed many bugs
 * prepared for modem support
 *
 * Revision 1.1  1997/05/04 19:59:52  rommel
 * Initial revision
 *
 */

#ifndef _DAYTIME_H
#define _DAYTIME_H

typedef enum {TIME = 0, DAYTIME = 1, SNTP = 2} service;
typedef enum {UDP = 0, TCP = 1, MODEM = 3} protocol;

#if defined(OS2) && defined(__IBMC__)
typedef unsigned int bitfield;
#else
typedef unsigned char bitfield;
#endif

typedef struct
{
  bitfield mode     : 3;
  bitfield version  : 3;
  bitfield leap_ind : 2;
}
flagbits;

typedef struct
{
  long integer;
  long fraction;
}
tstamp;

typedef struct
{
  flagbits flags;
  char stratum;
  char poll;
  char precision;
  long root_delay;
  long root_dispersion;
  long reference_id;
  tstamp reference_time;
  tstamp originate_time;
  tstamp receive_time;
  tstamp transmit_time;
  /* long authenticator[3]; */
}
sntp;

#endif /* _DAYTIME_H */

/* end of daytime.h */
