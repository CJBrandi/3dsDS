/* Minimal AC service seam for host tests; production builds use devkitPro. */
#ifndef TEST_3DS_H
#define TEST_3DS_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef uintptr_t u32; /* Keep buffer pointers intact in the 64-bit host fake. */
typedef int32_t Result;
typedef unsigned Handle;
typedef struct { unsigned slot_mask; } acuConfig;
#define R_FAILED(r) ((r)<0)
#define R_SUCCEEDED(r) ((r)>=0)
#define AC_OPEN 0
#define AC_WPA2_AES 7
#define RESET_ONESHOT 0
static inline u32 IPC_MakeHeader(unsigned id,unsigned n,unsigned t) {return (id<<16)|(n<<6)|t;}
static inline u32 IPC_Desc_StaticBuffer(size_t size,unsigned id) {return (size<<14)|(id<<10)|2;}
static inline u32 IPC_Desc_CurProcessId(void) {return 0x20;}
static inline u32 IPC_Desc_SharedHandles(unsigned n) {return (n-1)<<26;}
u32 *getThreadCommandBuffer(void);
Handle *acGetSessionHandle(void);
Result svcSendSyncRequest(Handle h);
Result svcCloseHandle(Handle h);
Result svcCreateEvent(Handle *h,unsigned reset);
Result svcClearEvent(Handle h);
Result svcWaitSynchronization(Handle h,int64_t timeout);
Result ACI_LoadNetworkSetting(unsigned slot);
Result ACI_GetNetworkWirelessEssidSecuritySsid(void *ssid);
Result ACU_CreateDefaultConfig(acuConfig *c);
Result ACU_SetNetworkArea(acuConfig *c,unsigned area);
Result ACU_SetAllowApType(acuConfig *c,unsigned mask);
Result ACU_ConnectAsync(const acuConfig *c,Handle event);
Result ACU_GetWifiStatus(u32 *out);
Result ACU_GetSSID(char *out);
#endif
