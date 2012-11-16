/*
 * Copyright (c) 1996 The Regents of the University of California.
 * All rights reserved.
 *
 * This module contributed by John Brezak <brezak@apollo.hp.com>.
 * January 31, 1996
 *
 * Additional contributions Orion Hodson (UCL) 1996-98.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: win32.c 4551 2010-01-07 16:16:09Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#ifndef lint
static char rcsid[] =
    "@(#) $Header$ (LBL)";
#endif

#include "config_win32.h"
#include "debug.h"
#include "util.h"

int
uname(struct utsname *ub)
{
        SYSTEM_INFO   sysinfo;
        OSVERSIONINFO osinfo;

        GetSystemInfo(&sysinfo);
        osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&osinfo);

        strcpy(ub->sysname, "Windows");
        strcpy(ub->nodename, "Unknown"); /* could use gethostbyname */

        /* ub->release and ub->machine*/
        if (osinfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
                sprintf(ub->release, "NT %d.%d", osinfo.dwMajorVersion, osinfo.dwMinorVersion);
                switch (sysinfo.wProcessorArchitecture) {
                case PROCESSOR_ARCHITECTURE_INTEL:
                        sprintf(ub->machine, "i%d86", sysinfo.wProcessorLevel);
                        break;
                case PROCESSOR_ARCHITECTURE_MIPS :
                        (void)strncpy(ub->machine, "mips", SYS_NMLN);
                        break;
                case PROCESSOR_ARCHITECTURE_ALPHA:
                        sprintf(ub->machine, "alpha-%d", sysinfo.wProcessorLevel);
                        break;
                case PROCESSOR_ARCHITECTURE_PPC:
                        (void)strncpy(ub->machine, "ppc", SYS_NMLN);
                        break;
                default:
                        (void)strncpy(ub->machine, "unknown", SYS_NMLN);
                        break;
                }
        } else if (osinfo.dwPlatformId == VER_PLATFORM_WIN32s) {
                strcpy(ub->release, "3.1");
                strcpy(ub->machine, "i386");
        } else if (osinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
                if (osinfo.dwMinorVersion == 0) {
                        strcpy(ub->release, "95");
                } else {
                        strcpy(ub->release, "98");
                }
                switch(sysinfo.dwProcessorType) {
                case PROCESSOR_INTEL_386:
                        strcpy(ub->machine, "i386");
                        break;
                case PROCESSOR_INTEL_486:
                        strcpy(ub->machine, "i486");
                        break;
                case PROCESSOR_INTEL_PENTIUM:
                        strcpy(ub->machine, "i586");
                        break;
                }
        } else {
                sprintf(ub->release, "Unknown %d.%d", osinfo.dwMajorVersion, osinfo.dwMinorVersion);
                sprintf(ub->machine, "Unknown");
        }

        /* Append service pack info onto end of release */
        if (osinfo.szCSDVersion != NULL) {
                DWORD avail = SYS_NMLN - strlen(ub->release);
                strncat(ub->release, osinfo.szCSDVersion, avail);
        }

        /* ub->version */
        sprintf(ub->version, "%lu", osinfo.dwBuildNumber);

        return TRUE;
}

uid_t
getuid(void)
{
    return 0;

}

gid_t
getgid(void)
{
    return 0;
}

unsigned long
gethostid(void)
{
    char   hostname[WSADESCRIPTION_LEN];
    struct hostent *he;

    if ((gethostname(hostname, WSADESCRIPTION_LEN) == 0) &&
        (he = gethostbyname(hostname)) != NULL) {
            return *((unsigned long*)he->h_addr_list[0]);
    }

    /*XXX*/
    return 0;
}

int
nice(int pri)
{
    return 0;
}

extern char *__progname;

