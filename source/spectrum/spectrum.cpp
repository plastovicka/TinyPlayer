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
#include <math.h>
#include <mmreg.h>

#include "spectrum.h"
#include "resource.h"

#define scaleY 400

HINSTANCE inst;
int Gain = GainNorm;
int Nbars=25;
int isStereo=0;
int barSpeed=10;
int peakSpeed=2;

enum{ clBkgnd, clPeak, clSeparator, clGreen, clYellow, clRed, Ncl };
COLORREF colors[]={0x000000, 0xf09090, 0x00ffff, 0x00ff00, 0x00fff0, 0x0000e0};

#define endA(a) (a+(sizeof(a)/sizeof(*a)))
#define sizeA(A) (sizeof(A)/sizeof(*A))
template <class T> inline void aminmax(T &x, int l, int h){
	if(x<l) x=l;
	if(x>h) x=h;
}

GUID guidFloat ={0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};

//------------------------------------------------------------------

const TCHAR *subkey= TEXT("Software\\Petr Lastovicka\\player\\spectrum");
struct Treg { TCHAR *s; int *i; } regVal[]={
	{TEXT("gain"), &Gain},
	{TEXT("bands"), &Nbars},
	{TEXT("barSpeed"), &barSpeed},
	{TEXT("peakSpeed"), &peakSpeed},
	{TEXT("stereo"), &isStereo},
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

CSpectrumWindow::CSpectrumWindow(HWND visualWnd) :
visualWnd(visualWnd),
		hwndDlg(NULL),
		bmpDC(NULL)
{
	readini();
	aminmax(Gain, MinGain, MaxGain);
	aminmax(Nbars, MinBars, MaxBars);
	aminmax(barSpeed, MinBarFall, MaxBarFall);
	aminmax(peakSpeed, MinPeakFall, MaxPeakFall);
	state = fft_init();
	memset(barData, 0, sizeof(barData));
}

CSpectrumWindow::~CSpectrumWindow()
{
	writeini();
	fft_close(state);
	delBitmap();
}

//property dialog box initialization
void CSpectrumWindow::init(HWND hwndDlg)
{
	this->hwndDlg = hwndDlg;
	hwndGain = GetDlgItem(hwndDlg, IDC_GAIN);
	hwndBands = GetDlgItem(hwndDlg, IDC_BANDS);
	hwndPeakFall = GetDlgItem(hwndDlg, IDC_PEAKFALL);
	hwndBarFall = GetDlgItem(hwndDlg, IDC_BARFALL);

	//set the scroll ranges for the trackbars
	SendMessage(hwndGain, TBM_SETRANGE, TRUE, MAKELONG(MinGain, MaxGain));
	SendMessage(hwndGain, TBM_SETPOS, TRUE, (LPARAM)Gain);
	SendMessage(hwndBands, TBM_SETRANGE, TRUE, MAKELONG(MinBars, MaxBars));
	SendMessage(hwndBands, TBM_SETPOS, TRUE, (LPARAM)Nbars);
	SendMessage(hwndPeakFall, TBM_SETRANGE, TRUE, MAKELONG(MinPeakFall, MaxPeakFall));
	SendMessage(hwndPeakFall, TBM_SETPOS, TRUE, (LPARAM)peakSpeed);
	SendMessage(hwndBarFall, TBM_SETRANGE, TRUE, MAKELONG(MinBarFall, MaxBarFall));
	SendMessage(hwndBarFall, TBM_SETPOS, TRUE, (LPARAM)barSpeed);

	CheckRadioButton(hwndDlg, IDC_MONO, IDC_STEREO, isStereo ? IDC_STEREO : IDC_MONO);
	invalidate();
}


//trackbar message
void CSpectrumWindow::processTrackbar(WPARAM wParam, LPARAM lParam)
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
	else if((HWND)lParam == hwndBands){
		Nbars = pos;
		initBitmap();
	}
	else if((HWND)lParam == hwndPeakFall){
		peakSpeed = pos;
	}
	else if((HWND)lParam == hwndBarFall){
		barSpeed = pos;
	}
	invalidate();
}
//------------------------------------------------------------------
COLORREF mixColor(COLORREF c1, COLORREF c2, double k)
{
	BYTE r, g, b;

	r=(BYTE)(GetRValue(c2)*k+GetRValue(c1)*(1-k));
	g=(BYTE)(GetGValue(c2)*k+GetGValue(c1)*(1-k));
	b=(BYTE)(GetBValue(c2)*k+GetBValue(c1)*(1-k));
	return RGB(r, g, b);
}

