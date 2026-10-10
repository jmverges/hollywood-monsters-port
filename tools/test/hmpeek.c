/* hmpeek.exe <process.exe> <address> <length> <out-file>
 * Reads memory of a running Windows process into a file (used under Wine for the scripts' `dump` command).
 * Build: i686-w64-mingw32-gcc -O2 -o hmpeek.exe hmpeek.c */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 5) { fprintf(stderr, "usage: hmpeek <process.exe> <address> <length> <out-file>\n"); return 2; }
    unsigned long addr = strtoul(argv[2][0] == '*' ? argv[2] + 1 : argv[2], NULL, 0), len = strtoul(argv[3], NULL, 0);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe; pe.dwSize = sizeof pe;
    DWORD pid = 0;
    for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe))
        if (_stricmp(pe.szExeFile, argv[1]) == 0) { pid = pe.th32ProcessID; break; }
    CloseHandle(snap);
    if (!pid) { fprintf(stderr, "process %s not found\n", argv[1]); return 1; }
    HANDLE h = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    if (!h) { fprintf(stderr, "OpenProcess failed %lu\n", GetLastError()); return 1; }
    unsigned char *buf = malloc(len);
    SIZE_T n = 0;
    if (argv[2][0] == '*') {                       /* *ADDR: follow the 32-bit pointer stored at ADDR */
        unsigned int p = 0;
        ReadProcessMemory(h, (LPCVOID)strtoul(argv[2] + 1, NULL, 0), &p, 4, &n);
        addr = p;
    }
    BOOL ok = ReadProcessMemory(h, (LPCVOID)addr, buf, len, &n);
    CloseHandle(h);
    FILE *f = fopen(argv[4], "wb");
    if (f) { fwrite(buf, 1, n, f); fclose(f); }
    printf("dump %#lx+%lu -> %s (%s)\n", addr, len, argv[4], ok ? "ok" : "failed");
    return ok ? 0 : 1;
}
