- Overview

This file documents the time and daytime server and client programs
for OS/2 and Windows NT. There is a server program (the daytimed.exe
daemon) and a client program (daytime.exe).

The server implements the RFC 867 (daytime), RFC 868 (time) and RFC
1361 (SNTP) services, both for TCP and UDP protocols. The client can
use any of these services and either protocol to retrieve the time
from another host and adjust its local time. Using SNTP over TCP is an
extension over RFC 1361 (although mentioned there).

While the daytime service is more intended for human use, the time and
SNTP services are meant for machine use, like in these programs. But
the server and client support all of them, for completeness. Also, the
programs are named after the daytime service even though the time
service or SNTP are the preferred methods.

Full source code is included, the programs can compiled with the
emx+gcc package or IBM's VisualAge C++.

- TZ environment variable

This program requires the TZ environment variable to be set
correctly in order to have time zone and daylight savings time
be recognized as needed in your location.

Please see the appendix below on how to set TZ. The setting
for central europe, for example, would be:

SET TZ=CET-1CED,3,-1,0,7200,10,-1,0,10800,3600

- Usage:

To install, copy the programs into a directory listed in the PATH. You
are now ready to use the client program. Run them without arguments to
get a description of their command line options.

- Special notes for the OS/2 server program:

To use the server program, you must decide if you want to run it all
the time as a standalone daemon or as a subservice from the inetd
super-daemon. The inetd method saves memory but makes the response
time longer (since the server must be started as a process for each
request). In addition, when used with inetd, the server can only
respond to requests via the TCP protocol, not to UDP requests, due to
the way inetd is implemented under OS/2 and due to my own lazyness.
But then, usually the TCP protocol is to be preferred anyway.

To use the daytimed server with inetd, two lines must be added to
\tcpip\etc\inetd.lst:

	daytime tcp daytimed -d
	time tcp daytimed -t

That's all, then you have to restart inetd.

- Special notes for the Windows NT programs:

Both the client and the server programs can run as NT services. You
can use -I and -U to install or uninstall them as services and then
use NET START/STOP to start and stop them. The service names are
daytime and daytimed, respectively. They appear as "Daytime Client"
and "Daytime Server" in the NT control panel's services applet.

The client program will usually need command line arguments to run (to
specify the service, protocol and server host to be used). You specify
them at the time when you install it as a service in addition to -I.
It will save them in the registry and use at the time started as a
service. If you ever need to change the options used by the client
program as a service, you need to uninstall it with -U and then
install it as a service with -I and the new options again.

Running the client as a service makes, by the way, only sense when the
-c option is used. Otherwise, it will terminate immediately after
setting the time once. If you want to set the time in short intervals,
run the client program with -c as a service. If you want to set the
time only in longer intervals, such as once or twice a day or even
longer, it is better to run the client program using the AT scheduling
service.

- Copyright:

This code is in the public domain, but let me know if you make
improvements to it or fix something that I missed.

Kai Uwe Rommel

--
/* Kai Uwe Rommel                   ARS Computer & Consulting GmbH *
 * rommel@ars.de (http://www.ars.de)             Muenchen, Germany *
 * rommel@leo.org (http://www.leo.org/pub/comp/os/os2 maintenance) */

DOS ... is still a real mode only non-reentrant interrupt
handler, and always will be.                -Russell Williams





Appendix: The TZ environment variable

This variable is used to describe the time-zone information that the
locale will use.

The following command sets the standard time zone to CST, sets the
daylight saving time zone to CDT, and sets a difference of 6 hours
between CDT and Coordinated Universal Time (UTC).

	SET TZ=CST6CDT

When only the standard time zone is specified, the default difference
in hours from UTC is 0 instead of 5.

For the TZ variable, the SET command has the following format:


	SET TZ=SSS+h[:m:s]DDD[,sm,sw,sd,st,em,ew,ed,et,shift]

The values for the TZ variable are defined in the following table. The
default values given are for the POSIX C locale, the default locale
supported by IBM C and C++ Compilers.

Var.	Description

SSS 	Standard-timezone identifier. It must be three characters,
  	must begin with a letter, and can contain spaces. Zone names
  	are determined by local or country convention. For example,
  	EST stands for Eastern Standard Time and applies to parts of
  	North America.

h,m,s	The variable h specifies the difference (in hours) between the
	standard time zone and CUT, formerly Greenwich mean time
	(GMT). You can optionally use m to specify minutes after the
	hour, and s to specify seconds after the minute. A positive
	number denotes time zones west of the Greenwich meridian; a
        negative number denotes time zones east of the Greenwich
        meridian. The number must be an integer value.

DDD	Daylight saving time (DST) zone identifier. It must be three
        characters, must begin with a letter, and can contain spaces.

sm	Starting month (1 to 12) of DST.

sw	Starting week (-4 to 4) of DST. Use negative numbers to count
        back from the last week of the month (-1) and positive numbers
        to count from the first week (1).

sd	Starting day of DST.
        0 to 6 if sw != 0
        1 to 31 if sw = 0

st	Starting time (in seconds) of DST.

em	Ending month (1 to 12) of DST.

ew	Ending week (-4 to 4) of DST. Use negative numbers to count
	back from the last week of the month (-1) and positive numbers
	to count from the first week (1).</td>

ed	Ending day of DST.
        0 to 6 if ew != 0
        1 to 31 if ew = 0

et	Ending time of DST (in seconds).

shift	Amount of time change (in seconds).

If you give values for any of sm, sw, sd, st, em, ew, ed, et, or
shift, you must give values for all of them. Otherwise, the entire
statement is considered not valid, and the time zone information is
not changed.