//create color bar
void CSpectrumWindow::drawBar(HDC dc, RECT *rc)
{
	int yYG, yRY, y;
	HGDIOBJ oldPen;
	COLORREF c;

	oldPen= GetCurrentObject(dc, OBJ_PEN);
	yYG=int(Height*0.5);
	yRY=int(Height*0.35);
	for(y=rc->top; y<rc->bottom; y++){
		if(y>yYG){
			c=mixColor(colors[clYellow], colors[clGreen], double(y-yYG)/(Height-yYG));
		}
		else if(y>yRY){
			c=colors[clYellow];
		}
		else{
			c=mixColor(colors[clRed], colors[clYellow], double(y)/(yRY));
		}
		DeleteObject(SelectObject(dc, CreatePen(PS_SOLID, 0, c)));
		MoveToEx(dc, rc->left, y, NULL);
		LineTo(dc, rc->right, y);
	}
	DeleteObject(SelectObject(dc, oldPen));
}

//fall down
void CSpectrumWindow::updateBar(int &which, int y, int speed)
{
	int s= drawInterval*speed/30;
	if(s<=0) s=1;
	which -= s;
	if(which<y) which=y;
}

void CSpectrumWindow::DrawChannel(TbarData *dest, SHORT *src, int viewX)
{
	int i, c, h, y, x, oldBar, oldPeak;
	RECT rc;
	float tmp_out[FFT_BUFFER_SIZE/2+1];
	int data[FFT_BUFFER_SIZE/2];
	int xscale[MaxBars+1];

	fft_perform(src, tmp_out, state);
	for(i = 0; i < FFT_BUFFER_SIZE/2; i++){
		data[i] = (int)sqrt(tmp_out[i + 1]);
	}
	for(i = 0; i <= Nbars; i++){
		xscale[i] = (int)(0.5+pow((double)(FFT_BUFFER_SIZE/2), double(i)/Nbars));
		if(i>0 && xscale[i]<=xscale[i-1]) xscale[i]=xscale[i-1]+1;
	}
	HGDIOBJ oldPen= SelectObject(hdc, CreatePen(PS_SOLID, 0, colors[clPeak]));
	HBRUSH br = CreateSolidBrush(colors[clBkgnd]);
	//erase peaks
	x=0;
	for(i = 0; i < Nbars; i++){
		oldBar= dest[i].bar;
		if(dirty) oldBar=0;
		oldPeak=dest[i].peak;
		for(c = xscale[i], y = 0; c < xscale[i + 1]; c++){
			if(data[c] > y)  y = data[c];
		}
		y>>=12;
		if(y>0) y= (int)(log((float)y)*28*Gain/GainNorm*(1.8+double(i)/Nbars));
		updateBar(dest[i].bar, y, barSpeed);
		updateBar(dest[i].peak, y, peakSpeed);

		rc.left=x;
		x=viewX*(i+1)/Nbars;
		rc.right=x-1;
		if(rc.right<=rc.left) rc.right++;
		//erase peak
		rc.bottom= Height-oldPeak*Height/scaleY;
		rc.top= rc.bottom-1;
		FillRect(hdc, &rc, br);
		rc.bottom= Height-oldBar*Height/scaleY;
		rc.top= Height-dest[i].bar*Height/scaleY;
		if(dest[i].bar > oldBar){
			//draw bar
			BitBlt(hdc, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top,
				bmpDC, 0, rc.top, SRCCOPY);
		}
		else{
			//erase bar
			h=rc.top; rc.top=rc.bottom; rc.bottom=h;
			FillRect(hdc, &rc, br);
		}
		//draw peak
		h=Height-dest[i].peak*Height/scaleY-1;
		MoveToEx(hdc, rc.left, h, NULL);
		LineTo(hdc, rc.right, h);
	}
	DeleteObject(br);
	DeleteObject(SelectObject(hdc, oldPen));
}

