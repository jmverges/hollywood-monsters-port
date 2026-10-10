/* hmpoke.exe <process.exe> <address> <8|16|32> <value>
 * Writes into the memory of a running Windows process (used under Wine to apply the test scripts' `poke`
 * commands to the original game). Build: i686-w64-mingw32-gcc -O2 -o hmpoke.exe hmpoke.c */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 5) { fprintf(stderr, "usage: hmpoke <process.exe> <address> <8|16|32> <value>\n"); return 2; }
    unsigned long addr = strtoul(argv[2], NULL, 0), val = strtoul(argv[4], NULL, 0);
    int bits = atoi(argv[3]);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe; pe.dwSize = sizeof pe;
    DWORD pid = 0;
    for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe))
        if (_stricmp(pe.szExeFile, argv[1]) == 0) { pid = pe.th32ProcessID; break; }
    CloseHandle(snap);
    if (!pid) { fprintf(stderr, "process %s not found\n", argv[1]); return 1; }
    HANDLE h = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
    if (!h) { fprintf(stderr, "OpenProcess failed %lu\n", GetLastError()); return 1; }
    SIZE_T n = 0;
    BOOL ok = WriteProcessMemory(h, (LPVOID)addr, &val, bits / 8, &n);
    CloseHandle(h);
    printf("poke%d %#lx = %lu -> %s\n", bits, addr, val, ok ? "ok" : "failed");
    return ok ? 0 : 1;
}
