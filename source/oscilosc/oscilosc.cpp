/*
 (C) 2004-2007  Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdio.h>
#include <mmreg.h>
#include "oscilosc.h"
#include "resource.h"

#define MaxBits 9
#define MaxOffset 100
#define MinGain 15
#define MaxGain 600
#define GainNorm (MaxGain/3)
#define MinScale 20                    
#define MaxScale 1000
#define ScaleNorm (MaxScale/4)

HINSTANCE inst;
int Gain = GainNorm;
int Scale= ScaleNorm;
int Offset=0;

enum{ clLeft, clRight, clSeparator, clBkgnd, Ncl };
COLORREF colors[]={0x00ff00, 0xff4040, 0x808080, 0x000000};

#define endA(a) (a+(sizeof(a)/sizeof(*a)))
#define sizeA(A) (sizeof(A)/sizeof(*A))
template <class T> inline void aminmax(T &x, int l, int h){
	if(x<l) x=l;
	if(x>h) x=h;
}

GUID guidFloat ={0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};

//------------------------------------------------------------------

const TCHAR *subkey= TEXT("Software\\Petr Lastovicka\\player\\oscilosc");
struct Treg { TCHAR *s; int *i; } regVal[]={
	{TEXT("gain"), &Gain},
	{TEXT("offset"), &Offset},
	{TEXT("scale"), &Scale},
};

void readini()
{
	HKEY key;
	DWORD d;
	if(RegOpenKeyEx(HKEY_CURRENT_USER, subkey, 0, KEY_QUERY_VALUE, &key)==ERROR_SUCCESS){
		for(Treg *u=regVal; u<endA(regVal); u++){
			d=sizeof(int);
			RegQueryValueEx(key, u->s, 0, 0, (BYTE *)u->i, &d);
		}
		d=sizeof(colors);
		RegQueryValueEx(key, TEXT("colors"), 0, 0, (BYTE *)colors, &d);
		RegCloseKey(key);
	}
}

void writeini()
{
	HKEY key;
	if(RegCreateKey(HKEY_CURRENT_USER, subkey, &key)==ERROR_SUCCESS){
		for(Treg *u=regVal; u<endA(regVal); u++){
			RegSetValueEx(key, u->s, 0, REG_DWORD,
				(BYTE *)u->i, sizeof(int));
		}
		RegSetValueEx(key, TEXT("colors"), 0, REG_BINARY, (BYTE *)colors, sizeof(colors));
		RegCloseKey(key);
	}
}

//------------------------------------------------------------------

CScopeWindow::CScopeWindow(HWND visualWnd) :
visualWnd(visualWnd),
		hwndDlg(NULL)
{
	pPoints[0]=pPoints[1]=0;
	readini();
	aminmax(Scale, MinScale, MaxScale);
	aminmax(Gain, MinGain, MaxGain);
	aminmax(Offset, 0, MaxOffset);
}

CScopeWindow::~CScopeWindow()
{
	delete[] pPoints[0];
	delete[] pPoints[1];
	writeini();
}

//property dialog box initialization
void CScopeWindow::init(HWND hWnd)
{
	this->hwndDlg = hWnd;
	hwndGain =     GetDlgItem(hWnd, IDC_GAIN);
	hwndOffset =   GetDlgItem(hWnd, IDC_OFFSET);
	hwndTimebase = GetDlgItem(hWnd, IDC_TIMEBASE);

	//set the scroll ranges for the trackbars
	SendMessage(hwndGain, TBM_SETRANGE, TRUE, MAKELONG(MinGain, MaxGain));
	SendMessage(hwndGain, TBM_SETPOS, TRUE, (LPARAM)Gain);
	SendMessage(hwndOffset, TBM_SETRANGE, TRUE, MAKELONG(0, MaxOffset));
	SendMessage(hwndOffset, TBM_SETPOS, TRUE, (LPARAM)Offset);
	SendMessage(hwndTimebase, TBM_SETRANGE, TRUE, MAKELONG(MinScale, MaxScale));
	SendMessage(hwndTimebase, TBM_SETPOS, TRUE, (LPARAM)Scale);

	invalidate();
}


//trackbar message
void CScopeWindow::ProcessVertScrollCommands(WPARAM wParam, LPARAM lParam)
{
	int pos;
	int command = LOWORD(wParam);

	if(command != TB_ENDTRACK && command != TB_THUMBTRACK &&
			command != TB_LINEDOWN && command != TB_LINEUP &&
			command != TB_PAGEUP && command != TB_PAGEDOWN)
			return;

	pos = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0L);

	if((HWND)lParam == hwndGain){
		Gain = pos;
	}
	else if((HWND)lParam == hwndOffset){
		Offset = pos;
	}
	else if((HWND)lParam == hwndTimebase){
		Scale = pos;
	}
	invalidate();
}


/*
//searches backward for a positive going zero crossing in the waveform
int CScopeWindow::SearchForPosZeroCrossing(int StartPoint)
{
int last,cur,j;
BYTE *p = pBuffer + StartPoint*nBytesPerSample*nChannels;
const int n=500;

last=-1;
for(j = n; j > 0; j--){
if(p<=pBuffer) p=pBuffer+nBuf;
p-=nChannels*nBytesPerSample;
switch(nBytesPerSample){
default:
cur = int(*(BYTE*)p)-128;
break;
case 2:
cur = *(SHORT*)p;
break;
case 4:
cur = *(LONG*)p;
break;
}
if(cur < 0 && last >= 0) break;
last = cur;
}
return (p-pBuffer)/nBytesPerSample/nChannels;
}
*/
//------------------------------------------------------------------