//copy samples from pBuffer to srcBuffer
void CSpectrumWindow::prepareSrc(BYTE *pBuffer, BYTE *pBeg, BYTE *pEnd)
{
	int i, c;
	const int len=FFT_BUFFER_SIZE;

	if(nBytesPerSample>2 || nChannels>2){
		BYTE *p = pBeg+nBytesPerSample;
		for(i=0; i<len; i++){
			for(c=0; c<nChannels; c++){
				if(c<2){
					srcBuffer[c][i]=
						isFloat ? (SHORT)(((float*)p)[-1] * (1<<15)) :
						(SHORT)((((signed char)p[-1])<<8)+p[-2]);
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
				srcBuffer[0][i] = *p++;
				srcBuffer[1][i] = *p++;
			}
		}
		else{
			for(i=0; i<len; i++){
				if((BYTE*)p==pEnd) p=(SHORT*)pBuffer;
				srcBuffer[0][i] = *p++;
			}
		}
	}
	else{ //nBytesPerSample==1
		BYTE *p = pBeg;
		for(i=0; i<len; i++){
			if((BYTE*)p==pEnd) p=pBuffer;
			srcBuffer[0][i] = (SHORT)((*p++ - 128)<<8);
			if(nChannels>1) srcBuffer[1][i] = (SHORT)((*p++ - 128)<<8);
		}
	}
}

//redraw the widow (bitmap in memory)
void CSpectrumWindow::draw(BYTE *start, BYTE *index, BYTE *end)
{
	DWORD t=GetTickCount();
	drawInterval=t-lastTick;
	lastTick=t;
	if(dirty){
		//paint the background
		RECT rc;
		SetRect(&rc, 0, 0, Width, Height);
		HBRUSH br = CreateSolidBrush(colors[clBkgnd]);
		FillRect(hdc, &rc, br);
		DeleteObject(br);
	}
	//calculate start position
	index -= FFT_BUFFER_SIZE*nBytesPerSample*nChannels;
	if(int(index-start)<0) index+=end-start;
	//convert samples to 16-bit
	prepareSrc(start, index, end);
	//draw bands
	if(isStereo && nChannels>1){
		DrawChannel(barData[0], srcBuffer[0], Width/2);
		SetViewportOrgEx(hdc, Width/2+1, 0, NULL);
		DrawChannel(barData[1], srcBuffer[1], Width/2);
		SetViewportOrgEx(hdc, 0, 0, NULL);
		//draw separator between left and right channels
		HGDIOBJ oldPen= SelectObject(hdc, CreatePen(PS_SOLID, 0, colors[clSeparator]));
		MoveToEx(hdc, Width/2, 0, NULL);
		LineTo(hdc, Width/2, Height);
		DeleteObject(SelectObject(hdc, oldPen));
	}
	else{
		if(nChannels>1){
			//convert from stereo to mono
			for(int i=0; i<FFT_BUFFER_SIZE; i++){
				srcBuffer[0][i]= (SHORT)((srcBuffer[0][i]+srcBuffer[1][i])>>1);
			}
		}
		DrawChannel(barData[0], srcBuffer[0], Width);
	}
	//update info in the property page dialog
	if(hwndDlg && dirty){
		TCHAR buf[40];
		_stprintf(buf, TEXT("%.2f x"), Gain/(double)GainNorm);
		SetDlgItemText(hwndDlg, IDC_GAINLABEL, buf);
		_stprintf(buf, TEXT("%d"), Nbars);
		SetDlgItemText(hwndDlg, IDC_BANDSLABEL, buf);
	}
	dirty=false;
}


void CSpectrumWindow::delBitmap()
{
	if(bmpDC){
		DeleteObject(SelectObject(bmpDC, oldBitmap));
		DeleteDC(bmpDC);
		bmpDC=0;
	}
}

//create bitmap for bar
void CSpectrumWindow::initBitmap()
{
	RECT rc;
	delBitmap();
	bmpDC = CreateCompatibleDC(hdc);
	rc.right=Width/Nbars;
	rc.bottom=Height;
	colorBitmap= CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
	oldBitmap = SelectObject(bmpDC, colorBitmap);
	rc.left=rc.top=0;
	drawBar(bmpDC, &rc);
}

//redraw visualization window and trackbar captions
void CSpectrumWindow::invalidate()
{
	InvalidateRect(visualWnd, 0, FALSE);
	dirty=true;
}

//------------------------------------------------------------------

//filter is connected to the graph, or format changes
void CSpectrumWindow::setFormat(WAVEFORMATEX *pwf)
{
	nChannels = pwf->nChannels;
	nBytesPerSample = pwf->wBitsPerSample/8;
	isFloat = pwf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
		pwf->wFormatTag==WAVE_FORMAT_EXTENSIBLE && ((WAVEFORMATEXTENSIBLE *)pwf)->SubFormat == guidFloat;
	invalidate();
}

//visualization window is resized
void CSpectrumWindow::setSize(int w, int h, HDC dc)
{
	Width=w;
	Height=h;
	hdc=dc;
	initBitmap();
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

		CSpectrumWindow *pScopeWindow = (CSpectrumWindow*)GetWindowLong(hDlg, GWL_USERDATA);

		switch(uMsg){
			case WM_INITDIALOG:
				pScopeWindow->init(hDlg);
				*(int*)lParam=31; //caption string id
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
						pScopeWindow->initBitmap();
						pScopeWindow->invalidate();
					}
				}
				if(cmd==IDC_MONO){
					isStereo=0;
					pScopeWindow->invalidate();
				}
				if(cmd==IDC_STEREO){
					isStereo=1;
					pScopeWindow->invalidate();
				}
				break;

			case WM_DRAWITEM:
			{
				//draw color square
				DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT*)lParam;
				HBRUSH br= CreateSolidBrush(colors[di->CtlID-100]);
				FillRect(di->hDC, &di->rcItem, br);
				DeleteObject(br);
			}
				break;

			case WM_VSCROLL:
			case WM_HSCROLL:
				pScopeWindow->processTrackbar(wParam, lParam);
				break;

			case WM_CLOSE:
				pScopeWindow->hwndDlg=0;
				break;
		}
		return FALSE;
	}


	DLL_API void *visInit(HWND visualWnd)
	{
		return new CSpectrumWindow(visualWnd);
	}

	DLL_API void visSetSize(void *data, int w, int h, HDC dc)
	{
		CSpectrumWindow *obj = (CSpectrumWindow*)data;
		obj->setSize(w, h, dc);
	}

	DLL_API void visSetFormat(void *data, WAVEFORMATEX *format)
	{
		CSpectrumWindow *obj = (CSpectrumWindow*)data;
		obj->setFormat(format);
	}

	DLL_API void visDraw(void *data, BYTE *start, BYTE *index, BYTE *end)
	{
		CSpectrumWindow *obj = (CSpectrumWindow*)data;
		obj->draw(start, index, end);
	}

	DLL_API void visFree(void *data)
	{
		CSpectrumWindow *obj = (CSpectrumWindow*)data;
		delete obj;
	}

}
