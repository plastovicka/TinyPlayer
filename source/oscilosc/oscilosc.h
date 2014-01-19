/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */

class CScopeWindow
{
public:
	HWND visualWnd;               // Visualization window
	HDC hdc;                      // device context
	HPEN penLeft;                 // pen for left channel wave
	HPEN penRight;                // pen for right channel wave
	LONG Width;                   // Client window width
	LONG Height;                  // Client window height
	bool dirty;                   // redraw trackbar captions

	POINT *pPoints[2];            // Array of points for the left channel
	POINT *pPoints2;              // Array of points for the right channel
	int nPoints;                  // Size of pPoints1|2
	int PointsToDisplay2;         // Number of displayed points

	int nChannels;                // number of channels (2)
	int nSamplesPerSec;           // samples per second (44100)
	int nBytesPerSample;          // bytes per sample (2)
	bool isFloat;

	HWND hwndDlg;                 // Handle for property dialog
	HWND hwndGain;
	HWND hwndOffset;
	HWND hwndTimebase;

	void allocBuffers();
	void preparePoints(BYTE *pBeg, int len, BYTE *pBuffer, BYTE *pEnd);
	void DrawChannel1();
	void DrawChannel2();
	void draw1();
	void draw(BYTE *start, BYTE *index, BYTE *end);
	void ProcessVertScrollCommands(WPARAM wParam, LPARAM lParam);
	void invalidate();

	CScopeWindow(HWND visualWnd);
	~CScopeWindow();
	void init(HWND hwndDlg);
	void setFormat(WAVEFORMATEX *pwf);
	void setSize(int x, int y, HDC dc);
};

