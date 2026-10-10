/* Minimal DirectDraw 7 / DirectSound declarations for Ghidra typing (32-bit layout). */
typedef struct _DDCOLORKEY { unsigned long dwColorSpaceLowValue; unsigned long dwColorSpaceHighValue; } DDCOLORKEY;
typedef struct _DDPIXELFORMAT {
    unsigned long dwSize; unsigned long dwFlags; unsigned long dwFourCC; unsigned long dwRGBBitCount;
    unsigned long dwRBitMask; unsigned long dwGBitMask; unsigned long dwBBitMask; unsigned long dwRGBAlphaBitMask;
} DDPIXELFORMAT;
typedef struct _DDSCAPS2 { unsigned long dwCaps; unsigned long dwCaps2; unsigned long dwCaps3; unsigned long dwCaps4; } DDSCAPS2;
typedef struct _DDSURFACEDESC2 {
    unsigned long dwSize; unsigned long dwFlags; unsigned long dwHeight; unsigned long dwWidth;
    long lPitch; unsigned long dwBackBufferCount; unsigned long dwMipMapCount; unsigned long dwAlphaBitDepth;
    unsigned long dwReserved; void *lpSurface;
    DDCOLORKEY ddckCKDestOverlay; DDCOLORKEY ddckCKDestBlt; DDCOLORKEY ddckCKSrcOverlay; DDCOLORKEY ddckCKSrcBlt;
    DDPIXELFORMAT ddpfPixelFormat; DDSCAPS2 ddsCaps; unsigned long dwTextureStage;
} DDSURFACEDESC2;
typedef struct _DDBLTFX { unsigned long dwSize; unsigned long rest[24]; } DDBLTFX;
typedef struct _DXRECT { long left; long top; long right; long bottom; } DXRECT;
typedef struct _DXPALETTEENTRY { unsigned char peRed; unsigned char peGreen; unsigned char peBlue; unsigned char peFlags; } DXPALETTEENTRY;
typedef struct _DXWAVEFORMATEX { unsigned short wFormatTag; unsigned short nChannels; unsigned long nSamplesPerSec;
    unsigned long nAvgBytesPerSec; unsigned short nBlockAlign; unsigned short wBitsPerSample; unsigned short cbSize; } DXWAVEFORMATEX;
typedef struct _DSBUFFERDESC { unsigned long dwSize; unsigned long dwFlags; unsigned long dwBufferBytes; unsigned long dwReserved;
    DXWAVEFORMATEX *lpwfxFormat; unsigned long guid3D[4]; } DSBUFFERDESC;
typedef struct _DSBCAPS { unsigned long dwSize; unsigned long dwFlags; unsigned long dwBufferBytes; unsigned long dwUnlockTransferRate; unsigned long dwPlayCpuOverhead; } DSBCAPS;

struct IDirectDrawPalette;
struct IDirectDrawSurface7;
struct IDirectDraw7;
struct IDirectSound;
struct IDirectSoundBuffer;

typedef struct IDirectDrawPaletteVtbl {
    long (__stdcall *QueryInterface)(struct IDirectDrawPalette *This, void *riid, void **ppv);
    unsigned long (__stdcall *AddRef)(struct IDirectDrawPalette *This);
    unsigned long (__stdcall *Release)(struct IDirectDrawPalette *This);
    long (__stdcall *GetCaps)(struct IDirectDrawPalette *This, unsigned long *lpdwCaps);
    long (__stdcall *GetEntries)(struct IDirectDrawPalette *This, unsigned long dwFlags, unsigned long dwBase, unsigned long dwNumEntries, DXPALETTEENTRY *lpEntries);
    long (__stdcall *Initialize)(struct IDirectDrawPalette *This, struct IDirectDraw7 *lpDD, unsigned long dwFlags, DXPALETTEENTRY *lpDDColorTable);
    long (__stdcall *SetEntries)(struct IDirectDrawPalette *This, unsigned long dwFlags, unsigned long dwStartingEntry, unsigned long dwCount, DXPALETTEENTRY *lpEntries);
} IDirectDrawPaletteVtbl;
typedef struct IDirectDrawPalette { IDirectDrawPaletteVtbl *lpVtbl; } IDirectDrawPalette;

