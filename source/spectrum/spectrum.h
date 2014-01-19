/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#include "fft.h"

#define MinGain 15
#define MaxGain 600
#define GainNorm (MaxGain/3)
#define MinBars 1
#define MaxBars 127
#define MinPeakFall 1
#define MaxPeakFall 5
#define MinBarFall MaxPeakFall
#define MaxBarFall 50

struct TbarData {
	int bar;
	int peak;
};

class CSpectrumWindow {
public:
	HWND visualWnd;               // Visualization window
	HDC hdc;                      // device context
	LONG Width;                   // Client window width
	LONG Height;                  // Client window height
	bool dirty;                   // redraw trackbar captions

	DWORD lastTick;
	DWORD drawInterval;

	int nChannels;                // number of channels (2)
	int nBytesPerSample;          // bytes per sample (2)
	bool isFloat;

	HWND hwndDlg;                 // Handle for property dialog
	HWND hwndGain;
	HWND hwndBands;
	HWND hwndPeakFall;
	HWND hwndBarFall;

	HDC bmpDC;
	HBITMAP colorBitmap;
	HGDIOBJ oldBitmap;

	fft_state *state;
	SHORT srcBuffer[2][FFT_BUFFER_SIZE];
	TbarData barData[2][MaxBars];

	void DrawChannel(TbarData *dest, SHORT *src, int viewX);
	void drawBar(HDC dc, RECT *rc);
	void prepareSrc(BYTE *pBuffer, BYTE *pBeg, BYTE *pEnd);
	void updateBar(int &which, int value, int speed);
	void draw(BYTE *start, BYTE *index, BYTE *end);
	void processTrackbar(WPARAM wParam, LPARAM lParam);
	void invalidate();
	void initBitmap();
	void delBitmap();

	CSpectrumWindow(HWND visualWnd);
	~CSpectrumWindow();
	void init(HWND hwndDlg);
	void setFormat(WAVEFORMATEX *pwf);
	void setSize(int x, int y, HDC dc);
};

