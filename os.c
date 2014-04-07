/* os.c
 *
 * Author:   <rommel@ars.de>
 * Created: Sat Apr 26 1997
 */

static char *rcsid =
"$Id: os.c,v 1.8 2002/11/11 12:50:16 Rommel Exp Rommel $";
static char *rcsrev = "$Revision: 1.8 $";

/*
 * $Log: os.c,v $
 * Revision 1.8  2002/11/11 12:50:16  Rommel
 * fix OS/2 timezone resetting bug
 *
 * Revision 1.7  2002/05/21 06:50:50  Rommel
 * fixes psock_errno problem for OS/2
 *
 * Revision 1.6  1999/06/13 12:01:44  rommel
 * *** empty log message ***
 *
 * Revision 1.5  1999/03/05 15:47:06  rommel
 * added logging
 *
 * Revision 1.4  1999/02/27 14:16:51  rommel
 * changed NT service code
 *
 * Revision 1.3  1998/07/30 06:46:09  rommel
 * added Win32 port
 * added SNTP support
 * fixed many bugs
 * prepared for modem support
 *
 * Revision 1.2  1997/05/04 13:39:04  rommel
 * added NT service code
 *
 * Revision 1.1  1997/04/26 14:21:54  Rommel
 * Initial revision
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "os.h"

#ifdef OS2

#if defined(__UEL__)
#include <os2/os2.h>
#else
#define INCL_DOS
#include <os2.h>
#endif

int stime(time_t *newtime)
{
  struct tm *newtm = localtime(newtime);
  DATETIME dt;

  DosGetDateTime(&dt);

  dt.hours   = newtm -> tm_hour;
  dt.minutes = newtm -> tm_min;
  dt.seconds = newtm -> tm_sec;
  dt.hundredths = 0;

  dt.day     = newtm -> tm_mday;
  dt.month   = newtm -> tm_mon + 1;
  dt.year    = newtm -> tm_year + 1900;
  dt.weekday = newtm -> tm_wday;

  /* dt.timezone = -1; */

  return DosSetDateTime(&dt) != 0;
}

void print_sock_errno(char *text)
{
#if defined(__EMX__) || defined(__UEL__)
  int rc = errno;
  lprintf("%s: %s", text, strerror(rc));
#else
  int rc = sock_errno();
  lprintf("%s: error code %d", text, rc);
#endif
}

void print_h_errno(char *text)
{
  const char* msg;
  switch (h_errno)
  {
  case NETDB_INTERNAL:
    msg = "Internal error";
    break;
  case NETDB_SUCCESS:
    msg = "No error";
    break;
  case HOST_NOT_FOUND:
    msg = "Host not found";
    break;
  case TRY_AGAIN:
    msg = "Try again";
    break;
  case NO_RECOVERY:
    msg = "Non recoverable error";
    break;
  case NO_DATA:
    msg = "No such record";
    break;
  default:
    msg = "Unknown error";
  }

  lprintf("%s: %s", text, msg);
}

#endif

#ifdef WIN32

#include <windows.h>
#include <stdio.h>

int stime(time_t *newtime)
{
  struct tm *newtm = localtime(newtime);
  SYSTEMTIME dt;

  dt.wHour   = newtm -> tm_hour;
  dt.wMinute = newtm -> tm_min;
  dt.wSecond = newtm -> tm_sec;
  dt.wMilliseconds = 0;

  dt.wDay    = newtm -> tm_mday;
  dt.wMonth  = newtm -> tm_mon + 1;
  dt.wYear   = newtm -> tm_year + 1900;
  dt.wDayOfWeek = 0;

  return !SetLocalTime(&dt);
}

int sock_init(void)
{
  WSADATA wsaData;
  return WSAStartup(MAKEWORD(1, 1), &wsaData);
}

void print_sock_errno(char *text)
{
  int rc = WSAGetLastError();
  lprintf("%s: error code %d", text, rc);
}

static char *szServiceName;
static int (*fnServiceFunction)(void);
static int nServiceRC;

static SC_HANDLE schService;
static SC_HANDLE schSCManager;
static SERVICE_STATUS ssStatus;
static SERVICE_STATUS_HANDLE sshStatus;
static HANDLE hEventTerminate;
static HANDLE hThread;

static void StopService(LPTSTR lpszMsg)
{
  HANDLE hEventSource;
  CHAR chMsg[256];
  LPCTSTR lpszStrings[2];
  DWORD dwError = GetLastError();

  sprintf(chMsg, "%s error: %d", szServiceName, dwError);
  lpszStrings[0] = chMsg;
  lpszStrings[1] = lpszMsg;

  if ((hEventSource = RegisterEventSource(NULL, szServiceName)) != NULL)
  {
    ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 2, 0,
		lpszStrings, NULL);
    DeregisterEventSource(hEventSource);
  }

  SetEvent(hEventTerminate);
}

static BOOL ReportStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
			 DWORD dwCheckPoint, DWORD dwWaitHint)
{
  BOOL fResult;

  if (dwCurrentState == SERVICE_START_PENDING)
    ssStatus.dwControlsAccepted = 0;
  else
    ssStatus.dwControlsAccepted =
      SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;

  ssStatus.dwCurrentState = dwCurrentState;
  ssStatus.dwWin32ExitCode = dwWin32ExitCode;
  ssStatus.dwCheckPoint = dwCheckPoint;
  ssStatus.dwWaitHint = dwWaitHint;

  if (!(fResult = SetServiceStatus(sshStatus, &ssStatus)))
    StopService("SetServiceStatus");

  return fResult;
}