typedef struct IDirectDrawSurface7Vtbl {
    long (__stdcall *QueryInterface)(struct IDirectDrawSurface7 *This, void *riid, void **ppv);
    unsigned long (__stdcall *AddRef)(struct IDirectDrawSurface7 *This);
    unsigned long (__stdcall *Release)(struct IDirectDrawSurface7 *This);
    long (__stdcall *AddAttachedSurface)(struct IDirectDrawSurface7 *This, struct IDirectDrawSurface7 *lpDDSAttachedSurface);
    long (__stdcall *AddOverlayDirtyRect)(struct IDirectDrawSurface7 *This, DXRECT *lpRect);
    long (__stdcall *Blt)(struct IDirectDrawSurface7 *This, DXRECT *lpDestRect, struct IDirectDrawSurface7 *lpDDSrcSurface, DXRECT *lpSrcRect, unsigned long dwFlags, DDBLTFX *lpDDBltFx);
    long (__stdcall *BltBatch)(struct IDirectDrawSurface7 *This, void *lpDDBltBatch, unsigned long dwCount, unsigned long dwFlags);
    long (__stdcall *BltFast)(struct IDirectDrawSurface7 *This, unsigned long dwX, unsigned long dwY, struct IDirectDrawSurface7 *lpDDSrcSurface, DXRECT *lpSrcRect, unsigned long dwTrans);
    long (__stdcall *DeleteAttachedSurface)(struct IDirectDrawSurface7 *This, unsigned long dwFlags, struct IDirectDrawSurface7 *lpDDSAttachedSurface);
    long (__stdcall *EnumAttachedSurfaces)(struct IDirectDrawSurface7 *This, void *lpContext, void *lpEnumSurfacesCallback);
    long (__stdcall *EnumOverlayZOrders)(struct IDirectDrawSurface7 *This, unsigned long dwFlags, void *lpContext, void *lpfnCallback);
    long (__stdcall *Flip)(struct IDirectDrawSurface7 *This, struct IDirectDrawSurface7 *lpDDSurfaceTargetOverride, unsigned long dwFlags);
    long (__stdcall *GetAttachedSurface)(struct IDirectDrawSurface7 *This, DDSCAPS2 *lpDDSCaps, struct IDirectDrawSurface7 **lplpDDAttachedSurface);
    long (__stdcall *GetBltStatus)(struct IDirectDrawSurface7 *This, unsigned long dwFlags);
    long (__stdcall *GetCaps)(struct IDirectDrawSurface7 *This, DDSCAPS2 *lpDDSCaps);
    long (__stdcall *GetClipper)(struct IDirectDrawSurface7 *This, void **lplpDDClipper);
    long (__stdcall *GetColorKey)(struct IDirectDrawSurface7 *This, unsigned long dwFlags, DDCOLORKEY *lpDDColorKey);
    long (__stdcall *GetDC)(struct IDirectDrawSurface7 *This, void **lphDC);
    long (__stdcall *GetFlipStatus)(struct IDirectDrawSurface7 *This, unsigned long dwFlags);
    long (__stdcall *GetOverlayPosition)(struct IDirectDrawSurface7 *This, long *lplX, long *lplY);
    long (__stdcall *GetPalette)(struct IDirectDrawSurface7 *This, struct IDirectDrawPalette **lplpDDPalette);
    long (__stdcall *GetPixelFormat)(struct IDirectDrawSurface7 *This, DDPIXELFORMAT *lpDDPixelFormat);
    long (__stdcall *GetSurfaceDesc)(struct IDirectDrawSurface7 *This, DDSURFACEDESC2 *lpDDSurfaceDesc);
    long (__stdcall *Initialize)(struct IDirectDrawSurface7 *This, struct IDirectDraw7 *lpDD, DDSURFACEDESC2 *lpDDSurfaceDesc);
    long (__stdcall *IsLost)(struct IDirectDrawSurface7 *This);
    long (__stdcall *Lock)(struct IDirectDrawSurface7 *This, DXRECT *lpDestRect, DDSURFACEDESC2 *lpDDSurfaceDesc, unsigned long dwFlags, void *hEvent);
    long (__stdcall *ReleaseDC)(struct IDirectDrawSurface7 *This, void *hDC);
    long (__stdcall *Restore)(struct IDirectDrawSurface7 *This);
    long (__stdcall *SetClipper)(struct IDirectDrawSurface7 *This, void *lpDDClipper);
    long (__stdcall *SetColorKey)(struct IDirectDrawSurface7 *This, unsigned long dwFlags, DDCOLORKEY *lpDDColorKey);
    long (__stdcall *SetOverlayPosition)(struct IDirectDrawSurface7 *This, long lX, long lY);
    long (__stdcall *SetPalette)(struct IDirectDrawSurface7 *This, struct IDirectDrawPalette *lpDDPalette);
    long (__stdcall *Unlock)(struct IDirectDrawSurface7 *This, DXRECT *lpRect);
    long (__stdcall *UpdateOverlay)(struct IDirectDrawSurface7 *This, DXRECT *lpSrcRect, struct IDirectDrawSurface7 *lpDDDestSurface, DXRECT *lpDestRect, unsigned long dwFlags, void *lpDDOverlayFx);
    long (__stdcall *UpdateOverlayDisplay)(struct IDirectDrawSurface7 *This, unsigned long dwFlags);
    long (__stdcall *UpdateOverlayZOrder)(struct IDirectDrawSurface7 *This, unsigned long dwFlags, struct IDirectDrawSurface7 *lpDDSReference);
    long (__stdcall *GetDDInterface)(struct IDirectDrawSurface7 *This, void **lplpDD);
    long (__stdcall *PageLock)(struct IDirectDrawSurface7 *This, unsigned long dwFlags);
    long (__stdcall *PageUnlock)(struct IDirectDrawSurface7 *This, unsigned long dwFlags);
    long (__stdcall *SetSurfaceDesc)(struct IDirectDrawSurface7 *This, DDSURFACEDESC2 *lpDDsd2, unsigned long dwFlags);
    long (__stdcall *SetPrivateData)(struct IDirectDrawSurface7 *This, void *guidTag, void *lpData, unsigned long cbSize, unsigned long dwFlags);
    long (__stdcall *GetPrivateData)(struct IDirectDrawSurface7 *This, void *guidTag, void *lpBuffer, unsigned long *lpcbBufferSize);
    long (__stdcall *FreePrivateData)(struct IDirectDrawSurface7 *This, void *guidTag);
    long (__stdcall *GetUniquenessValue)(struct IDirectDrawSurface7 *This, unsigned long *lpValue);
    long (__stdcall *ChangeUniquenessValue)(struct IDirectDrawSurface7 *This);
    long (__stdcall *SetPriority)(struct IDirectDrawSurface7 *This, unsigned long dwPriority);
    long (__stdcall *GetPriority)(struct IDirectDrawSurface7 *This, unsigned long *lpdwPriority);
    long (__stdcall *SetLOD)(struct IDirectDrawSurface7 *This, unsigned long dwMaxLOD);
    long (__stdcall *GetLOD)(struct IDirectDrawSurface7 *This, unsigned long *lpdwMaxLOD);
} IDirectDrawSurface7Vtbl;
typedef struct IDirectDrawSurface7 { IDirectDrawSurface7Vtbl *lpVtbl; } IDirectDrawSurface7;