//copy samples from pBuffer to pPoints1|2
void CScopeWindow::preparePoints(BYTE *pBeg, int len, BYTE *pBuffer, BYTE *pEnd)
{
	int i, c;

	if(nBytesPerSample>2 || nChannels>2){
		BYTE *p = pBeg+nBytesPerSample;
		for(i=0; i<len; i++){
			for(c=0; c<nChannels; c++){
				if(c<2){
					pPoints[c][i].y =
						isFloat ? (LONG)(((float*)p)[-1] * (1<<(MaxBits-1))) :
						((((signed char)p[-1])<<8)+p[-2])>>(16-MaxBits);
				}
				if(p==pEnd) p=pBuffer;
				p+=nBytesPerSample;
			}
		}
	}
	else if(nBytesPerSample==2){
		SHORT *p = (SHORT*)pBeg;
		if(nChannels>1){
			for(i=0; i<len; i++){
				if((BYTE*)p==pEnd) p=(SHORT*)pBuffer;
				pPoints[0][i].y = (*p++)>>(16-MaxBits);
				pPoints[1][i].y = (*p++)>>(16-MaxBits);
			}
		}
		else{
			for(i=0; i<len; i++){
				if((BYTE*)p==pEnd) p=(SHORT*)pBuffer;
				pPoints[0][i].y = (*p++)>>(16-MaxBits);
			}
		}
	}
	else{ //nBytesPerSample==1
		BYTE *p = pBeg;
		for(i=0; i<len; i++){
			if((BYTE*)p==pEnd) p=pBuffer;
			pPoints[0][i].y = (*p++ - 128)<<(MaxBits-8);
			if(nChannels>1) pPoints[1][i].y = (*p++ - 128)<<(MaxBits-8);
		}
	}
}

void CScopeWindow::DrawChannel1()
{
	int o= Height/2;
	if(nChannels>1) o -= Height*Offset/(MaxOffset*4);
	SetViewportOrgEx(hdc, 0, o, NULL);
	SelectObject(hdc, penLeft);
	Polyline(hdc, pPoints[0], PointsToDisplay2);
}

