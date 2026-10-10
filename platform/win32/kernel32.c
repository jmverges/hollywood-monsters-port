/* KERNEL32 / OLE32 subset: sleep, debug output, threads, events, disk info. */
#include "hm_platform.h"

void Sleep(DWORD dwMilliseconds) { host_sleep_ms(dwMilliseconds); }

void OutputDebugStringA(LPCSTR lpOutputString)
{
    HM_TRACE("OutputDebugString: %s", lpOutputString ? (const char *)lpOutputString : "");
}

struct thread_ctx { LPTHREAD_START_ROUTINE fn; LPVOID param; };
static unsigned thread_entry(void *p)
{
    struct thread_ctx *c = p;
    LPTHREAD_START_ROUTINE fn = c->fn;
    LPVOID param = c->param;
    host_free(c);
    return (unsigned)fn(param);
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress,
                    LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
    (void)lpThreadAttributes; (void)dwStackSize; (void)dwCreationFlags;
    struct thread_ctx *c = host_malloc(sizeof *c);
    c->fn = lpStartAddress;
    c->param = lpParameter;
    void *t = host_thread_create(thread_entry, c);
    static DWORD next_id = 100;
    if (lpThreadId) *lpThreadId = next_id++;
    HM_TRACE("CreateThread(%p) -> %p", (void *)lpStartAddress, t);
    return t ? (HANDLE)t : 0;
}

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName)
{
    (void)lpEventAttributes; (void)lpName;
    return (HANDLE)host_event_create(bManualReset != 0, bInitialState != 0);
}

DWORD WaitForMultipleObjects(DWORD nCount, HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
    int r = host_event_wait_any((int)nCount, (void **)lpHandles, bWaitAll != 0, dwMilliseconds);
    return r < 0 ? 0x102u /* WAIT_TIMEOUT */ : (DWORD)r;
}

BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector,
                       LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
    (void)lpRootPathName;
    if (lpSectorsPerCluster) *lpSectorsPerCluster = 8;
    if (lpBytesPerSector) *lpBytesPerSector = 512;
    if (lpNumberOfFreeClusters) *lpNumberOfFreeClusters = 0x40000;      /* 1 GB: the game multiplies these in 32 bits */
    if (lpTotalNumberOfClusters) *lpTotalNumberOfClusters = 0x80000;
    return 1;
}

HRESULT CoInitialize(LPVOID pvReserved) { (void)pvReserved; return 0; }
void CoUninitialize(void) { }