typedef struct IDirectDraw7Vtbl {
    long (__stdcall *QueryInterface)(struct IDirectDraw7 *This, void *riid, void **ppv);
    unsigned long (__stdcall *AddRef)(struct IDirectDraw7 *This);
    unsigned long (__stdcall *Release)(struct IDirectDraw7 *This);
    long (__stdcall *Compact)(struct IDirectDraw7 *This);
    long (__stdcall *CreateClipper)(struct IDirectDraw7 *This, unsigned long dwFlags, void **lplpDDClipper, void *pUnkOuter);
    long (__stdcall *CreatePalette)(struct IDirectDraw7 *This, unsigned long dwFlags, DXPALETTEENTRY *lpDDColorArray, struct IDirectDrawPalette **lplpDDPalette, void *pUnkOuter);
    long (__stdcall *CreateSurface)(struct IDirectDraw7 *This, DDSURFACEDESC2 *lpDDSurfaceDesc2, struct IDirectDrawSurface7 **lplpDDSurface, void *pUnkOuter);
    long (__stdcall *DuplicateSurface)(struct IDirectDraw7 *This, struct IDirectDrawSurface7 *lpDDSurface, struct IDirectDrawSurface7 **lplpDupDDSurface);
    long (__stdcall *EnumDisplayModes)(struct IDirectDraw7 *This, unsigned long dwFlags, DDSURFACEDESC2 *lpDDSurfaceDesc2, void *lpContext, void *lpEnumModesCallback);
    long (__stdcall *EnumSurfaces)(struct IDirectDraw7 *This, unsigned long dwFlags, DDSURFACEDESC2 *lpDDSD2, void *lpContext, void *lpEnumSurfacesCallback);
    long (__stdcall *FlipToGDISurface)(struct IDirectDraw7 *This);
    long (__stdcall *GetCaps)(struct IDirectDraw7 *This, void *lpDDDriverCaps, void *lpDDHELCaps);
    long (__stdcall *GetDisplayMode)(struct IDirectDraw7 *This, DDSURFACEDESC2 *lpDDSurfaceDesc2);
    long (__stdcall *GetFourCCCodes)(struct IDirectDraw7 *This, unsigned long *lpNumCodes, unsigned long *lpCodes);
    long (__stdcall *GetGDISurface)(struct IDirectDraw7 *This, struct IDirectDrawSurface7 **lplpGDIDDSSurface);
    long (__stdcall *GetMonitorFrequency)(struct IDirectDraw7 *This, unsigned long *lpdwFrequency);
    long (__stdcall *GetScanLine)(struct IDirectDraw7 *This, unsigned long *lpdwScanLine);
    long (__stdcall *GetVerticalBlankStatus)(struct IDirectDraw7 *This, int *lpbIsInVB);
    long (__stdcall *Initialize)(struct IDirectDraw7 *This, void *lpGUID);
    long (__stdcall *RestoreDisplayMode)(struct IDirectDraw7 *This);
    long (__stdcall *SetCooperativeLevel)(struct IDirectDraw7 *This, void *hWnd, unsigned long dwFlags);
    long (__stdcall *SetDisplayMode)(struct IDirectDraw7 *This, unsigned long dwWidth, unsigned long dwHeight, unsigned long dwBPP, unsigned long dwRefreshRate, unsigned long dwFlags);
    long (__stdcall *WaitForVerticalBlank)(struct IDirectDraw7 *This, unsigned long dwFlags, void *hEvent);
    long (__stdcall *GetAvailableVidMem)(struct IDirectDraw7 *This, DDSCAPS2 *lpDDSCaps2, unsigned long *lpdwTotal, unsigned long *lpdwFree);
    long (__stdcall *GetSurfaceFromDC)(struct IDirectDraw7 *This, void *hdc, struct IDirectDrawSurface7 **lpDDS);
    long (__stdcall *RestoreAllSurfaces)(struct IDirectDraw7 *This);
    long (__stdcall *TestCooperativeLevel)(struct IDirectDraw7 *This);
    long (__stdcall *GetDeviceIdentifier)(struct IDirectDraw7 *This, void *lpdddi, unsigned long dwFlags);
    long (__stdcall *StartModeTest)(struct IDirectDraw7 *This, void *lpModesToTest, unsigned long dwNumEntries, unsigned long dwFlags);
    long (__stdcall *EvaluateMode)(struct IDirectDraw7 *This, unsigned long dwFlags, unsigned long *pSecondsUntilTimeout);
} IDirectDraw7Vtbl;
typedef struct IDirectDraw7 { IDirectDraw7Vtbl *lpVtbl; } IDirectDraw7;