void CScopeWindow::DrawChannel2()
{
	if(nChannels>1){
		SetViewportOrgEx(hdc, 0, Height/2 + Height*Offset/(MaxOffset*4), NULL);
		SelectObject(hdc, penRight);
		Polyline(hdc, pPoints[1], PointsToDisplay2);
	}
}

void CScopeWindow::draw1()
{
	SetMapMode(hdc, MM_ANISOTROPIC);
	SetWindowOrgEx(hdc, 0, 0, NULL);
	SetWindowExtEx(hdc, PointsToDisplay2, 1<<(MaxBits-1), NULL);
	int m= -Height*Gain/(2*GainNorm);
	if(nChannels>1) m -= m*Offset/(MaxOffset*2);
	SetViewportExtEx(hdc, Width, m, NULL);
	static bool w;
	w=!w;
	if(w){ DrawChannel1(); DrawChannel2(); }
	else{ DrawChannel2(); DrawChannel1(); }
	//restore old mode
	SetMapMode(hdc, MM_TEXT);
	SetViewportOrgEx(hdc, 0, 0, NULL);
}

//redraw the widow (bitmap in memory)
void CScopeWindow::draw(BYTE *start, BYTE *index, BYTE *end)
{
	if(!pPoints[0] || !pPoints[1] && nChannels>1) return;
	//paint the background
	RECT rc;
	SetRect(&rc, 0, 0, Width, Height);
	HBRUSH br = CreateSolidBrush(colors[clBkgnd]);
	FillRect(hdc, &rc, br);
	DeleteObject(br);
	//calculate start position
	//IndexEnd2= SearchForPosZeroCrossing(IndexEnd2-1);
	index -= PointsToDisplay2*nBytesPerSample*nChannels;
	if(int(index-start)<0) index+=end-start;
	//draw the horizontal line
	HGDIOBJ hPenOld = SelectObject(hdc, CreatePen(PS_SOLID, 0, colors[clSeparator]));
	int y = Height / 2;
	MoveToEx(hdc, 0, y, NULL);
	LineTo(hdc, Width, y);
	DeleteObject(SelectObject(hdc, hPenOld));
	//convert source data
	preparePoints(index, PointsToDisplay2, start, end);
	//create pens
	penLeft = CreatePen(PS_SOLID, 0, colors[clLeft]);
	penRight = CreatePen(PS_SOLID, 0, colors[clRight]);
	//draw the wave
	draw1();
	SelectObject(hdc, hPenOld);
	DeleteObject(penLeft);
	DeleteObject(penRight);
	//update info in the property page dialog
	if(hwndDlg && dirty){
		TCHAR buf[40];
		if(nSamplesPerSec){
			_stprintf(buf, TEXT("%.2f ms"), PointsToDisplay2*1000.0/nSamplesPerSec);
			SetDlgItemText(hwndDlg, IDC_WINSIZE, buf);
		}
		_stprintf(buf, TEXT("%.2f x"), Gain/(double)GainNorm);
		SetDlgItemText(hwndDlg, IDC_GAINLABEL, buf);
		_stprintf(buf, TEXT("%d"), Offset);
		SetDlgItemText(hwndDlg, IDC_OFFSETLABEL, buf);
	}
	dirty=false;
}


//redraw visualization window and trackbar captions
void CScopeWindow::invalidate()
{
	PointsToDisplay2= min(Width*Scale/ScaleNorm, nPoints);
	InvalidateRect(visualWnd, 0, FALSE);
	dirty=true;
}

//------------------------------------------------------------------

void CScopeWindow::allocBuffers()
{
	int j;

	delete[] pPoints[0];
	delete[] pPoints[1];

	pPoints[0] = new POINT[nPoints = 6000];
	if(pPoints[0]){
		for(j = 0; j < nPoints; j++)  pPoints[0][j].x = j;
	}

	pPoints[1] = NULL;
	if(nChannels>1){
		pPoints[1] = new POINT[nPoints];
		if(pPoints[1])
			for(j = 0; j < nPoints; j++)  pPoints[1][j].x = j;
	}
}

