#include <limits.h>
#include <malloc.h>
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>   /* abs() */
#include <string.h>
#include <winsock2.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "inet_pton.h"

#include <Iphlpapi.h>


static char *find_win32_interface(const char *addr)
{
	struct in_addr inaddr;
	char *iface = 0;

	if (inet_pton(AF_INET, addr, &inaddr))
	{
		MIB_IPFORWARDROW route;

		printf("got addr %x %s\n", inaddr.s_addr, inet_ntoa(inaddr));
		if (GetBestRoute(inaddr.s_addr, 0, &route) == NO_ERROR)
		{
			IP_ADAPTER_INFO oneinfo;
			PIP_ADAPTER_INFO allinfo = 0;

			unsigned long len;
			struct in_addr dst, mask, nexthop;

			dst.s_addr = route.dwForwardDest;
			mask.s_addr = route.dwForwardMask;
			nexthop.s_addr = route.dwForwardNextHop;

			printf("found route dst=%s mask=%s nexthop=%s ifindex=%d\n",
				inet_ntoa(dst), inet_ntoa(mask), inet_ntoa(nexthop),
				route.dwForwardIfIndex);

			len = sizeof(oneinfo);
			if (GetAdaptersInfo(&oneinfo, &len) == ERROR_SUCCESS)
			{
				printf("got allinfo in one\n");
				allinfo = &oneinfo;
			}
			else
			{
				allinfo = (PIP_ADAPTER_INFO) malloc(len);
				if (GetAdaptersInfo(allinfo, &len) != ERROR_SUCCESS)
					allinfo = 0;
			}

			if (allinfo)
			{


				PIP_ADAPTER_INFO a;
				{
					for (a = allinfo; a != 0; a = a->Next)
					{
						PIP_ADDR_STRING s;

						printf("name='%s' desc='%s' index=%d\n", 
							a->AdapterName, a->Description, a->Index);

						for (s = &a->IpAddressList; s != 0; s = s->Next)
						{
							printf("Address: '%s'\n", s->IpAddress.String);
						}
					}
				}
			}
#if 0
			len = sizeof(addrs);
			if (GetAdaptersAddresses(AF_INET, 0, 0, addrs, &len) == ERROR_SUCCESS)
			{
				PIP_ADAPTER_ADDRESSES a;

				a = addrs;

				while (a)
				{
					struct sockaddr_in *sockaddr = 0;


					if (a->FirstUnicastAddress)
						sockaddr = (struct sockaddr_in *) a->FirstUnicastAddress->Address.lpSockaddr;

					if (sockaddr)
					{
						printf("idx=%d name=%s addr=%s\n", 
							a->IfIndex, a->AdapterName, inet_ntoa(sockaddr->sin_addr));
					}
					else
					{
						printf("idx=%d name=%s addr=None\n", 
							a->IfIndex, a->AdapterName);
					}
					a = a->Next;
				}
			}
#endif
		}
	}

	return iface;
}

int main(int argc, char* argv[])
{
	find_win32_interface("224.1.2.3");
	return 0;
}