typedef struct IDirectSoundBufferVtbl {
    long (__stdcall *QueryInterface)(struct IDirectSoundBuffer *This, void *riid, void **ppv);
    unsigned long (__stdcall *AddRef)(struct IDirectSoundBuffer *This);
    unsigned long (__stdcall *Release)(struct IDirectSoundBuffer *This);
    long (__stdcall *GetCaps)(struct IDirectSoundBuffer *This, DSBCAPS *pDSBufferCaps);
    long (__stdcall *GetCurrentPosition)(struct IDirectSoundBuffer *This, unsigned long *pdwCurrentPlayCursor, unsigned long *pdwCurrentWriteCursor);
    long (__stdcall *GetFormat)(struct IDirectSoundBuffer *This, DXWAVEFORMATEX *pwfxFormat, unsigned long dwSizeAllocated, unsigned long *pdwSizeWritten);
    long (__stdcall *GetVolume)(struct IDirectSoundBuffer *This, long *plVolume);
    long (__stdcall *GetPan)(struct IDirectSoundBuffer *This, long *plPan);
    long (__stdcall *GetFrequency)(struct IDirectSoundBuffer *This, unsigned long *pdwFrequency);
    long (__stdcall *GetStatus)(struct IDirectSoundBuffer *This, unsigned long *pdwStatus);
    long (__stdcall *Initialize)(struct IDirectSoundBuffer *This, struct IDirectSound *pDirectSound, DSBUFFERDESC *pcDSBufferDesc);
    long (__stdcall *Lock)(struct IDirectSoundBuffer *This, unsigned long dwOffset, unsigned long dwBytes, void **ppvAudioPtr1, unsigned long *pdwAudioBytes1, void **ppvAudioPtr2, unsigned long *pdwAudioBytes2, unsigned long dwFlags);
    long (__stdcall *Play)(struct IDirectSoundBuffer *This, unsigned long dwReserved1, unsigned long dwPriority, unsigned long dwFlags);
    long (__stdcall *SetCurrentPosition)(struct IDirectSoundBuffer *This, unsigned long dwNewPosition);
    long (__stdcall *SetFormat)(struct IDirectSoundBuffer *This, DXWAVEFORMATEX *pcfxFormat);
    long (__stdcall *SetVolume)(struct IDirectSoundBuffer *This, long lVolume);
    long (__stdcall *SetPan)(struct IDirectSoundBuffer *This, long lPan);
    long (__stdcall *SetFrequency)(struct IDirectSoundBuffer *This, unsigned long dwFrequency);
    long (__stdcall *Stop)(struct IDirectSoundBuffer *This);
    long (__stdcall *Unlock)(struct IDirectSoundBuffer *This, void *pvAudioPtr1, unsigned long dwAudioBytes1, void *pvAudioPtr2, unsigned long dwAudioBytes2);
    long (__stdcall *Restore)(struct IDirectSoundBuffer *This);
} IDirectSoundBufferVtbl;
typedef struct IDirectSoundBuffer { IDirectSoundBufferVtbl *lpVtbl; } IDirectSoundBuffer;