//filter is connected to the graph, or format changes
void CScopeWindow::setFormat(WAVEFORMATEX *pwf)
{
	bool isFloat1 = pwf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
		pwf->wFormatTag==WAVE_FORMAT_EXTENSIBLE && ((WAVEFORMATEXTENSIBLE *)pwf)->SubFormat == guidFloat;

	if(pPoints[0] && nChannels==pwf->nChannels &&
		nBytesPerSample==pwf->wBitsPerSample/8 &&
		nSamplesPerSec==(int)pwf->nSamplesPerSec &&
		isFloat==isFloat1) return;

	nChannels = pwf->nChannels;
	nBytesPerSample = pwf->wBitsPerSample/8;
	nSamplesPerSec = pwf->nSamplesPerSec;
	isFloat = isFloat1;
	allocBuffers();
	invalidate();
}

//visualization window is resized
void CScopeWindow::setSize(int w, int h, HDC dc)
{
	Width=w;
	Height=h;
	hdc=dc;
	invalidate();
}

//------------------------------------------------------------------

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD, LPVOID)
{
	inst= hModule;
	return TRUE;
}


#define DLL_API __declspec(dllexport)

extern "C"{

	//dialog box procedure (but __cdecl calling convention)
	DLL_API BOOL visPropertyPage(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		static CHOOSECOLOR chc;
		static COLORREF custom[16];
		int cmd;

		CScopeWindow *pScopeWindow = (CScopeWindow*)GetWindowLong(hDlg, GWL_USERDATA);

		switch(uMsg){
			case WM_INITDIALOG:
				pScopeWindow->init(hDlg);
				if(lParam) *(int*)lParam=30;
				return TRUE;

			case WM_COMMAND:
				cmd= LOWORD(wParam);
				if(cmd>=100 && cmd<100+sizeA(colors)){
					chc.lStructSize= sizeof(CHOOSECOLOR);
					chc.hwndOwner= hDlg;
					chc.hInstance= 0;
					chc.rgbResult= colors[cmd-100];
					chc.lpCustColors= custom;
					chc.Flags= CC_RGBINIT|CC_FULLOPEN;
					if(ChooseColor(&chc)){
						colors[cmd-100]=chc.rgbResult;
						InvalidateRect(GetDlgItem(hDlg, cmd), 0, TRUE);
						pScopeWindow->invalidate();
					}
				}
				break;

			case WM_DRAWITEM:
			{
				DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT*)lParam;
				HBRUSH br= CreateSolidBrush(colors[di->CtlID-100]);
				FillRect(di->hDC, &di->rcItem, br);
				DeleteObject(br);
			}
				break;

			case WM_VSCROLL:
				pScopeWindow->ProcessVertScrollCommands(wParam, lParam);
				break;

			case WM_CLOSE:
				pScopeWindow->hwndDlg=0;
				break;
		}
		return FALSE;
	}


	DLL_API void *visInit(HWND visualWnd)
	{
		return new CScopeWindow(visualWnd);
	}

	DLL_API void visSetSize(void *data, int w, int h, HDC dc)
	{
		CScopeWindow *obj = (CScopeWindow*)data;
		obj->setSize(w, h, dc);
	}

	DLL_API void visSetFormat(void *data, WAVEFORMATEX *format)
	{
		CScopeWindow *obj = (CScopeWindow*)data;
		obj->setFormat(format);
	}

	DLL_API void visDraw(void *data, BYTE *start, BYTE *index, BYTE *end)
	{
		CScopeWindow *obj = (CScopeWindow*)data;
		obj->draw(start, index, end);
	}

	DLL_API void visFree(void *data)
	{
		CScopeWindow *obj = (CScopeWindow*)data;
		delete obj;
	}

}
