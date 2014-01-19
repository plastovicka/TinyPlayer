/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#include "util.h"

#define MsubtLines 8
#define MINFONT 5
#define DEFAULTFRAME 23.976

struct Tline
{
	char *txt;
	int len;
	RECT rc;
};

class CSubtitles
{
public:
	LONGLONG pos, start, stop, delta, maxDelay;
	char *file, *ptr;
	int lines;
	bool on;
	bool disabled;
	double scale;
	Tline T[MsubtLines];

	CSubtitles(){ file=0; lines=0; on=false; disabled=false; }
	~CSubtitles(){ close(); }
	void load(TCHAR *fileName);
	void reset();
	void calcMaxDelay();
	void next();
	void timer();
	void draw(HDC dc);
	void clear();
	void close();
};

// Overlay callback handler object class
class COverlayCallback : public MyUnknown, public IDDrawExclModeVideoCallback
{
public:
	COverlayCallback();

	DECLARE_IUNKNOWN;

	STDMETHODIMP OnUpdateOverlay(BOOL  bBefore,
		DWORD dwFlags, BOOL  bOldVisible,
		const RECT *prcSrcOld, const RECT *prcDestOld,
		BOOL  bNewVisible,
		const RECT *prcSrcNew, const RECT *prcDestNew);

	STDMETHODIMP OnUpdateColorKey(COLORKEY const *pKey, DWORD dwColor);

	STDMETHODIMP OnUpdateSize(DWORD dwWidth, DWORD dwHeight,
		DWORD dwARWidth, DWORD dwARHeight);
};

HRESULT AddOvMToGraph();
void findSubtitles();
HRESULT createDirectDraw();
void restoreOvM();

extern COLORREF colorKeyRGB;         // key color for overlay mixer
extern LPDIRECTDRAW pDDObject;       // DirectDraw interface
extern LPDIRECTDRAWSURFACE pPrimary; // primary surface
extern IDDrawExclModeVideoCallback *pOverlayCallback;  // overlay callback handler interface

extern CSubtitles subtitles;
extern int subtOutLine, subtPosWin, subtPosFull;
extern LOGFONT subtFont;
extern TCHAR subtFolder[256];
extern HMODULE ddrawDll;
extern POINT monitorOffset;