typedef struct IDirectSoundVtbl {
    long (__stdcall *QueryInterface)(struct IDirectSound *This, void *riid, void **ppv);
    unsigned long (__stdcall *AddRef)(struct IDirectSound *This);
    unsigned long (__stdcall *Release)(struct IDirectSound *This);
    long (__stdcall *CreateSoundBuffer)(struct IDirectSound *This, DSBUFFERDESC *pcDSBufferDesc, struct IDirectSoundBuffer **ppDSBuffer, void *pUnkOuter);
    long (__stdcall *GetCaps)(struct IDirectSound *This, void *pDSCaps);
    long (__stdcall *DuplicateSoundBuffer)(struct IDirectSound *This, struct IDirectSoundBuffer *pDSBufferOriginal, struct IDirectSoundBuffer **ppDSBufferDuplicate);
    long (__stdcall *SetCooperativeLevel)(struct IDirectSound *This, void *hwnd, unsigned long dwLevel);
    long (__stdcall *Compact)(struct IDirectSound *This);
    long (__stdcall *GetSpeakerConfig)(struct IDirectSound *This, unsigned long *pdwSpeakerConfig);
    long (__stdcall *SetSpeakerConfig)(struct IDirectSound *This, unsigned long dwSpeakerConfig);
    long (__stdcall *Initialize)(struct IDirectSound *This, void *pcGuidDevice);
} IDirectSoundVtbl;
typedef struct IDirectSound { IDirectSoundVtbl *lpVtbl; } IDirectSound;


typedef struct _DSBPOSITIONNOTIFY { unsigned long dwOffset; void *hEventNotify; } DSBPOSITIONNOTIFY;
struct IDirectSoundNotify;
typedef struct IDirectSoundNotifyVtbl {
    long (__stdcall *QueryInterface)(struct IDirectSoundNotify *This, void *riid, void **ppv);
    unsigned long (__stdcall *AddRef)(struct IDirectSoundNotify *This);
    unsigned long (__stdcall *Release)(struct IDirectSoundNotify *This);
    long (__stdcall *SetNotificationPositions)(struct IDirectSoundNotify *This, unsigned long dwPositionNotifies, DSBPOSITIONNOTIFY *pcPositionNotifies);
} IDirectSoundNotifyVtbl;
typedef struct IDirectSoundNotify { IDirectSoundNotifyVtbl *lpVtbl; } IDirectSoundNotify;

long __stdcall DirectDrawCreateEx(void *lpGuid, void **lplpDD, void *iid, void *pUnkOuter);
long __stdcall DirectSoundCreate(void *pcGuidDevice, struct IDirectSound **ppDS, void *pUnkOuter);