static VOID CALLBACK ServiceControl(DWORD dwCtrlCode)
{
  DWORD dwState = SERVICE_RUNNING;

  switch(dwCtrlCode)
  {

  case SERVICE_CONTROL_PAUSE:
    if (ssStatus.dwCurrentState == SERVICE_RUNNING)
    {
      SuspendThread(hThread);
      dwState = SERVICE_PAUSED;
    }
    break;

  case SERVICE_CONTROL_CONTINUE:
    if (ssStatus.dwCurrentState == SERVICE_PAUSED)
    {
      ResumeThread(hThread);
      dwState = SERVICE_RUNNING;
    }
    break;

  case SERVICE_CONTROL_STOP:
    dwState = SERVICE_STOP_PENDING;
    ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 1, 3000);
    SetEvent(hEventTerminate);
    return;

  case SERVICE_CONTROL_INTERROGATE:
    break;

  default:
    break;

  }

  ReportStatus(dwState, NO_ERROR, 0, 0);
}

static DWORD CALLBACK WorkerThread(LPVOID pArgs)
{
  nServiceRC = fnServiceFunction();
  return 0;
}

static void ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
  DWORD nTID;

  ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  ssStatus.dwServiceSpecificExitCode = 0;

  sshStatus = RegisterServiceCtrlHandler(TEXT("SimpleService"), ServiceControl);

  if (sshStatus)
  {
    if (ReportStatus(SERVICE_START_PENDING, NO_ERROR, 0, 0))
    {
      hEventTerminate = CreateEvent(NULL, TRUE, FALSE, NULL);

      if (hEventTerminate)
      {
	hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, &nTID);

	if (hThread)
	{
	  if (ReportStatus(SERVICE_RUNNING, NO_ERROR, 0, 0))
	    WaitForSingleObject(hEventTerminate, INFINITE);
	}

	CloseHandle(hEventTerminate);
      }
    }
  }

  ReportStatus(SERVICE_STOPPED, nServiceRC, 0, 0);

  return;
}

void run_as_service(char *name, int (*function)(void))
{
  SERVICE_TABLE_ENTRY dispatchTable[] =
  {
    {TEXT(name), (LPSERVICE_MAIN_FUNCTION) ServiceMain},
    {NULL, NULL}
  };

  szServiceName = name;
  fnServiceFunction = function;

  if (!StartServiceCtrlDispatcher(dispatchTable))
    StopService("StartServiceCtrlDispatcher failed.");
}

int started_as_service(void)
{
  STARTUPINFO si;
  GetStartupInfo(&si);
  return (si.wShowWindow == 0);
}

int install_as_service(char *name, char *title, int install)
{
  char szPath[512];
  int rc = 0;

  GetModuleFileName(NULL, szPath, sizeof(szPath));

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

  if (install)
  {
    schService = CreateService(schSCManager, name, title, SERVICE_ALL_ACCESS,
      SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
      SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, szPath,
      NULL, NULL, NULL, NULL, NULL);

    if (!schService)
      rc = GetLastError();
    else
      CloseServiceHandle(schService);
  }
  else
  {
    schService = OpenService(schSCManager, name, SERVICE_ALL_ACCESS);

    if (schService)
      if (!DeleteService(schService))
	rc = GetLastError();
  }

  CloseServiceHandle(schSCManager);

  return rc;
}

int save_options(char *name, void *data, int size)
{
  char keyname[512];
  HKEY key;
  DWORD rc;

  strcpy(keyname, "System\\CurrentControlSet\\Services\\");
  strcat(keyname, name);

  if ((rc = RegOpenKey(HKEY_LOCAL_MACHINE, keyname, &key)) != 0)
    return rc;
  rc = RegSetValueEx(key, "Parameters", 0, REG_BINARY, data, size);
  RegCloseKey(key);

  return rc;
}

int restore_options(char *name, void *data, int size)
{
  char keyname[512];
  HKEY key;
  DWORD rc, type, bytes;

  strcpy(keyname, "System\\CurrentControlSet\\Services\\");
  strcat(keyname, name);

  if ((rc = RegOpenKey(HKEY_LOCAL_MACHINE, keyname, &key)) != 0)
    return rc;
  bytes = size;
  rc = RegQueryValueEx(key, "Parameters", 0, &type, data, &bytes);
  RegCloseKey(key);

  return rc;
}

#endif

char *logfile;

int lprintf(char *format, ...)
{
  va_list argptr;
  char buffer[1024];
  int count;
  char filename[256], datetime[32];
  FILE *log;
  time_t now;
  struct tm *tm;

  time(&now);
  tm = localtime(&now);
  count = strftime(buffer, sizeof(buffer), "%m/%d/%y-%H:%M:%S ", tm);

  va_start(argptr, format);
  count += vsprintf(buffer + count, format, argptr);
  va_end(argptr);

  if (logfile != NULL && (log = fopen(logfile, "a")) != NULL)
  {
    fputs(buffer, log);
    putc('\n', log);
    fclose(log);
  }

  fputs(buffer, stdout);
  putc('\n', stdout);
  fflush(stdout);
 
  return count;
}

/* end of os.c */