void
ShowMessage(int level, char *msg)
{
    MessageBeep(level);
    MessageBox(NULL, msg, __progname,
	       level | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
}

static char szTemp[256];

#ifndef strcasestr
#include <ctype.h>
char *strcasestr(const char *haystack, const char *needle)
{
        int i=-1;
        while (haystack[++i] != '\0') {
                if (tolower(haystack[i]) == tolower(needle[0])) {
                        int j=i, k=0, match=0;
                        while (tolower(haystack[++j]) == tolower(needle[++k])) {
                                match=1;
                            // Catch case when they match at the end
                                  printf("j:%d, k:%d\n",j,k);
                                if (haystack[j] == '\0' && needle[k] == '\0') {
                                  printf("Mj:%d, k:%d\n",j,k);
                                        return &haystack[i];
                                }
                        }
                        // Catch normal case
                        if (match && needle[k] == '\0'){
                                  printf("Norm j:%d, k:%d\n",j,k);
                                return &haystack[i];
                        }
                }
        }
        return NULL;
}
#endif

#if 0
int
printf(const char *fmt, ...)
{
	int retval;

	va_list ap;
	va_start(ap, fmt);
	retval = _vsnprintf(szTemp, 256, fmt, ap);
	OutputDebugString(szTemp);
	va_end(ap);

	return retval;
}
#endif

int
fprintf(FILE *f, const char *fmt, ...)
{
	int 	retval;
	va_list	ap;

	va_start(ap, fmt);
	if (f == stderr) {
		retval = _vsnprintf(szTemp, 256, fmt, ap);
		OutputDebugString(szTemp);
	} else {
		retval = vfprintf(f, fmt, ap);
	}
	va_end(ap);
	return retval;
}

void
perror(const char *msg)
{
    DWORD cMsgLen;
    CHAR *msgBuf;
    DWORD dwError = GetLastError();

    cMsgLen = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
			    FORMAT_MESSAGE_ALLOCATE_BUFFER | 40, NULL,
			    dwError,
			    MAKELANGID(0, SUBLANG_ENGLISH_US),
			    (LPTSTR) &msgBuf, 512,
			    NULL);
    if (!cMsgLen)
	fprintf(stderr, "%s%sError code %lu\n",
		msg?msg:"", msg?": ":"", dwError);
    else {
	fprintf(stderr, "%s%s%s\n", msg?msg:"", msg?": ":"", msgBuf);
	LocalFree((HLOCAL)msgBuf);
    }
}

extern int main(int argc, const char *argv[]);
extern int __argc;
extern char **__argv;

static char argv0[255];		/* Buffer used to hold argv0. */

HINSTANCE hAppInstance;
HINSTANCE hAppPrevInstance;

char *__progname = "main";

#define WS_VERSION_ONE MAKEWORD(1,1)
#define WS_VERSION_TWO MAKEWORD(2,2)

int APIENTRY
WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow)
{
    char *p;
    WSADATA WSAdata;
    int r;
	if (WSAStartup(WS_VERSION_TWO, &WSAdata) != 0) {
		if (WSAStartup(WS_VERSION_ONE, &WSAdata) != 0) {
    		MessageBox(NULL, "Windows Socket initialization failed. TCP/IP stack\nis not installed or is damaged.", "Network Error", MB_OK | MB_ICONERROR);
			exit(-1);
		}
   		MessageBox(NULL, "Windows Socket initialization only found version One of TCP/IP stack\n Functionality may be limited.", "Network Error", MB_OK | MB_ICONERROR);
    }

    debug_msg("WSAStartup OK: %sz\nStatus:%s\n", WSAdata.szDescription, WSAdata.szSystemStatus);

    hAppInstance     = hInstance;
    hAppPrevInstance = hPrevInstance;

    SetMessageQueue(64);

    if (GetModuleFileName(NULL, argv0, 255) == 0) {
	    MessageBox(NULL, "GetModuleFileName failed\n", "Random Error", MB_OK | MB_ICONERROR);
	    exit(-1);
    }
    p = argv0;
    __progname = strrchr(p, '/');
    if (__progname != NULL) {
	__progname++;
    } else {
	__progname = strrchr(p, '\\');
	if (__progname != NULL) {
	    __progname++;
	} else {
	    __progname = p;
	}
    }

    r = main(__argc, (const char**)__argv);

    WSACleanup();
    return r;
}
