/*
	(C) 2004-2012  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include <commdlg.h>
#include <time.h>
#include <shlobj.h>
#include <stdlib.h>
#include <mmreg.h>

#include "assocext.h"
#include "tinyplay.h"
#include "draw.h"         
#include "xml.h"
#include "cd.h"
#include "dvd.h"

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"wsock32.lib")
#pragma comment(lib,"version.lib")
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"quartz.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"strmiids.lib")

//------------------------------------------------------------------

COLORREF colors[]={0x000000, 0xeaf7fb, 0xffffff, 0xff0000, 0x000000, 0x00ff00,
0x892cb1, 0xc0c0c0, 0x000000, 0xf0f0f0, 0x70e8e8, 0x000010, 0x000000};

char *toolNames[]={
	"Previous", "Play", "Pause", "Stop", "Next",
	"", "Open files", "Open folder", "Remove all",
	"", "Options", "Visualization",
	"Full screen", "Step", "Jump forward",
	"Jump backward", "Save playlist", "Font",
	"Zoom in", "Zoom out", "File info",
	"Play CD", "Open URL", "Equalizer", "Play DVD"
};

const int Ntool=12;
TBBUTTON tbb[]={
	{0, ID_FILE_PREVIOUS, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{1, ID_FILE_PLAY, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{2, ID_FILE_PAUSE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{3, ID_FILE_STOP, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{4, ID_FILE_NEXT, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{0, 0, 0, TBSTYLE_SEP, {0}, 0},
	{5, ID_FILE_OPENCLIP, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{6, ID_OPENDIR, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{8, ID_PLAYLIST_REMOVEALL, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{0, 0, 0, TBSTYLE_SEP, {0}, 0},
	{7, ID_OPTIONS, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{17, ID_VISUAL, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{9, ID_FILE_FULLSCREEN, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{10, ID_SINGLE_STEP, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{11, ID_CONTROL_JUMPFORWARD, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{12, ID_CONTROL_JUMPBACKWARD, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{13, ID_PLAYLIST_SAVE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{14, ID_FILE_FONT, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{15, ID_ZOOMIN, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{16, ID_ZOOMOUT, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{18, ID_FILEINFO, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{19, ID_PLAYAUDIOCD, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{20, ID_OPENURL, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{21, ID_EQUALIZER, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
	{22, ID_DVD_FILE_OPENDVD, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0},
};

const DWORD priorCnst[]={IDLE_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS,
HIGH_PRIORITY_CLASS, REALTIME_PRIORITY_CLASS, 16384, 32768};
char *priorTab[]={"idle", "normal", "high", "realtime", "below normal", "above normal"};
char *threadPriorTab[]={"idle", "lowest", "below normal", "normal", "above normal"};

char *cdSpeedStrTab[]={"1 x", "2 x", "4 x", "8 x", "12 x", "16 x", "24 x", "32 x", "40 x", "max"};
char *cdMethodTab[]={"MMC", "MMC2", "MMC3", "MMC4", "Read10", "NEC", "SONY", "D4_12", "D5_10", "A8_12"};


const int
toolH=25,
 volumeH=23,
 volumeW=100,
 timeLGap=7,
 SUBTDELTA=5000000,
 SUBTMOVE=3,
 loadAdvanceTime=8000,
 argMagic=449529714;

int
visualTimer=50,
 visualPriority=1,
 volumeType=2,
 soundConversion=ID_SOUND_STEREO,
 unixCDDB=0,
 CDDBenc=1,
 autoConnect=0,
 subtOutLine=1,   //pen width 
 subtPosWin=40,   //subtitle y position in window
 subtPosFull=100, //subtitle y position in fullscreen
 subtMaxDelay=10, //maximum visibility time of one subtitle
 subtOpaque=0,    //draw background
 repeat=0,
 randomPlay=0,
 moveSnap=15,
 startFullScreen=1,
 visualStartFullScreen=0,
 startForeground=1,
 sortOnLoad=1,
 timeRemain=0,
 winOnFullDelay=2000,
 noHideWinOnFull=0,
 convertUnderscores=1,
 keepRatio=1,
 notMpgOverlay=0,
 jumpSec=10,
 priority=1,
 tick=240,
 videoW=352,
 visualW=200, visualH=150,
 mainLeft=90, mainTop=20, mainW=447, mainH=280,
 videoLeft=90+50, videoTop=20+280,
 visualLeft=90+447, visualTop=40,
 visualDialogX=90+447, visualDialogY=40+200,
 audioDialogX=90+447, audioDialogY=40+180,
 ddColorDialogX, ddColorDialogY,
 eqX=90, eqY=20+280,
 isVisual=1,
 useOverlayMixer=2,
 subtAutoLoad=1,
 gapless=0,
 cdUseOrder=1,
 cdTextOn=1,
 cdda=1,
 cdSpeed=3,
 cdMethod=0,
 cdBigEndian=0,
 cdSwapLR=0,
 cdReadSectors=10,
 crossfade=0,
 smoothPause=1, smoothStop=0, smoothSeek=0,
 audioFilter=1,
 multInst=0,
 noMsg=0,
 regExtAtStartup=0,
 audioSaver=TRUE,
 timeH,
 sortedCol=0,
 descending=1,
 state=ID_FILE_STOP,
 lastPos, lastTime,
 defaultSpeed=1000,
 fullscreen,
 fullMoves=0,
 currentFile=-1,
 forceOrigRatio=0,
 videoWidth, videoHeight,
 videoFrameLeft, videoFrameTop,
 scrW, scrH,
 resizing=0,
 width,
 optionsPage=0,
 autoInfo=0,
 iso88591=0,
 numAdded,
 noSort,
 Ncolumns,
 lastStartPos,
 Naccel,
 hideMouse=0,
 timeL=39+timeLGap;

bool
isVideo=false,
 winOnFull=false,
 stopAfterCurrent,
 delreg=false,
 seeking;

TCHAR extensions[EXTENSIONS_LEN]=_T("ogg;mp3;mpg;mpeg;m2v;avi;vob;wma;wav;divx;wmv;mov;ogm;asf;ape;mpc;m4a;aac;mid;s3m;dat;cda;ifo;m3u;asx");
TCHAR associated[EXTENSIONS_LEN];
TCHAR audioExt[EXTENSIONS_LEN]=_T("mp3;wma;wav;ape;m4a;aac;mid;m3u;wpl;asx");
TCHAR subtExt[SUBTEXT_LEN];
TCHAR listExt[LISTEXT_LEN];
TCHAR outRender[128];
TCHAR subtFolder[256]; //folder for subtitles 
TCHAR savedList[MAX_PATH];
TCHAR url[256];
TCHAR servername[128];
TCHAR serverCgi[128];
TCHAR serverSubmitCgi[128];
TCHAR email[256];
TCHAR *visualNames[MAXVISUALS];
TCHAR *winTitle=_T("Tiny Player");
TCHAR *gaplessFile=0;
TCHAR openDirFolder[MAX_PATH];
TCHAR columnStr[512];
TCHAR fmtTitle[128];

LONGLONG position, seekPos;
double frameRate, playbackRate=1;
long ratio1, ratio2, origW, origH;
static RECT videoSnap1, videoSnap2;
TsetExeState setThreadExecutionState;
Darray<Titem> playlist;

short colWidth[Mcolumns]={30, 240, 45, 56, 60, 300, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};

HWND hWin, dlgWin, videoWnd, visualWnd, listbox, header, toolbar, trackbar, audioDialog, DDColorWnd;
WNDPROC listWndProc;
HACCEL haccel;
ACCEL accel[Maccel];
ACCEL dlgKey;
HINSTANCE inst;
HFONT hFont=0;
LOGFONT font;
HBRUSH listBrush, bkgndBrush;
HMIXEROBJ hMixer;
CRITICAL_SECTION infoCritSect;

// DirectShow interfaces
IGraphBuilder *pGB;
IMediaControl *pMC;
IMediaEventEx *pME;
IVideoWindow  *pVW;
IBasicVideo   *pBV;
IBasicAudio   *pBA;
IMediaSeeking *pMS;
IMediaPosition *pMP;
IGraphBuilder *pNextGB;
IMixerPinConfig2 *pColor;
VisualizationFilter *visualFilter;

const TCHAR *subkey=_T("Software\\Petr Lastovicka\\player");
struct Treg { char *s; int *i; } regVal[]={
	{"full screen", &startFullScreen},
	{"time interval", &tick},
	{"remaining", &timeRemain},
	{"priority", &priority},
	{"jump", &jumpSec},
	{"start foreground", &startForeground},
	{"repeat", &repeat},
	{"random", &randomPlay},
	{"sort", &sortOnLoad},
	{"video top", &videoTop},
	{"video left", &videoLeft},
	{"videoWidth", &videoW},
	{"top", &mainTop},
	{"left", &mainLeft},
	{"width", &mainW},
	{"height", &mainH},
	{"noUnderscore", &convertUnderscores},
	{"keep ratio", &keepRatio},
	{"keepFullRatio", &forceOrigRatio},
	{"current", &currentFile},
	{"visualX", &visualLeft},
	{"visualY", &visualTop},
	{"visualW", &visualW},
	{"visualH", &visualH},
	{"visualTimer", &visualTimer},
	{"visualOn", &isVisual},
	{"visualDialogX", &visualDialogX},
	{"visualDialogY", &visualDialogY},
	{"subtitlePen", &subtOutLine},
	{"subtitlePosFull", &subtPosFull},
	{"subtitlePosWin", &subtPosWin},
	{"subtitleMaxDelay", &subtMaxDelay},
	{"subtitleOutLine", &subtOutLine},
	{"subtitleAutoLoad", &subtAutoLoad},
	{"subtitleOpaque", &subtOpaque},
	{"useOverlayMixer", &useOverlayMixer},
	{"optionsPage", &optionsPage},
	{"overlap", &crossfade},
	{"gapless", &gapless},
	{"volumeType", &volumeType},
	{"regExt", &regExtAtStartup},
	{"autoInfo", &autoInfo},
	{"visualFullScreen", &visualStartFullScreen},
	{"screenSaver", &audioSaver},
	{"smoothPause", &smoothPause},
	{"smoothStop", &smoothStop},
	{"smoothSeek", &smoothSeek},
	{"cdUseOrder", &cdUseOrder},
	{"CDDBformat", &unixCDDB},
	{"CDDBencoding", &CDDBenc},
	{"freedbPort", &port},
	{"proxyPort", &proxyPort},
	{"useProxy", &useProxy},
	{"autoConnectFreedb", &autoConnect},
	{"iTimeOut", &inetTimeout},
	{"speed", &defaultSpeed},
	{"audioFilter", &audioFilter},
	{"dsBuffer", &dsBufferSize},
	{"dsPreBuffer", &dsPreBuffer},
	{"dsUseNotify", &dsUseNotif},
	{"dsNnotif", &dsNnotif},
	{"dsSleep", &dsSleep},
	{"dsSeekDelay", &dsSeekDelay},
	{"visualPriority", &visualPriority},
	{"audioX", &audioDialogX},
	{"audioY", &audioDialogY},
	{"eqX", &eqX},
	{"eqY", &eqY},
	{"eqVisible", &eqVisible},
	{"cdSpeed", &cdSpeed},
	{"cdMethod", &cdMethod},
	{"cdBigEndian", &cdBigEndian},
	{"cdSwapLR", &cdSwapLR},
	{"cdReadSectors", &cdReadSectors},
	{"cdEnableCDTEXT", &cdTextOn},
	{"cdDigital", &cdda},
	{"iso88591", &iso88591},
	{"aviOvrlay", &notMpgOverlay},
	{"hideMouse", &hideMouse},
	{"dsEqualizer", &dsEq},
	{"cdPeriodicSpeed", &cdPeriodicSpeed},
	{"cdSpeedPeriod", &cdSpeedPeriod},
	{"dsCheckUnderrun", &dsCheckUnderrun},
};

struct Tregs { char *s; TCHAR *i; DWORD n; } regValS[]={
	{"language", lang, sizeof(lang)},
	{"ext", extensions, sizeof(extensions)},
	{"assoc", associated, sizeof(associated)},
	{"subtitleExt", subtExt, sizeof(subtExt)},
	{"audioDevice", outRender, sizeof(outRender)},
	{"subtitleFolder", subtFolder, sizeof(subtFolder)},
	{"url", url, sizeof(url)},
	{"visualDLL", visualDLL, sizeof(visualDLL)},
 // {"playlistExt",listExt,sizeof(listExt)},
	{"title", fmtTitle, sizeof(fmtTitle)},
	{"CDDBfolder", cddbFolder, sizeof(cddbFolder)},
	{"freedbServer", servername, sizeof(servername)},
	{"freedbPath", serverCgi, sizeof(serverCgi)},
	{"iml", email, sizeof(email)},
	{"proxy", proxy, sizeof(proxy)},
	{"proxyUser", proxyUser, sizeof(proxyUser)},
	{"ppx", proxyPassword, sizeof(proxyPassword)},
	{"eqFreq", eqFreqStr, sizeof(eqFreqStr)},
	{"openDir", openDirFolder, sizeof(openDirFolder)},
	{"columns", columnStr, sizeof(columnStr)},
	{"autosaveM3U", savedList, sizeof(savedList)},
};

struct Tregb { char *s; void *i; DWORD n; } regValB[]={
	{"keys", accel, sizeof(accel)}, //must be the first item of regValB
#ifdef UNICODE
	{"font", &font, sizeof(LOGFONT)},
	{"subtFont", &subtFont, sizeof(LOGFONT)},
#else
	{"fontA",&font,sizeof(LOGFONT)},
	{"subtFontA",&subtFont,sizeof(LOGFONT)},
#endif
	{"colors", colors, sizeof(colors)},
	{"colWidth", colWidth, sizeof(colWidth)},
};
//------------------------------------------------------------------

//append formatted text to a buffer
int dtprintf(Darray<WCHAR> &buf, WCHAR *fmt, ...)
{
	int n, len;
	va_list v;
	va_start(v, fmt);
	if(buf.len>0 && buf[buf.len-1]==0) buf--;
	for(len=max(buf.capacity-buf.len, 128);; len*=2){
		n=_vsnwprintf(buf+=len, len, fmt, v);
		if(n>=0 && n<len){
			buf -= len-n;
			break; //OK
		}
		buf-=len;
		if(len>10000000) break; //error
	}
	*buf++=0;
	va_end(v);
	return n;
}

int dtprintf(Darray<char> &buf, char *fmt, ...)
{
	int n, len;
	va_list v;
	va_start(v, fmt);
	if(buf.len>0 && buf[buf.len-1]==0) buf--;
	for(len=max(buf.capacity-buf.len, 128);; len*=2){
		n=_vsnprintf(buf+=len, len, fmt, v);
		if(n>=0 && n<len){
			buf -= len-n;
			break; //OK
		}
		buf-=len;
		if(len>10000000) break; //error
	}
	*buf++=0;
	va_end(v);
	return n;
}


void noTopMost()
{
	noHideWinOnFull++;
	//if(isVideo && !pDDXM && pVW) pVW->put_FullScreenMode(OAFALSE);
	SetWindowPos(videoWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE);
	SetWindowPos(visualWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE|SWP_ASYNCWINDOWPOS);
}

//show message box
int vmsg(int id, char *text, int btn, va_list v)
{
	if(noMsg && btn==MB_OK) return -1;
	static const int Mbuf=4096;
	TCHAR *buf=(TCHAR*)_alloca(2*Mbuf);
	convertA2T(text, textT);
	_vsntprintf(buf, Mbuf, lng(id, textT), v);
	buf[Mbuf-1]=0;
	noTopMost();
	noMsg++;
	int result=MessageBox(dlgWin ? dlgWin : hWin, buf, winTitle, btn);
	noMsg--;
	noHideWinOnFull--;
	return result;
}

int msglng1(int btn, int id, char *text, ...)
{
	va_list ap;
	va_start(ap, text);
	int result = vmsg(id, text, btn, ap);
	va_end(ap);
	return result;
}

void msglng(int id, char *text, ...)
{
	va_list ap;
	va_start(ap, text);
	vmsg(id, text, MB_OK, ap);
	va_end(ap);
}

void msg(char *text, ...)
{
	va_list ap;
	va_start(ap, text);
	vmsg(-1, text, MB_OK, ap);
	va_end(ap);
}

void invalidateVideo()
{
	if(subtFont.lfHeight<0) subtFont.lfHeight=-subtFont.lfHeight;
	InvalidateRect(videoWnd, 0, TRUE);
}

void invalidateTime()
{
	RECT rc;
	rc.top=toolH;
	rc.bottom=rc.top+timeH;
	rc.left=0;
	rc.right=width;
	InvalidateRect(hWin, &rc, TRUE);
}

TCHAR *cutExt(TCHAR *fn)
{
	for(TCHAR *s= fn+lstrlen(fn); s>=fn; s--){
		if(*s=='.') return s;
	}
	return 0;
}

void setPriority()
{
	if(priority<0) priority=0;
	if(priority>=sizeA(priorCnst)) priority=sizeA(priorCnst)-1;
	SetPriorityClass(GetCurrentProcess(), priorCnst[priority]);
}

bool isTop()
{
	return (GetWindowLong(hWin, GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
}

int lParam2pos(LPARAM lParam)
{
	if(currentFile<0) return 3;
	int x= (short)LOWORD(lParam);
	amax(x, width);
	x-=timeL;
	if(x<0){ seekPos=0; return 1; }
	seekPos= playlist[currentFile].length*x/(width-timeL);
	return 0;
}

void printTime(TCHAR *buf, LONGLONG t)
{
	if(!t && !seeking){
		*buf=0;
	}
	else{
		if(t<0){ *buf++='-'; t=-t; }
		int s= int(t/UNITS);
		if(s>=3600){
			_stprintf(buf, _T("%d:%02d:%02d "), s/3600, (s/60)%60, s%60);
		}
		else{
			_stprintf(buf, _T("%d:%02d "), s/60, s%60);
		}
	}
}

LONGLONG scanTime(TCHAR *buf)
{
	LONGLONG t;
	TCHAR *e;

	t=0;
	for(;;){
		t+=_tcstol(buf, &e, 10);
		if(e==buf) break;
		buf=e;
		if(*buf==':'){ t*=60; buf++; }
	}
	return t*UNITS;
}

void hideWinOnFull()
{
	if(noHideWinOnFull) return;
	KillTimer(hWin, 1002);
	winOnFull=false;
	if(fullscreen && (isVideo || isVisual && visualData) && state!=ID_FILE_STOP){
		setMouse();
		SetWindowPos(isVideo ? videoWnd : visualWnd,
			HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
		//if(isVideo && !pDDXM && pVW) pVW->put_FullScreenMode(OATRUE);
	}
}

void getDefaultKeyColor()
{
	DEVMODE dm;
	dm.dmSize = sizeof(dm);
	colorKeyRGB= 0x100010; //nearly black
	if(EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &dm)){
		if(dm.dmBitsPerPel==8) colorKeyRGB= 0xff00ff; //pink
	}
}

//------------------------------------------------------------------
void checkMenuItem(UINT cmd, int check)
{
	CheckMenuItem(GetMenu(hWin), cmd, check ? MF_CHECKED : MF_UNCHECKED);
}

void checkSizeMenu()
{
	UINT nItems[] ={ID_FILE_SIZE_HALF, ID_FILE_SIZE_DOUBLE,
		ID_FILE_SIZE_NORMAL, ID_FILE_SIZE_THREEQUARTER, ID_FILE_SIZE_150};

	UINT cmd=0;
	if(pVW){
		RECT rc;
		GetClientRect(videoWnd, &rc);
		if(rc.right==origW/2) cmd=ID_FILE_SIZE_HALF;
		else if(rc.right==origW*3/4) cmd=ID_FILE_SIZE_THREEQUARTER;
		else if(rc.right==origW) cmd=ID_FILE_SIZE_NORMAL;
		else if(rc.right==origW*3/2) cmd=ID_FILE_SIZE_150;
		else if(rc.right==origW*2) cmd=ID_FILE_SIZE_DOUBLE;
	}
	for(int i=0; i<sizeA(nItems); i++){
		checkMenuItem(nItems[i], cmd == nItems[i]);
	}
	checkMenuItem(ID_FILE_FULLSCREEN, fullscreen);
}

void checkRateMenu()
{
	UINT nItems[] ={ID_RATE_CUSTOM, ID_RATE_HALF, ID_RATE_NORMAL,
		ID_RATE_DOUBLE, ID_RATE_75, ID_RATE_125};

	UINT cmd=ID_RATE_CUSTOM;
	if(playbackRate==0.5) cmd=ID_RATE_HALF;
	else if(playbackRate==1.0) cmd=ID_RATE_NORMAL;
	else if(playbackRate==2.0) cmd=ID_RATE_DOUBLE;
	else if(playbackRate==0.75) cmd=ID_RATE_75;
	else if(playbackRate==1.25) cmd=ID_RATE_125;

	for(int i=0; i<sizeA(nItems); i++){
		bool check = (cmd == nItems[i]);
		SendMessage(toolbar, TB_CHECKBUTTON, nItems[i], MAKELONG(check ? TRUE : FALSE, 0));
		checkMenuItem(nItems[i], check);
	}
}

void checkSoundMenu()
{
	for(int i=ID_SOUND_FIRST; i<=ID_SOUND_LAST; i++){
		bool check = (i==soundConversion);
		checkMenuItem(i, check);
	}
}

void checkMenus()
{
	checkMenuItem(ID_REPEAT, repeat==1);
	checkMenuItem(ID_REPEAT1, repeat==2);
	checkMenuItem(ID_REPEAT_STOP, repeat==3);
	checkMenuItem(ID_RANDOM, randomPlay);
	checkMenuItem(ID_ONTOP, isTop());
	checkMenuItem(ID_VISUAL, isVisual);
	checkMenuItem(ID_SUBTITLES_OFF, subtitles.disabled);
}

//------------------------------------------------------------------

//set volume to value, or get volume if value<0
void controlVolume(int value)
{
	MIXERLINE &ml=*(MIXERLINE*)_alloca(sizeof(MIXERLINE)+sizeof(MIXERCONTROL));
	MIXERCONTROL &mc=*(MIXERCONTROL*)(&ml+1);
	MIXERLINECONTROLS mlc;
	MIXERCONTROLDETAILS mcd;
	ULONG mcu;

	if(!volumeType) return;
	if(!hMixer) mixerOpen((HMIXER*)&hMixer, 0, (DWORD)hWin, 0, CALLBACK_WINDOW);
	ml.cbStruct=sizeof(MIXERLINE);
	ml.dwComponentType= (volumeType==1) ? MIXERLINE_COMPONENTTYPE_DST_SPEAKERS :
		(cdda || currentFile<0 || !isCDA(&playlist[currentFile]) ? MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT : MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC);
	if(mixerGetLineInfo(hMixer, &ml, MIXER_GETLINEINFOF_COMPONENTTYPE)==MMSYSERR_NOERROR){
		mlc.cbStruct=sizeof(MIXERLINECONTROLS);
		mlc.cControls=1;
		mlc.cbmxctrl=sizeof(MIXERCONTROL);
		mlc.pamxctrl=&mc;
		mlc.dwLineID=ml.dwLineID;
		mlc.dwControlType= MIXERCONTROL_CONTROLTYPE_VOLUME;
		if(mixerGetLineControls(hMixer, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE)==MMSYSERR_NOERROR){
			mcd.cbStruct=sizeof(MIXERCONTROLDETAILS);
			mcd.dwControlID=mc.dwControlID;
			mcd.cChannels=1;
			mcd.cMultipleItems=0;
			mcd.cbDetails=4;
			mcd.paDetails=&mcu;
			if(mixerGetControlDetails(hMixer, &mcd, MIXER_GETCONTROLDETAILSF_VALUE)==MMSYSERR_NOERROR){
				int minBound= ((LONG*)&mc.Bounds)[0];
				int maxBound= ((LONG*)&mc.Bounds)[1];
				int rng = maxBound - minBound;
				if(value<0){
					SendMessage(trackbar, TBM_SETPOS, TRUE,
						(rng<1000) ? (mcu*100)/rng : mcu/(rng/100));
				}
				else{
					//change volume
					if(rng<1000) mcu= (value*rng)/100;
					else mcu= value*(rng/100);
					mixerSetControlDetails(hMixer, &mcd, MIXER_SETCONTROLDETAILSF_VALUE);
				}
			}
		}
	}
}

void incrVolume(int incr)
{
	int v= incr + (int)SendMessage(trackbar, TBM_GETPOS, 0, 0);
	aminmax(v, 0, 100);
	controlVolume(v);
}

//------------------------------------------------------------------
void encEmail()
{
	TCHAR *s;
	for(s=email; *s; s++) *s^=25;
	for(s=proxyPassword; *s; s++) *s^=26;
}

LONG regDeleteTree(HKEY hKey, LPCTSTR lpSubKey)
{
	HKEY key;
	DWORD i;
	TCHAR buf[64];
	FILETIME f;

	if(RegOpenKeyEx(hKey, lpSubKey, 0, KEY_ENUMERATE_SUB_KEYS, &key)==ERROR_SUCCESS){
		i=sizeA(buf);
		while(RegEnumKeyEx(key, 0, buf, &i, 0, 0, 0, &f)==ERROR_SUCCESS){
			if(regDeleteTree(key, buf)!=ERROR_SUCCESS) break;
		}
		RegCloseKey(key);
	}
	return RegDeleteKey(hKey, lpSubKey);
}

//delete registry settings
void deleteini()
{
	HKEY key;
	DWORD i;

	if(regDeleteTree(HKEY_CURRENT_USER, subkey)==ERROR_SUCCESS){
		if(RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Petr Lastovicka"), 0, KEY_QUERY_VALUE, &key)==ERROR_SUCCESS){
			i=1;
			RegQueryInfoKey(key, 0, 0, 0, &i, 0, 0, 0, 0, 0, 0, 0);
			RegCloseKey(key);
			if(!i){
				RegDeleteKey(HKEY_CURRENT_USER, _T("Software\\Petr Lastovicka"));
			}
		}
	}
	deleteAssoc();
}

//save settings to the registry 
void writeini1()
{
	HKEY key;
	if(RegCreateKey(HKEY_CURRENT_USER, subkey, &key)!=ERROR_SUCCESS)
		msglng(735, "Cannot write to Windows registry");
	else{
		for(Treg *u=regVal; u<endA(regVal); u++){
			RegSetValueExA(key, u->s, 0, REG_DWORD,
				(BYTE *)u->i, sizeof(int));
		}
		encEmail();
		for(Tregs *v=regValS; v<endA(regValS); v++){
			convertA2T(v->s, t);
			RegSetValueEx(key, t, 0, REG_SZ,
				(BYTE *)v->i, (DWORD) sizeof(TCHAR)*(lstrlen(v->i)+1));
		}
		encEmail();
		regValB[0].n=Naccel*sizeof(ACCEL);
		for(Tregb *w=regValB; w<endA(regValB); w++){
			RegSetValueExA(key, w->s, 0, REG_BINARY, (BYTE *)w->i, w->n);
		}
		if(sites) RegSetValueExA(key, "sites", 0, REG_SZ, (BYTE *)sites, strlen(sites)+1);

		float eqBuf[Mequalizer];
		for(int i=0; i<Nequalizer; i++){
			eqBuf[i]=eqParam[i].value;
		}
		RegSetValueExA(key, "eqValues", 0, REG_BINARY, (BYTE *)eqBuf, sizeof(eqBuf[0])*Nequalizer);

		TBSAVEPARAMS sp;
		sp.hkr=HKEY_CURRENT_USER;
		sp.pszSubKey=subkey;
		sp.pszValueName=_T("toolbar");
		SendMessage(toolbar, TB_SAVERESTORE, TRUE, (LPARAM)&sp);
		RegCloseKey(key);
	}
}

void writeini()
{
	writeini1();
	saveList(savedList);
}

//read settings from the registry
void readini()
{
	HKEY key;
	DWORD d;
	if(RegOpenKeyEx(HKEY_CURRENT_USER, subkey, 0, KEY_QUERY_VALUE, &key)==ERROR_SUCCESS){
		for(Treg *u=regVal; u<endA(regVal); u++){
			d=sizeof(int);
			RegQueryValueExA(key, u->s, 0, 0, (BYTE *)u->i, &d);
		}
		encEmail();
		for(Tregs *v=regValS; v<endA(regValS); v++){
			convertA2T(v->s, t);
			d=v->n;
			RegQueryValueEx(key, t, 0, 0, (BYTE *)v->i, &d);
		}
		encEmail();
		for(Tregb *w=regValB; w<endA(regValB); w++){
			d=w->n;
			if(RegQueryValueExA(key, w->s, 0, 0, (BYTE *)w->i, &d)==ERROR_SUCCESS
				&& w==regValB) Naccel=d/sizeof(ACCEL);
		}
		if(RegQueryValueExA(key, "sites", 0, 0, 0, &d)==ERROR_SUCCESS && d<10000000){
			delete[] sites;
			sites= new char[d];
			sites[0]=0;
			RegQueryValueExA(key, "sites", 0, 0, (BYTE*)sites, &d);
		}
		float eqBuf[Mequalizer];
		d=sizeof(eqBuf);
		if(RegQueryValueExA(key, "eqValues", 0, 0, (BYTE*)eqBuf, &d)==ERROR_SUCCESS){
			for(unsigned i=0; i<d/sizeof(eqBuf[0]); i++){
				eqParam[i].value=eqBuf[i];
			}
		}
		RegCloseKey(key);
	}
	if(!sites){
		sites= dupStr("freedb.freedb.org http 80 /~cddb/cddb.cgi Random freedb server");
	}
	parseEqualizerFreq();
}

//------------------------------------------------------------------
typedef void(*TbrowseDirFunc)(TCHAR*, HWND);

void browseDirFunc(TCHAR *dir, HWND wnd)
{
	SetWindowText(wnd, dir);
}

int CALLBACK browseCallback(HWND wnd, UINT mesg, LPARAM lP, LPARAM data)
{
	if(mesg==BFFM_INITIALIZED){
		SendMessage(wnd, BFFM_SETSELECTION, TRUE, (LPARAM)openDirFolder);
	}
	return 0;
}

void openDirFunc(TCHAR *dir, HWND)
{
	numAdded=0;
	openDir(dir);
	gotoFileN();
}

void browseDir(TbrowseDirFunc f, HWND owner, HWND param)
{
	TCHAR dir[MAX_PATH];
	BROWSEINFO bi;
	bi.hwndOwner= owner;
	bi.pidlRoot=0;
	bi.pszDisplayName= dir;
	bi.lpszTitle= 0;
	bi.ulFlags=BIF_RETURNONLYFSDIRS;
	bi.lpfn= (f==openDirFunc) ? browseCallback : 0;
	bi.iImage=0;
	ITEMIDLIST *iil;
	iil=SHBrowseForFolder(&bi);
	if(iil){
		if(SHGetPathFromIDList(iil, dir)){
			f(dir, param);
		}
		IMalloc *ma;
		SHGetMalloc(&ma);
		ma->Free(iil);
		ma->Release();
	}
}

void openDir()
{
	browseDir(openDirFunc, hWin, 0);
}

void openSubtitles()
{
	static OPENFILENAME ofn;
	TCHAR buf[MAX_PATH];
	TCHAR filter[128];

	// create file filter
	lstrcpy(filter, lng(503, _T("Subtitles")));
	wrFileFilter(filter, subtExt);
	*buf = 0;
	ofn.lpstrInitialDir= 0;
	if(state!=ID_FILE_STOP){
		ofn.lpstrInitialDir= playlist[currentFile].file;
	}

	ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner = hWin;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = sizeA(buf);
	ofn.lpstrDefExt = _T("*.sub");
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_READONLY | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_EXPLORER;
	if(!GetOpenFileName(&ofn)) return;
	subtitles.disabled=false;
	subtitles.load(buf);
	if(!subtitles.file) msglng(786, "Can't load subtitles !\nFile: %s", buf);
	checkMenus();
}
//------------------------------------------------------------------
void setListSize()
{
	RECT rc;
	HD_LAYOUT hdl;
	WINDOWPOS wp;
	GetClientRect(hWin, &rc);
	rc.top=timeH+toolH;
	hdl.prc = &rc;
	hdl.pwpos = &wp;
	Header_Layout(header, &hdl);
	int y= wp.cy+timeH+toolH;
	SetWindowPos(listbox, 0, 0, y, rc.right, rc.bottom-y, SWP_NOZORDER);
	SetWindowPos(header, wp.hwndInsertAfter, 0, timeH+toolH,
		wp.cx, wp.cy, wp.flags|SWP_SHOWWINDOW);
	SetWindowPos(toolbar, 0, 0, 0, rc.right, toolH, SWP_NOZORDER);
	SetWindowPos(trackbar, 0, rc.right-volumeW, toolH-volumeH, volumeW, volumeH, SWP_NOZORDER);
}

void elapsedRemain()
{
	timeRemain=!timeRemain;
	invalidateTime();
}

void repaintTime(HDC hdc)
{
	HDC dc=hdc;
	if(!dc) dc=GetDC(hWin);
	LONGLONG pos= seeking ? seekPos : position;
	LONGLONG len= currentFile<0 ? 0 : playlist[currentFile].length;
	if(pos || seeking){
		int t= int(pos/UNITS);
		if(t!=lastTime){
			lastTime=t;
			TCHAR buf[32];
			buf[0]=' ';
			printTime(buf+1, timeRemain ? pos-len : pos);
			SetTextColor(dc, colors[clTime]);
			SetBkColor(dc, colors[clBkTime]);
			//calculate timeL
			SIZE sz;
			int n=lstrlen(buf);
			if(GetTextExtentPoint32(dc, buf, n, &sz)){
				sz.cx+=timeLGap;
				if(sz.cx!=timeL && (!seeking || sz.cx>timeL)){
					timeL=sz.cx;
					invalidateTime();
				}
			}
			//print time
			TextOut(dc, 0, toolH, buf, n);
		}
	}
	if(len){
		RECT rc;
		rc.right= timeL + int((width-timeL)*pos/len);
		rc.left=lastPos;
		if(rc.left!=rc.right){
			lastPos= rc.right;
			rc.top=toolH+3;
			rc.bottom=rc.top+timeH-6;
			int cl= clProgress;
			if(rc.left>rc.right){
				cl= clBkgnd;
				rc.right=rc.left;
				rc.left=lastPos;
			}
			HBRUSH br= CreateSolidBrush(colors[cl]);
			FillRect(dc, &rc, br);
			DeleteObject(br);
		}
	}
	if(!hdc) ReleaseDC(hWin, dc);
}

void snapWnd(RECT *rc, RECT *w, RECT *d)
{
	int W= rc->right - rc->left;
	if(abs(rc->left-w->left+d->left)<moveSnap){
		d->left+=rc->left-w->left;
		rc->left=w->left;
	}
	else if(d->left){
		rc->left=w->left+d->left;
		d->left=0;
	}
	if(abs(rc->right-w->right+d->right)<moveSnap){
		d->right+=rc->right-w->right;
		rc->left=w->right-W;
	}
	else if(d->right){
		rc->left=w->right-W+d->right;
		d->right=0;
	}
	rc->right= rc->left+W;

	int H= rc->bottom - rc->top;
	if(abs(rc->top-w->top+d->top)<moveSnap){
		d->top+=rc->top-w->top;
		rc->top=w->top;
	}
	else if(d->top){
		rc->top=w->top+d->top;
		d->top=0;
	}
	if(abs(rc->bottom-w->bottom+d->bottom)<moveSnap){
		d->bottom+=rc->bottom-w->bottom;
		rc->top=w->bottom-H;
	}
	else if(d->bottom){
		rc->top=w->bottom-H+d->bottom;
		d->bottom=0;
	}
	rc->bottom= rc->top+H;
}

void snapWnd1(RECT *rc, RECT *d)
{
	RECT w;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &w, 0);
	snapWnd(rc, &w, d);
}

void snapWnd2(RECT *rc, HWND w, RECT *d)
{
	RECT r1, r2;
	GetWindowRect(w, &r1);
	r2.top=r1.bottom;
	r2.bottom=r1.top;
	r2.left=r1.right;
	r2.right=r1.left;
	snapWnd(rc, &r2, d);
}

void stickWnd(RECT &rc, HWND wnd, int &left, int &top)
{
	RECT vrc;
	GetWindowRect(wnd, &vrc);
	int wndW= vrc.right - vrc.left;
	int wndH= vrc.bottom - vrc.top;
	int dx= rc.left - mainLeft;
	int dy= rc.top - mainTop;
	if(top==mainTop+mainH) dy= rc.bottom - mainTop-mainH;
	else if(left==mainLeft+mainW) dx= rc.right - mainLeft-mainW;
	else if(top+wndH!=mainTop && left+wndW!=mainLeft) dx=dy=0;
	left+= dx;
	top += dy;
	SetThreadPriority(paintThread, THREAD_PRIORITY_ABOVE_NORMAL);
	SetWindowPos(wnd, HWND_TOP, vrc.left+dx, vrc.top+dy, 0, 0,
		SWP_NOSIZE|SWP_NOACTIVATE|
		(wnd==videoWnd ? 0 : SWP_NOZORDER));
	visualSetPriority();
}

void onMoved()
{
	if(!IsZoomed(hWin) && !IsIconic(hWin)){
		RECT rc;
		GetWindowRect(hWin, &rc);
		stickWnd(rc, videoWnd, videoLeft, videoTop);
		stickWnd(rc, visualWnd, visualLeft, visualTop);
		mainTop= rc.top;
		mainLeft= rc.left;
		mainW= rc.right-rc.left;
		mainH= rc.bottom-rc.top;
	}
}

//---------------------------------------------------------------------------

void setFont()
{
	HGDIOBJ oldF;
	oldF=hFont;
	hFont=CreateFontIndirect(&font);
	SendMessage(listbox, WM_SETFONT, (WPARAM)hFont, (LPARAM)MAKELPARAM(TRUE, 0));
	SendMessage(header, WM_SETFONT, (WPARAM)hFont, (LPARAM)MAKELPARAM(TRUE, 0));
	DeleteObject(oldF);
	TEXTMETRIC tm;
	HDC dc=GetDC(listbox);
	oldF= SelectObject(dc, hFont);
	GetTextMetrics(dc, &tm);
	SelectObject(dc, oldF);
	ReleaseDC(listbox, dc);
	SendMessage(listbox, LB_SETITEMHEIGHT, 0, MAKELPARAM(tm.tmHeight+1+tm.tmExternalLeading, 0));
}

void accelChanged()
{
	DestroyAcceleratorTable(haccel);
	if(Naccel>0){
		haccel= CreateAcceleratorTable(accel, Naccel);
	}
	else{
		haccel= LoadAccelerators(inst, MAKEINTRESOURCE(IDR_ACCELERATOR1));
		Naccel= CopyAcceleratorTable(haccel, accel, sizeA(accel));
	}
}

void timersChanged()
{
	if(state==ID_FILE_PLAY){
		if(tick) SetTimer(hWin, 1000, tick, NULL);
		else KillTimer(hWin, 1000);
		if(isVisual) SetTimer(visualWnd, 1, visualTimer, 0);
		else KillTimer(visualWnd, 1);
	}
}


void reloadMenu()
{
	//submenu lang id from right to left, submenus from bottom to top
	static int subId[]={
		405, 412, 419, 417, 416, 415, 420, 414, 413, 407, 409, 404, 403, 402, 410, 411, 408, 401, 406};

	loadMenu(hWin, MAKEINTRESOURCE(IDR_MENU), subId);
	menuDVD = GetSubMenu(GetMenu(hWin), MENU_DVD);
	ghMenuLanguageMenu = GetSubMenu(menuDVD, MENU_MenuLanguage);
	ghTitleMenu = GetSubMenu(menuDVD, MENU_Title);
	ghChapterMenu = GetSubMenu(menuDVD, MENU_Chapter);
	ghAudioMenu = GetSubMenu(menuDVD, MENU_Audio);
	ghSubpictureMenu = GetSubMenu(menuDVD, MENU_SubPicture);
	ghAngleMenu = GetSubMenu(menuDVD, MENU_Angle);
	if(!isDVD) RemoveMenu(GetMenu(hWin), MENU_DVD, MF_BYPOSITION);

	checkSizeMenu();
	checkRateMenu();
	checkSoundMenu();
	checkMenus();
	DrawMenuBar(hWin);
	visualMenu();
	if(state==ID_FILE_PLAY) enumFilters();
}

void reloadEqualizer()
{
	if(equalizerWnd){
		SendMessage(equalizerWnd, WM_CLOSE, 0, 0);
		showEqualizer();
	}
}

void langChanged()
{
	reloadMenu();
	initColumns();
	if(visualDialog) visualProperty(0);
	if(audioDialog) audioProperty(0);
	reloadEqualizer();
	changeDialog(DDColorWnd, ddColorDialogX, ddColorDialogY, MAKEINTRESOURCE(IDD_DDCOLOR), (DLGPROC)DDColorProc);
}


void getCmdName(TCHAR *buf, int n, int cmd)
{
	TCHAR *s, *d;

	s=lng(cmd, 0);
	if(s){
		lstrcpyn(buf, s, n);
	}
	else{
		MENUITEMINFO mii;
		mii.cbSize=sizeof(MENUITEMINFO);
		mii.fMask=MIIM_TYPE;
		mii.dwTypeData=buf;
		mii.cch= n;
		GetMenuItemInfo(GetMenu(hWin), cmd, FALSE, &mii);
	}
	for(s=d=buf; *s; s++){
		if(*s=='\t') *s=0;
		if(*s!='&') *d++=*s;
	}
	*d=0;
}

void changeKey(HWND hDlg, UINT vkey)
{
	TCHAR *buf=(TCHAR*)_alloca(2*64);
	BYTE flg;
	ACCEL *a, *e;

	dlgKey.key=(USHORT)vkey;
	//read shift states
	flg=FVIRTKEY|FNOINVERT;
	if(GetKeyState(VK_CONTROL)<0) flg|=FCONTROL;
	if(GetKeyState(VK_SHIFT)<0) flg|=FSHIFT;
	if(GetKeyState(VK_MENU)<0) flg|=FALT;
	dlgKey.fVirt=flg;
	//set hotkey edit control
	printKey(buf, &dlgKey);
	SetWindowText(GetDlgItem(hDlg, IDC_KEY), buf);
	//modify accelerator table
	e=accel+Naccel;
	if(!dlgKey.cmd){
		for(a=accel; a<e; a++){
			if(a->key==vkey && a->fVirt==flg){
				PostMessage(hDlg, WM_COMMAND, a->cmd, 0);
			}
		}
	}
	else{
		for(a=accel;; a++){
			if(a==e){
				*a=dlgKey;
				Naccel++;
				break;
			}
			if(a->cmd==dlgKey.cmd){
				a->fVirt=dlgKey.fVirt;
				a->key=dlgKey.key;
				if(vkey==0){
					memcpy(a, a+1, (e-a-1)*sizeof(ACCEL));
					Naccel--;
				}
				break;
			}
		}
		accelChanged();
		//change the main menu
		reloadMenu();
	}
}

//show or hide mouse cursor
void setMouse()
{
	bool show = !hideMouse && !fullscreen ||
		winOnFull || isDVD && g_bMenuOn;
	if(pVW) pVW->HideCursor(show ? OAFALSE : OATRUE);
	SetCursor(show ? LoadCursor(0, IDC_ARROW) : 0);
}

void mouseMoveFull()
{
	if(fullscreen && (position>UNITS || !tick || state==ID_FILE_PAUSE)
		&& !winOnFull && !noHideWinOnFull && (!isDVD || !g_bMenuOn)){
		POINT p;
		static POINT p1;
		static int tick1;
		GetCursorPos(&p);
		if(p.x!=p1.x || p.y!=p1.y){
			p1=p;
			fullMoves++;
			int tick = GetTickCount();
			if(tick-tick1 > 3000) fullMoves=0;
			tick1=tick;
		}
		if(fullMoves<12) return;
		winOnFull=true;
		noTopMost();
		SetWindowPos(hWin, isVideo ? videoWnd : visualWnd, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
		noHideWinOnFull--;
		SetTimer(hWin, 1002, winOnFullDelay, 0);
		setMouse();
	}
	fullMoves=0;
}

//------------------------------------------------------------------
DWORD getVer()
{
	HRSRC r;
	HGLOBAL h;
	void *s;
	VS_FIXEDFILEINFO *v;
	UINT i;

	r=FindResource(0, (TCHAR*)VS_VERSION_INFO, RT_VERSION);
	h=LoadResource(0, r);
	s=LockResource(h);
	if(!s || !VerQueryValueA(s, "\\", (void**)&v, &i)) return 0;
	return v->dwFileVersionMS;
}

LRESULT CALLBACK AboutDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	char buf[48];
	DWORD d;

	switch(message){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 11);
			d=getVer();
			sprintf(buf, "%d.%d", HIWORD(d), LOWORD(d));
			SetDlgItemTextA(hWnd, 101, buf);
			return TRUE;

		case WM_COMMAND:
			if(wParam==123){
				GetDlgItemTextA(hWnd, wParam, buf, sizeA(buf));
				ShellExecuteA(0, 0, buf, 0, 0, SW_SHOWNORMAL);
			}
			if(wParam == IDOK || wParam == IDCANCEL){
				EndDialog(hWnd, TRUE);
				return TRUE;
			}
			break;
	}
	return FALSE;
}

//------------------------------------------------------------------
void colorChanged()
{
	DeleteObject(listBrush);
	listBrush=CreateSolidBrush(colors[clBkList]);
	HBRUSH oldB= bkgndBrush;
	bkgndBrush=CreateSolidBrush(colors[clBkgnd]);
	SetClassLong(hWin, GCL_HBRBACKGROUND, (LONG)bkgndBrush);
	DeleteObject(oldB);
	InvalidateRect(hWin, 0, TRUE);
	InvalidateRect(listbox, 0, TRUE);
	InvalidateRect(toolbar, 0, TRUE);
	InvalidateRect(videoWnd, 0, TRUE);
}

static COLORREF custom[16];

BOOL CALLBACK ColorProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP)
{
	static bool chng;
	static CHOOSECOLOR chc;
	static COLORREF clold[Ncl];
	DRAWITEMSTRUCT *di;
	HBRUSH br;
	int cmd;

	switch(msg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 23);
			memcpy(clold, colors, sizeof(clold));
			chng=false;
			return TRUE;

		case WM_DRAWITEM:
			di = (DRAWITEMSTRUCT*)lP;
			br= CreateSolidBrush(colors[di->CtlID-100]);
			FillRect(di->hDC, &di->rcItem, br);
			DeleteObject(br);
			break;

		case WM_COMMAND:
			cmd=LOWORD(wP);
			switch(cmd){
				default: //color square
					chc.lStructSize= sizeof(CHOOSECOLOR);
					chc.hwndOwner= hWnd;
					chc.hInstance= 0;
					chc.rgbResult= colors[cmd-100];
					chc.lpCustColors= custom;
					chc.Flags= CC_RGBINIT|CC_FULLOPEN;
					if(ChooseColor(&chc)){
						colors[cmd-100]=chc.rgbResult;
						InvalidateRect(GetDlgItem(hWnd, cmd), 0, TRUE);
						colorChanged();
						chng=true;
					}
					break;
				case IDCANCEL:
					if(chng){
						memcpy(colors, clold, sizeof(clold));
						colorChanged();
					}
					//!
				case IDOK:
					EndDialog(hWnd, cmd);
			}
			break;
	}
	return FALSE;
}

//------------------------------------------------------------------
UINT APIENTRY CFHookProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if(message==WM_COMMAND && LOWORD(wParam)==1026){
		SendMessage(hDlg, WM_CHOOSEFONT_GETLOGFONT, 0, (LPARAM)&font);
		setFont();
		return 1;
	}
	return 0;
}

//------------------------------------------------------------------
UINT APIENTRY CFHookProcSubt(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if(message==WM_COMMAND && LOWORD(wParam)==1026){
		SendMessage(hDlg, WM_CHOOSEFONT_GETLOGFONT, 0, (LPARAM)&subtFont);
		invalidateVideo();
		return 1;
	}
	return 0;
}

//------------------------------------------------------------------
LRESULT CALLBACK UrlDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 18);
			SetDlgItemText(hWnd, IDC_URL, url);
			return TRUE;

		case WM_COMMAND:
			wParam=LOWORD(wParam);
			switch(wParam){
				case IDOK:
					GetDlgItemText(hWnd, IDC_URL, url, sizeA(url));
					addFile(url);
					//!
				case IDCANCEL:
					EndDialog(hWnd, wParam);
					return TRUE;
			}
	}
	return FALSE;
}

//------------------------------------------------------------------
LRESULT CALLBACK JumpDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TCHAR buf[16];

	switch(message){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 22);
			printTime(buf, position);
			SetDlgItemText(hWnd, IDC_JUMP, buf);
			return TRUE;

		case WM_COMMAND:
			wParam=LOWORD(wParam);
			switch(wParam){
				case IDOK:
					GetDlgItemText(hWnd, IDC_JUMP, buf, sizeA(buf));
					if(state == ID_FILE_STOP) gotoFile(currentFile);
					seek(scanTime(buf));
					//!
				case IDCANCEL:
					EndDialog(hWnd, wParam);
					return TRUE;
			}
	}
	return FALSE;
}

//------------------------------------------------------------------
LRESULT CALLBACK SubtTimeDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TCHAR buf[32];

	switch(message){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 17);
			_stprintf(buf, _T("%.3f"), subtitles.scale);
			SetDlgItemText(hWnd, IDC_SUBTSCALE, buf);
			_stprintf(buf, _T("%.1f"), double(subtitles.delta)/UNITS);
			SetDlgItemText(hWnd, IDC_SUBTSHIFT, buf);
			return TRUE;

		case WM_COMMAND:
			wParam=LOWORD(wParam);
			switch(wParam){
				case IDOK:
					GetDlgItemText(hWnd, IDC_SUBTSCALE, buf, sizeA(buf));
					subtitles.scale= _tcstod(buf, 0);
					GetDlgItemText(hWnd, IDC_SUBTSHIFT, buf, sizeA(buf));
					subtitles.delta= (LONGLONG)_tcstod(buf, 0)*UNITS;
					subtitles.reset();
					//!
				case IDCANCEL:
					EndDialog(hWnd, wParam);
					return TRUE;
			}
	}
	return FALSE;
}


//------------------------------------------------------------------
LRESULT CALLBACK RateDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TCHAR buf[32], *s;

	switch(message){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 21);
			_stprintf(buf, _T("%.3f"), playbackRate);
			SetDlgItemText(hWnd, IDC_RATE, buf);
			return TRUE;

		case WM_COMMAND:
			wParam=LOWORD(wParam);
			switch(wParam){
				case IDOK:
					GetDlgItemText(hWnd, IDC_RATE, buf, sizeA(buf));
					for(s=buf; *s; s++){
						if(*s==',') *s='.';
					}
					setRate(_tcstod(buf, 0));
					//!
				case IDCANCEL:
					EndDialog(hWnd, wParam);
					return TRUE;
			}
	}
	return FALSE;
}

//------------------------------------------------------------------
static WNDPROC toolbarWndProc;

LRESULT CALLBACK toolbarClassProc(HWND hWnd, UINT mesg, WPARAM wParam, LPARAM lParam)
{
	if(mesg==WM_HSCROLL){
		if((LOWORD(wParam)==TB_ENDTRACK || LOWORD(wParam)==TB_THUMBTRACK)){
			controlVolume(SendMessage(trackbar, TBM_GETPOS, 0, 0));
			SetFocus(listbox);
		}
	}
	return CallWindowProc((WNDPROC)toolbarWndProc, hWnd, mesg, wParam, lParam);
}


static WNDPROC editWndProc;

//window procedure for hotkey edit control
LRESULT CALLBACK hotKeyClassProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	switch(mesg){
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if(wP!=VK_CONTROL && wP!=VK_SHIFT && wP!=VK_MENU && wP!=VK_RWIN && wP!=VK_LWIN){
				if((wP==VK_BACK || wP==VK_DELETE) && dlgKey.key) wP=0;
				changeKey(GetParent(hWnd), (UINT)wP);
				return 0;
			}
			break;
		case WM_CHAR:
			return 0; //don't write character to the edit control
	}
	return CallWindowProc((WNDPROC)editWndProc, hWnd, mesg, wP, lP);
}

//------------------------------------------------------------------
LRESULT CALLBACK KeysDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static const int Mbuf=64;
	TCHAR *buf=(TCHAR*)_alloca(2*Mbuf);
	int i;

	switch(message){
		case WM_INITDIALOG:
			editWndProc = (WNDPROC)SetWindowLong(GetDlgItem(hWnd, IDC_KEY),
				GWL_WNDPROC, (LONG)hotKeyClassProc);
			setDlgTexts(hWnd, 19);
			dlgKey.cmd=0;
			return TRUE;

		case WM_COMMAND:
			wParam=LOWORD(wParam);
			switch(wParam){
				default:
					//popup menu item
					if(wParam<500 && wParam>=200){
						//set command text
						dlgKey.cmd=(USHORT)wParam;
						getCmdName(buf, Mbuf, wParam);
						SetDlgItemText(hWnd, IDC_KEYCMD, buf);
						HWND hHotKey=GetDlgItem(hWnd, IDC_KEY);
						if(!*buf) dlgKey.cmd=0;
						else SetFocus(hHotKey);
						//change accelerator table
						dlgKey.key=0;
						*buf=0;
						for(ACCEL *a=accel+Naccel-1; a>=accel; a--){
							if(a->cmd==dlgKey.cmd){
								dlgKey.fVirt=a->fVirt;
								dlgKey.key=a->key;
								printKey(buf, &dlgKey);
								break;
							}
						}
						//set hotkey edit control
						SetWindowText(hHotKey, buf);
					}
					break;
				case IDC_MENUDROP:
				{
					RECT rc;
					GetWindowRect(GetDlgItem(hWnd, IDC_MENUDROP), &rc);
					//create popup menu from the main menu
					HMENU hMenu= CreatePopupMenu();
					MENUITEMINFO mii;
					int n= GetMenuItemCount(GetMenu(hWin));
					for(i=0; i<n; i++){
						mii.cbSize=sizeof(mii);
						mii.fMask=MIIM_SUBMENU|MIIM_TYPE;
						mii.cch=Mbuf;
						mii.dwTypeData=buf;
						GetMenuItemInfo(GetMenu(hWin), i, TRUE, &mii);
						InsertMenuItem(hMenu, 0xffffffff, TRUE, &mii);
					}
					//show popup menu
					TrackPopupMenuEx(hMenu,
						TPM_RIGHTBUTTON, rc.left, rc.bottom, hWnd, NULL);
					//delete popup menu
					for(i=0; i<n; i++){
						RemoveMenu(hMenu, 0, MF_BYPOSITION);
					}
					DestroyMenu(hMenu);
					break;
				}
				case IDOK:
				case IDCANCEL:
					EndDialog(hWnd, wParam);
					return TRUE;
			}
	}
	return FALSE;
}

//------------------------------------------------------------------
void refreshTags()
{
	for(int i=0; i<playlist.len; i++){
		Titem *t= &playlist[i];
		t->freeTags();
	}
	invalidateList();
}

void chooseSubtFont(HWND wnd)
{
	static CHOOSEFONT f;
	f.lStructSize=sizeof(CHOOSEFONT);
	f.hwndOwner=wnd;
	f.Flags=CF_SCALABLEONLY|CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT|CF_SCRIPTSONLY|CF_ENABLEHOOK|CF_APPLY;
	f.lpLogFont=&subtFont;
	f.lpfnHook=CFHookProcSubt;
	if(ChooseFont(&f)) invalidateVideo();
}

int getRadioButton(HWND hWnd, int item1, int item2)
{
	for(int i=item1; i<=item2; i++){
		if(IsDlgButtonChecked(hWnd, i)){
			return i-item1;
		}
	}
	return 0;
}

struct TintValue{ int *value; WORD id; short dlgId; };
struct TboolValue{ int *value; WORD id; short dlgId; };
struct TstrValue{ TCHAR *value; int len; WORD id; short dlgId; };
struct TgroupValue{ int *value; WORD first; WORD last; short dlgId; };
struct TcomboValue{ int *value; WORD id; WORD lngId; char **strA; WORD len; short dlgId; };

static TintValue intOpts[]={
	{&tick, IDC_TICK, 1},
	{&jumpSec, IDC_JUMPSEC, 1},
	{&crossfade, IDC_OVERLAP, 3},
	{&dsBufferSize, IDC_DSBUFLEN, 3},
	{&dsPreBuffer, IDC_DSPREBUF, 3},
	{&dsSeekDelay, IDC_DSTRACK_PREBUF, 3},
	{&dsSleep, IDC_DSSLEEP, 3},
	{&dsNnotif, IDC_DSNOTIF_POS, 3},
	{&visualTimer, IDC_VISTIMER, 4},
	{&subtMaxDelay, IDC_SUBTDELAY, 6},
	{&cdReadSectors, IDC_CDSECTORS, 7},
	{&port, IDC_FREEDB_PORT, 9},
	{&proxyPort, IDC_PROXYPORT, 9},
	{&inetTimeout, IDC_FREEDB_TIMEOUT, 9},
	{&cdSpeedPeriod, IDC_CDSPEED_INTERVAL, 7},
};

static TboolValue boolOpts[]={
	{&startForeground, IDC_STARTFOREGROUND, 0},
	{&multInst, IDC_MULTINST, 0},
	{&regExtAtStartup, IDC_REGEXT, 0},
	{&sortOnLoad, IDC_SORTONLOAD, 1},
	{&audioSaver, IDC_SCREENSAVER, 1},
	{&gapless, IDC_GAPLESS, 3},
	{&convertUnderscores, IDC_UNDERSCORES, 2},
	{&autoInfo, IDC_AUTOINFO, 2},
	{&iso88591, IDC_ISO88591, 2},
	{&smoothPause, IDC_FADEOUT_PAUSE, 4},
	{&smoothStop, IDC_FADEOUT_STOP, 4},
	{&smoothSeek, IDC_FADEOUT_SEEK, 4},
	{&startFullScreen, IDC_FULLSCREEN, 5},
	{&keepRatio, IDC_KEEP_RATIO, 5},
	{&forceOrigRatio, IDC_FULLSCREENRATIO, 5},
	{&hideMouse, IDC_HIDE_MOUSE, 5},
	{&subtOutLine, IDC_SUBTOUTLINE, 6},
	{&subtAutoLoad, IDC_SUBTAUTOLOAD, 6},
	{&subtOpaque, IDC_SUBTOPAQUE, 6},
	{&cdTextOn, IDC_CDTEXT_ON, 7},
	{&cdSwapLR, IDC_CDSWAPLR, 7},
	{&cdBigEndian, IDC_CDBIGENDIAN, 7},
	{&cdda, IDC_CDDA, 7},
	{&cdUseOrder, IDC_CDUSEORDER, 8},
	{&useProxy, IDC_USEPROXY, 9},
	{&autoConnect, IDC_AUTO_CONNECT, 9},
	{&dsEq, IDC_ENABLE_EQ, 4},
	{&cdPeriodicSpeed, IDC_SETCDSPEED, 7},
	{&dsCheckUnderrun, IDC_DSCHECK_UNDERRUN, 3},
};

static TstrValue strOpts[]={
	{extensions, sizeA(extensions), IDC_EXTENSIONS, 0},
	{associated, sizeA(associated), IDC_ASSOCIATED, 0},
	{savedList, sizeA(savedList), IDC_SAVEDLIST, 0},
	{openDirFolder, sizeA(openDirFolder), IDC_OPENDIR_FOLDER, 1},
	{columnStr, sizeA(columnStr), IDC_COLUMNS, 2},
	{fmtTitle, sizeA(fmtTitle), IDC_TITLE, 2},
	{eqFreqStr, sizeA(eqFreqStr), IDC_EQ_FREQ, 4},
	{subtFolder, sizeA(subtFolder), IDC_SUBTDIR, 6},
	{subtExt, sizeA(subtExt), IDC_SUBTEXT, 6},
	{cddbFolder, sizeA(cddbFolder), IDC_CDDBFOLDER, 8},
	{servername, sizeA(servername), IDC_FREEDB_SERVER, 9},
	{serverCgi, sizeA(serverCgi), IDC_FREEDB_PATH, 9},
	{email, sizeA(email), IDC_EMAIL, 9},
	{proxy, sizeA(proxy), IDC_PROXY, 9},
	{proxyUser, sizeA(proxyUser), IDC_PROXY_USER, 9},
	{proxyPassword, sizeA(proxyPassword), IDC_PROXY_PASS, 9},
};

static TgroupValue groupOpts[]={
	{&audioFilter, IDC_USEDEVICE, IDC_USEDS, 3},
	{&dsUseNotif, IDC_DSNOTIF0, IDC_DSNOTIF1, 3},
	{&volumeType, IDC_VOLUME0, IDC_VOLUME2, 4},
	{&useOverlayMixer, IDC_USEOVERLAY0, IDC_USEOVERLAY2, 5},
	{&unixCDDB, IDC_CDDBWIN, IDC_CDDBUNIX, 8},
	{&CDDBenc, IDC_CDDBACP, IDC_CDDBUTF8, 8},
};

static TcomboValue comboOpts[]={
	{&priority, IDC_PRIORITYCOMBO, 660, priorTab, sizeA(priorTab), 0},
	{&visualPriority, IDC_VISUALPRIORITYCOMBO, 690, threadPriorTab, sizeA(threadPriorTab), 4},
	{&cdMethod, IDC_CDMETHOD, 9999, cdMethodTab, sizeA(cdMethodTab), 7},
	{&cdSpeed, IDC_CDSPEED, 9999, cdSpeedStrTab, sizeA(cdSpeedStrTab), 7},
};

BOOL propPageInit(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP, int curPage)
{
	int j;
	HWND sheet, combo;
	TintValue *ti;
	TboolValue *tb;
	TstrValue *ts;
	TgroupValue *tg;
	TcomboValue *tc;

	switch(mesg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd);
			sheet=GetParent(hWnd);
			SetWindowLong(sheet, GWL_EXSTYLE, GetWindowLong(sheet, GWL_EXSTYLE)&~WS_EX_CONTEXTHELP);
			for(ti=intOpts; ti<endA(intOpts); ti++){
				if(ti->dlgId==curPage) SetDlgItemInt(hWnd, ti->id, *ti->value, FALSE);
			}
			for(tb=boolOpts; tb<endA(boolOpts); tb++){
				if(tb->dlgId==curPage) CheckDlgButton(hWnd, tb->id, *tb->value ? BST_CHECKED : BST_UNCHECKED);
			}
			for(ts=strOpts; ts<endA(strOpts); ts++){
				if(ts->dlgId==curPage) SetDlgItemText(hWnd, ts->id, ts->value);
			}
			for(tg=groupOpts; tg<endA(groupOpts); tg++){
				if(tg->dlgId==curPage) CheckRadioButton(hWnd, tg->first, tg->last, *tg->value+tg->first);
			}
			for(tc=comboOpts; tc<endA(comboOpts); tc++){
				if(tc->dlgId==curPage){
					combo= GetDlgItem(hWnd, tc->id);
					for(j=0; j<tc->len; j++){
						convertA2T(tc->strA[j], t);
						SendMessage(combo, CB_ADDSTRING, 0,
							(LPARAM)lng(tc->lngId+j, t));
					}
					SendMessage(combo, CB_SETCURSEL, *tc->value, 0);
				}
			}
			return TRUE;
		case WM_COMMAND:
			if(HIWORD(wP)==EN_CHANGE || HIWORD(wP)==BN_CLICKED || HIWORD(wP)==CBN_SELCHANGE){
				PropSheet_Changed(GetParent(hWnd), hWnd);
			}
			break;
		case WM_NOTIFY:
			switch(((NMHDR*)lP)->code){
				case PSN_APPLY:
					for(ti=intOpts; ti<endA(intOpts); ti++){
						if(ti->dlgId==curPage) *ti->value= GetDlgItemInt(hWnd, ti->id, NULL, FALSE);
					}
					for(tb=boolOpts; tb<endA(boolOpts); tb++){
						if(tb->dlgId==curPage) *tb->value= IsDlgButtonChecked(hWnd, tb->id);
					}
					for(ts=strOpts; ts<endA(strOpts); ts++){
						if(ts->dlgId==curPage) GetDlgItemText(hWnd, ts->id, ts->value, ts->len);
					}
					for(tg=groupOpts; tg<endA(groupOpts); tg++){
						if(tg->dlgId==curPage) *tg->value= getRadioButton(hWnd, tg->first, tg->last);
					}
					for(tc=comboOpts; tc<endA(comboOpts); tc++){
						if(tc->dlgId==curPage) *tc->value= (int)SendMessage(GetDlgItem(hWnd, tc->id), CB_GETCURSEL, 0, 0);
					}
					SetWindowLong(hWnd, DWL_MSGRESULT, FALSE);
					return TRUE;
				case PSN_SETACTIVE:
					optionsPage=curPage;
					break;
			}
			break;
	}
	return FALSE;
}


BOOL CALLBACK optionsStartupProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	HKEY key;
	TCHAR *oldAssoc= (TCHAR*)_alloca(2*EXTENSIONS_LEN);
	lstrcpy(oldAssoc, associated);
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 0);
	switch(mesg){
		case WM_NOTIFY:
			switch(((NMHDR*)lP)->code){
				case PSN_APPLY:
					setPriority();
					associateFiles(associated, oldAssoc);
					if(RegCreateKey(HKEY_CURRENT_USER, subkey, &key)==ERROR_SUCCESS){
						RegSetValueEx(key, _T("multInst"), 0, REG_DWORD, (BYTE *)&multInst, sizeof(int));
						RegCloseKey(key);
					}
					break;
			}
			break;
	}
	return result;
}

BOOL CALLBACK optionsGeneralProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 1);
	switch(mesg){
		case WM_COMMAND:
			if(LOWORD(wP)==IDC_BROWSE){
				browseDir(browseDirFunc, hWnd, GetDlgItem(hWnd, IDC_OPENDIR_FOLDER));
			}
			break;
		case WM_NOTIFY:
			switch(((NMHDR*)lP)->code){
				case PSN_APPLY:
					timersChanged();
					if(state!=ID_FILE_STOP && !isVideo){
						SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, screenSaver1=audioSaver, 0, 0);
					}
					break;
			}
			break;
	}
	return result;
}

BOOL CALLBACK optionsPlaylistProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	TCHAR *oldCol= (TCHAR*)_alloca(sizeof(columnStr));
	_tcscpy(oldCol, columnStr);
	int oldIso= iso88591;
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 2);
	switch(mesg){
		case WM_NOTIFY:
			switch(((NMHDR*)lP)->code){
				case PSN_APPLY:
					if(_tcscmp(oldCol, columnStr)) initColumns();
					if(oldIso!=iso88591) refreshTags();
					invalidateList();
					break;
			}
			break;
	}
	return result;
}

BOOL CALLBACK optionsAudio1Proc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	HWND combo= GetDlgItem(hWnd, IDC_AUDIO_OUT);
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 3);
	switch(mesg){
		case WM_INITDIALOG:
			enumAudioRenderers(1, (void*)combo);
			break;
		case WM_NOTIFY:
			switch(((NMHDR*)lP)->code){
				case PSN_APPLY:
					SendMessage(combo, CB_GETLBTEXT, SendMessage(combo, CB_GETCURSEL, 0, 0), (LPARAM)outRender);
					break;
			}
			break;
	}
	return result;
}

BOOL CALLBACK optionsAudio2Proc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	TCHAR *eqOld=(TCHAR*)_alloca(sizeof(eqFreqStr));
	_tcscpy(eqOld, eqFreqStr);
	int volumeOld=volumeType;
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 4);
	switch(mesg){
		case WM_NOTIFY:
			switch(((NMHDR*)lP)->code){
				case PSN_APPLY:
					if(_tcscmp(eqOld, eqFreqStr)){
						parseEqualizerFreq();
						reloadEqualizer();
					}
					visualSetPriority();
					timersChanged();
					if(volumeType!=volumeOld){
						setListSize();
						ShowWindow(trackbar, volumeType ? SW_SHOW : SW_HIDE);
						controlVolume(-1);
					}
					break;
			}
			break;
	}
	return result;
}

BOOL CALLBACK optionsVideoProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	int overlayOld=useOverlayMixer;
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 5);
	switch(mesg){
		case WM_NOTIFY:
			switch(((NMHDR*)lP)->code){
				case PSN_APPLY:
					if(overlayOld!=useOverlayMixer && useOverlayMixer<2) AddOvMToGraph();
					if(pVW) setMouse();
					break;
			}
			break;
	}
	return result;
}

BOOL CALLBACK optionsSubtitlesProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 6);
	switch(mesg){
		case WM_COMMAND:
			if(LOWORD(wP)==IDC_SUBTCOLOR){
				DialogBox(inst, MAKEINTRESOURCE(IDD_COLORS), hWnd, (DLGPROC)ColorProc);
			}
			if(LOWORD(wP)==IDC_SUBTFONT){
				chooseSubtFont(hWnd);
			}
			if(LOWORD(wP)==IDC_BROWSE){
				browseDir(browseDirFunc, hWnd, GetDlgItem(hWnd, IDC_SUBTDIR));
			}
			break;
		case WM_NOTIFY:
			switch(((NMHDR*)lP)->code){
				case PSN_APPLY:
					timersChanged();
					subtitles.calcMaxDelay();
					invalidateVideo();
					break;
			}
			break;
	}
	return result;
}

BOOL CALLBACK optionsCDProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	int oldSpeed=cdSpeed;
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 7);
	switch(mesg){
		case WM_NOTIFY:
			switch(((NMHDR*)lP)->code){
				case PSN_APPLY:
					if(cdSpeed!=oldSpeed && isCD && cdda) cd.setSpeed();
					break;
			}
			break;
	}
	return result;
}

BOOL CALLBACK optionsLocalCDDBProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 8);
	switch(mesg){
		case WM_INITDIALOG:
			TCHAR acp[14];
			_stprintf(acp, _T("CP%d"), GetACP());
			SetDlgItemText(hWnd, IDC_CDDBACP, acp);
			break;

		case WM_COMMAND:
			if(LOWORD(wP)==IDC_BROWSE){
				browseDir(browseDirFunc, hWnd, GetDlgItem(hWnd, IDC_CDDBFOLDER));
			}
			break;
	}
	return result;
}

BOOL CALLBACK sitesProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	int i, cmd;
	HWND list= GetDlgItem(hWnd, IDC_SITELIST);

	switch(mesg){
		case WM_INITDIALOG:
			dlgWin=hWnd;
			setDlgTexts(hWnd, 12);
			parseSites(list, -1);
			return TRUE;
		case WM_COMMAND:
			cmd=LOWORD(wP);
			switch(cmd){
				case IDC_SITES:
					getSites();
					SendMessageA(list, LB_RESETCONTENT, 0, 0);
					parseSites(list, -1);
					break;
				case IDC_SITELIST:
					if(HIWORD(wP)!=LBN_DBLCLK) break;
					//!
				case IDSELECT:
					i=SendDlgItemMessage(hWnd, IDC_SITELIST, LB_GETCURSEL, 0, 0);
					if(i>=0) parseSites(list, i);
				case IDCANCEL:
					EndDialog(hWnd, cmd);
					break;
			}
	}
	return FALSE;
}

BOOL CALLBACK optionsFreedbProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	BOOL result= propPageInit(hWnd, mesg, wP, lP, 9);
	switch(mesg){
		case WM_COMMAND:
			if(LOWORD(wP)==IDC_SHOWSITELIST){
				if(DialogBox(inst, MAKEINTRESOURCE(IDD_SITES), hWnd, (DLGPROC)sitesProc)!=IDCANCEL){
					optionsFreedbProc(hWnd, WM_INITDIALOG, 0, 0);
				}
				dlgWin=0;
			}
			break;
	}
	return result;
}

void options()
{
	int i;
	static DLGPROC P[]={(DLGPROC)optionsStartupProc,
		(DLGPROC)optionsGeneralProc, (DLGPROC)optionsPlaylistProc,
		(DLGPROC)optionsAudio1Proc, (DLGPROC)optionsAudio2Proc,
		(DLGPROC)optionsVideoProc, (DLGPROC)optionsSubtitlesProc,
		(DLGPROC)optionsCDProc, (DLGPROC)optionsLocalCDDBProc,
		(DLGPROC)optionsFreedbProc
	};
	static TCHAR *T[]={_T("Start"), _T("General"), _T("Playlist"),
		_T("Audio1"), _T("Audio2"), _T("Video"), _T("Subtitles"),
		_T("CD drive"), _T("LocalCDDB"), _T("freedb")};
	static int O[]={24, 25, 35, 26, 32, 27, 28, 34, 14, 29};
	PROPSHEETPAGE psp[sizeA(P)], *p;
	PROPSHEETHEADER psh;

	for(i=0; i<sizeA(psp); i++){
		p=&psp[i];
		p->dwSize = sizeof(PROPSHEETPAGE);
		p->dwFlags = PSP_USETITLE;
		p->hInstance = inst;
		p->pszTemplate = MAKEINTRESOURCE(110+i);
		p->pfnDlgProc = P[i];
		p->pszTitle = lng(O[i], T[i]);
	}
	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE;
	psh.hwndParent = hWin;
	psh.hInstance = inst;
	psh.pszCaption = lng(20, _T("Options"));
	psh.nPages = sizeA(psp);
	psh.nStartPage = optionsPage;
	psh.ppsp = psp;
	PropertySheet(&psh);
}

//------------------------------------------------------------------
LRESULT CALLBACK videoWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static bool dragging=false;
	static bool videoMoved;
	static POINT lastMousePos;
	static DWORD pauseTime;
	RECT rc;

	if(isDVD && dvdWndProc(hWnd, message, wParam, lParam)) return 1;

	switch(message){
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmm = (LPMINMAXINFO)lParam;
			lpmm->ptMinTrackSize.x = minW;
			lpmm->ptMinTrackSize.y = minH;
		}
			break;
		case WM_SIZE:
			if(!resizing && ratio2 && !fullscreen){
				GetWindowRect(hWnd, &rc);
				videoTop= rc.top;
				videoLeft= rc.left;
				if(keepRatio){
					resizeVideo((int)sqrt(double(LOWORD(lParam)) *
						HIWORD(lParam) * ratio1/ratio2));
				}
				else{
					ratio2=HIWORD(lParam);
					resizeVideo(ratio1=LOWORD(lParam));
				}
			}
			break;

		case WM_RBUTTONDOWN:
			hideWinOnFull();
			break;

		case WM_LBUTTONDBLCLK:
			setFullScreen(!fullscreen);
			if(GetTickCount()-pauseTime < 500) pause();
			break;
		case WM_LBUTTONDOWN:
			if(fullscreen){
				if(winOnFull) hideWinOnFull();
				else if(!isDVD || !g_bMenuOn){
					pause();
					pauseTime=GetTickCount();
				}
			}
			else{
				SetCapture(hWnd);
				GetCursorPos(&lastMousePos);
				SetRectEmpty(&videoSnap1);
				SetRectEmpty(&videoSnap2);
				dragging=true;
				videoMoved=false;
			}
			break;
		case WM_MOUSEMOVE:
			if(dragging){
				POINT curPos;
				GetCursorPos(&curPos);
				int dx= curPos.x-lastMousePos.x;
				int dy= curPos.y-lastMousePos.y;
				if(dx || dy){
					GetWindowRect(hWnd, &rc);
					OffsetRect(&rc, dx, dy);
					snapWnd1(&rc, &videoSnap1);
					snapWnd2(&rc, hWin, &videoSnap2);
					SetWindowPos(hWnd, 0, rc.left, rc.top,
						0, 0, SWP_NOOWNERZORDER|SWP_NOSIZE);
					lastMousePos=curPos;
					videoMoved=true;
				}
			}
			else{
				if(!fullscreen){
					SetCursor(LoadCursor(0, IDC_ARROW));
					SetTimer(hWnd, 11, 100, 0);
				}
				mouseMoveFull();
			}
			break;

		case WM_MOUSEWHEEL:
		{
			static int d;
			d+=(short)HIWORD(wParam);
			while(d>=WHEEL_DELTA){
				PostMessage(hWin, WM_COMMAND, ID_ZOOMIN, 0);
				d-=WHEEL_DELTA;
			}
			while(d<=-WHEEL_DELTA){
				PostMessage(hWin, WM_COMMAND, ID_ZOOMOUT, 0);
				d+=WHEEL_DELTA;
			}
		}
			break;

		case WM_MOVE:
			if(!pDDXM && pVW){
				pVW->NotifyOwnerMessage((LONG_PTR)hWnd, message, wParam, lParam);
			}
			if(onVideoMoved()){
				//moved to another monitor
				videoLeft = (short)LOWORD(lParam);
				videoTop = (short)HIWORD(lParam);
				restoreOvM();
			}
			break;
		case WM_LBUTTONUP:
			if(dragging){
				dragging=false;
				ReleaseCapture();
				if(videoMoved){
					GetWindowRect(hWnd, &rc);
					videoTop= rc.top;
					videoLeft= rc.left;
				}
			}
			break;
		case WM_SETCURSOR:
			//only for Overlay Mixer
			if(LOWORD(lParam)==HTCLIENT) return TRUE;
			return DefWindowProc(hWnd, message, wParam, lParam);

		case WM_TIMER:
			if(wParam==11){
				setMouse();
				KillTimer(hWnd, wParam);
			}
			break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_APPCOMMAND:
			PostMessage(hWin, message, wParam, lParam);
			break;
		case WM_ERASEBKGND:
			if(fullscreen && !pDDXM){
				blackSides((HDC)wParam);
			}
			return TRUE;
		case WM_PAINT:
		{
			static PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			if(pDDXM){
				HBRUSH br=CreateSolidBrush(colorKeyRGB);
				rc.left=videoFrameLeft;
				rc.right=rc.left+videoWidth;
				rc.top=videoFrameTop;
				rc.bottom=rc.top+videoHeight;
				FillRect(ps.hdc, &rc, br);
				DeleteObject(br);
				blackSides(ps.hdc);
			}
			else{
				UpdateWindow(FindWindowEx(hWnd, 0, 0, 0));
			}
			if(subtitles.on) subtitles.draw(ps.hdc);
			EndPaint(hWnd, &ps);
		}
			break;
		case WM_CLOSE:
			ShowWindow(hWnd, SW_HIDE);
			break;
		default:
			if(pVW) pVW->NotifyOwnerMessage((LONG_PTR)hWnd, message, wParam, lParam);
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

//------------------------------------------------------------------
LRESULT CALLBACK WndMainProc1(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int i, notif, st;
	TCHAR *fn;

	if(isDVD) dvdWndMainProc(hWnd, message, wParam, lParam);

	switch(message){

		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmm = (LPMINMAXINFO)lParam;
			lpmm->ptMinTrackSize.x = 300;
			lpmm->ptMinTrackSize.y = 46+timeH+toolH;
		}
			break;

		case WM_ACTIVATE:
			if(LOWORD(wParam)!=WA_INACTIVE){
				if(((HWND)lParam)!=visualWnd){
					SetWindowPos(visualWnd, hWin, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_ASYNCWINDOWPOS);
					if(isTop()) SetWindowPos(visualWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_ASYNCWINDOWPOS);
				}
				if(isVideo && ((HWND)lParam)!=videoWnd){
					SetWindowPos(videoWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
				}
				else{
					SetFocus(listbox);
				}
			}
			break;

		case WM_PAINT:
		{
			static PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			lastPos=timeL;
			lastTime=-1;
			repaintTime(ps.hdc);
			EndPaint(hWnd, &ps);
		}
			break;

		case WM_TIMER:
			if(wParam==1002){
				static POINT p, p1;
				GetCursorPos(&p1);
				if(p.x!=p1.x || p.y!=p1.y){
					p=p1;
				}
				else{
					hideWinOnFull();
				}
			}
			else{
				if(getPosition()){
					if(!isVideo || !fullscreen || winOnFull || multiMonitors()) repaintTime(0);
					subtitles.timer();
					if(!isVideo && !isCD && repeat!=2 && !audioFilter){
						int remain= int((playlist[currentFile].length-position)/10000);
						if(gapless){
							if(remain<loadAdvanceTime && currentFile+1<playlist.len){
								fn= playlist[currentFile+1].file;
								if(gaplessFile!=fn){
									HANDLE thread= GetCurrentThread();
									SetThreadPriority(thread, THREAD_PRIORITY_LOWEST);
									gaplessFile=fn;
									releaseGB(pNextGB);
									if(SUCCEEDED(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
										IID_IGraphBuilder, (void **)&pNextGB))){
										if(FAILED(renderFile(pNextGB, fn))){
											releaseGB(pNextGB);
										}
									}
									SetThreadPriority(thread, THREAD_PRIORITY_NORMAL);
								}
							}
						}
						if(crossfade){
							remain-=crossfade;
							if(remain<tick && wavFile==INVALID_HANDLE_VALUE){
								if(remain>15) Sleep(remain);
								nextClip();
							}
						}
					}
				}
			}
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if(wParam==VK_ESCAPE){
				if(fullscreen) setFullScreen(0);
			}
			if(wParam==VK_RETURN && GetKeyState(VK_MENU)<0){
				setFullScreen(!fullscreen);
			}
			break;

		case WM_APPCOMMAND: //multimedia keys
		{
			int c= GET_APPCOMMAND_LPARAM(lParam);
			if(c==APPCOMMAND_MEDIA_PLAY_PAUSE) PostMessage(hWnd, WM_COMMAND, ID_CONTROL_PLAYPAUSE, 0);
			else if(c==APPCOMMAND_MEDIA_PREVIOUSTRACK) PostMessage(hWnd, WM_COMMAND, ID_FILE_PREVIOUS, 0);
			else if(c==APPCOMMAND_MEDIA_NEXTTRACK) PostMessage(hWnd, WM_COMMAND, ID_FILE_NEXT, 0);
			else if(c==APPCOMMAND_MEDIA_STOP) PostMessage(hWnd, WM_COMMAND, ID_FILE_STOP, 0);
			else return DefWindowProc(hWnd, message, wParam, lParam);
		}
			break;

		case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *lpdis = (LPDRAWITEMSTRUCT)lParam;
			if(lpdis->itemID == -1) break;
			drawItem(lpdis);
			break;
		}
		case WM_CTLCOLORLISTBOX:
			return (LRESULT)listBrush;

		case WM_MOVE:
		case WM_EXITSIZEMOVE:
			onMoved();
			break;
		case WM_MOVING:
		{
			static RECT d;
			snapWnd1((RECT*)lParam, &d);
		}
			return TRUE;

		case WM_SIZE:
			width=LOWORD(lParam);
			onMoved();
			setListSize();
			invalidateTime();
			break;

		case WM_DROPFILES:
			drop((HDROP)wParam);
			DragFinish((HDROP)wParam);
			break;

		case WM_NOTIFY:
		{
			HD_NOTIFY *nmhdr;
			NMTOOLBAR *nmtool;
			switch(((NMHDR*)lParam)->code){
				case TBN_QUERYDELETE:
					/*nmtool= (NMTOOLBAR*) lParam;
					i= nmtool->tbButton.idCommand;
					if(i==ID_FILE_STOP || i==ID_FILE_PLAY || i==ID_FILE_PAUSE) return FALSE;*/
					return TRUE;
				case TBN_QUERYINSERT:
					return TRUE;
				case TBN_GETBUTTONINFO:
					nmtool= (NMTOOLBAR*)lParam;
					i= nmtool->iItem;
					if(i<sizeA(tbb)){
						convertA2T(toolNames[i], t);
						lstrcpyn(nmtool->pszText, lng(tbb[i].idCommand+1000, t), nmtool->cchText);
						nmtool->tbButton= tbb[i];
						return TRUE;
					}
					break;
				case TBN_RESET:
					while(SendMessage(toolbar, TB_DELETEBUTTON, 0, 0));
					SendMessage(toolbar, TB_ADDBUTTONS, Ntool, (LPARAM)tbb);
					break;
				case HDN_TRACK:
					nmhdr = (HD_NOTIFY*)lParam;
					colWidth[nmhdr->iItem]= (short)nmhdr->pitem->cxy;
					Header_SetItem(header, nmhdr->iItem, nmhdr->pitem);
					invalidateList();
					break;
				case HDN_ITEMCLICK:
					nmhdr = (HD_NOTIFY*)lParam;
					if(sortedCol==nmhdr->iItem){
						descending= -descending;
					}
					else{
						sortedCol=nmhdr->iItem;
						descending= 1;
					}
					sortList();
					invalidateList();
					break;
			}
		}
			break;

		case WM_LBUTTONDOWN:
			if(HIWORD(lParam)<toolH+timeH && HIWORD(lParam)>=toolH){
				i=lParam2pos(lParam);
				if(i==1){
					elapsedRemain();
				}
				else if(i==0){
					seeking=true;
					SetCapture(hWnd);
					invalidateTime();
				}
			}
			break;
		case WM_LBUTTONUP:
			if(seeking){
				ReleaseCapture();
				seeking=false;
				invalidateTime();
				if(lParam2pos(lParam)<=1){
					st=state;
					if(st==ID_FILE_STOP) pause();
					if(state==ID_FILE_STOP) break;
					seek(seekPos);
					if(st==ID_FILE_STOP) PlayClip();
				}
			}
			break;
		case WM_MOUSEMOVE:
			if(seeking){
				lParam2pos(lParam);
				invalidateTime();
			}
			break;

		case WM_COPYDATA:
			if(((COPYDATASTRUCT*)lParam)->dwData==argMagic){
				convertW2T((WCHAR*)((COPYDATASTRUCT*)lParam)->lpData, t);
				addFileArg(t);
				return startForeground;
			}
			break;

		case WM_GRAPHNOTIFY:
			HandleGraphEvent();
			break;

		case MM_MCINOTIFY:
			if(isCD){
				if(wParam==MCI_NOTIFY_SUCCESSFUL) nextClip();
				if(wParam==MCI_NOTIFY_FAILURE) CloseClip();
			}
			break;

		case WM_USER+2: //end of file
			waNext();
			break;

		case WM_DISPLAYCHANGE:
			scrW=LOWORD(lParam);
			scrH=HIWORD(lParam);
			if(pDDXM){
				if(isDVD){
					dvdRestore();
				}
				else{
					restoreOvM();
				}
			}
			else{
				getDefaultKeyColor();
				sizeChanged();
			}
			break;

		case MM_MIXM_CONTROL_CHANGE:
			controlVolume(-1);
			break;

		case WM_CLOSE:
			SendMessage(hWin, WM_COMMAND, ID_FILE_EXIT, 0);
			break;

		case WM_QUERYENDSESSION:
			writeini();
			return TRUE;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_GETSTATE:
			return state;

		case WM_COMMAND:
			notif=HIWORD(wParam);
			wParam=LOWORD(wParam);
			if(setLang(wParam)) break;
			if(wParam>=ID_FILTERS && wParam<ID_FILTERS+50){
				filterProperty(wParam);
				break;
			}
			if(wParam>=ID_VISUALS && wParam<ID_VISUALS+MAXVISUALS){
				lstrcpy(visualDLL, visualNames[wParam-ID_VISUALS]);
				addVisual();
			}
			switch(wParam){

				case 101: //listbox
					if(notif==LBN_DBLCLK){
						i=SendMessage(listbox, LB_GETCURSEL, 0, 0);
						setSel(i, false);
						gotoFile(i);
					}
					break;
				case 900:
					updateCD();
					break;
				case 902:
					updateDVD();
					break;
				case 903:
					ratio(origW, origH);
					break;
				case 904:
					if(isDVD) SetTitle(1);
					break;
				case ID_PASTE:
					if(OpenClipboard(hWin)){
						drop((HDROP)GetClipboardData(CF_HDROP));
						CloseClipboard();
					}
					break;
				case ID_HELP_README:
				{
					TCHAR *buf=(TCHAR*)_alloca(2*MAX_PATH);
					getExeDir(buf, lng(13, _T("tinyplay_EN.txt")));
					if(ShellExecute(0, _T("open"), buf, 0, 0, SW_SHOWNORMAL)==(HINSTANCE)ERROR_FILE_NOT_FOUND){
						msglng(739, "File %s not found", buf);
					}
				}
					break;
				case ID_REFRESH:
					PostMessage(hWnd, WM_TIMER, 1000, 0);
					break;
				case ID_FILE_OPENCLIP:
					noTopMost();
					openFiles();
					noHideWinOnFull--;
					break;
				case ID_PLAYAUDIOCD:
					playCD();
					break;
				case ID_CDINFO:
					cdEditInfo();
					break;
				case ID_OPENDIR:
					noTopMost();
					openDir();
					noHideWinOnFull--;
					break;
				case ID_OPENURL:
					DialogBox(inst, MAKEINTRESOURCE(IDD_URL), hWin, (DLGPROC)UrlDlgProc);
					break;
				case ID_DELETEINI:
					delreg=true;;
					break;
				case ID_HELP_ABOUT:
					DialogBox(inst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWin, (DLGPROC)AboutDlgProc);
					break;
				case ID_OPTIONS:
					noTopMost();
					options();
					noHideWinOnFull--;
					hideWinOnFull();
					break;
				case ID_RATE_CUSTOM:
					DialogBox(inst, MAKEINTRESOURCE(IDD_RATE), hWin, (DLGPROC)RateDlgProc);
					break;
				case ID_JUMPTOTIME:
					DialogBox(inst, MAKEINTRESOURCE(IDD_JUMP), hWin, (DLGPROC)JumpDlgProc);
					break;
				case ID_COLORS:
					DialogBox(inst, MAKEINTRESOURCE(IDD_COLORS), hWin, (DLGPROC)ColorProc);
					break;
				case ID_FILE_EXIT:
					writeini();
					CloseClip();
					DestroyWindow(hWin);
					break;
				case ID_FILEINFO:
					i=SendMessage(listbox, LB_GETCURSEL, 0, 0);
					if(!getSel(i)) i=currentFile;
					fileInfo(i);
					hideWinOnFull();
					break;
				case ID_DELETE_FILE:
					removeCurrent();
					break;
				case ID_AUDIOSTREAM:
					switchAudioStream();
					break;

				case ID_FILE_FONT:
				{
					static CHOOSEFONT f;
					f.lStructSize=sizeof(CHOOSEFONT);
					f.hwndOwner=hWin;
					f.Flags=CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT|CF_SCRIPTSONLY|CF_ENABLEHOOK|CF_APPLY;
					f.lpLogFont=&font;
					f.lpfnHook=CFHookProc;
					if(ChooseFont(&f)) setFont();
				}
					break;
				case ID_SUBTITLES_FONT:
					chooseSubtFont(hWnd);
					break;
				case ID_TOOLBAR:
					SendMessage(toolbar, TB_CUSTOMIZE, 0, 0);
					break;
				case ID_KEYS:
					DialogBox(inst, MAKEINTRESOURCE(IDD_KEYS), hWin, (DLGPROC)KeysDlgProc);
					break;

				case ID_PLAYLIST_OPEN:
					noTopMost();
					openSavePlaylist(false);
					noHideWinOnFull--;
					break;
				case ID_PLAYLIST_SAVE:
					noTopMost();
					openSavePlaylist(true);
					noHideWinOnFull--;
					hideWinOnFull();
					break;
				case ID_PLAYLIST_REMOVE:
					removeSelected(false);
					break;
				case ID_PLAYLIST_CROP:
					removeSelected(true);
					break;
				case ID_PLAYLIST_REMOVEALL:
					removeAll();
					break;
				case ID_PLAYLIST_REMOVEPREVIOUS:
					removePrev();
					break;
				case ID_PLAYLIST_REVERSE:
					reverseList();
					break;
				case ID_PLAYLIST_INVERTSELECTION:
					invertList();
					break;
				case ID_PLAYLIST_SELECTALL:
					setSel(-1, true);
					break;
				case ID_UNSELECTALL:
					setSel(-1, false);
					break;
				case ID_ALLINFO:
					allInfo();
					break;

				case ID_FILE_PLAY:
					if(state==ID_FILE_PLAY){
						restart();
					}
					else{
						if(currentFile<0) currentFile=0;
						PlayClip();
					}
					break;
				case ID_FILE_PAUSE_NOPLAY:
					if(state==ID_FILE_STOP) break;
				case ID_FILE_PAUSE:
					pause();
					break;
				case ID_CONTROL_PLAYPAUSE:
					if(state==ID_FILE_STOP){
						PlayClip();
					}
					else{
						pause();
					}
					break;
				case ID_CONTROL_JUMPFORWARD:
					seekRel(jumpSec);
					break;
				case ID_CONTROL_JUMPBACKWARD:
					seekRel(-jumpSec);
					break;
				case ID_FILE_PREVIOUS_NOPLAY:
					if(state==ID_FILE_STOP) break;
				case ID_FILE_PREVIOUS:
					gotoFile(currentFile-1);
					break;
				case ID_FILE_NEXT_NOPLAY:
					if(state==ID_FILE_STOP) break;
				case ID_FILE_NEXT:
					if(currentFile==playlist.len-1 && state==ID_FILE_STOP){
						gotoFile(lastStartPos);
					}
					else{
						gotoFile(currentFile+1);
					}
					break;
				case ID_FILE_STOP:
					if(GetKeyState(VK_SHIFT)>=0){
						if(state!=ID_FILE_STOP){
							fadeOut(smoothStop);
							CloseClip();
							playBtn(ID_FILE_STOP);
						}
						break;
					}
					//!
				case ID_CONTROL_STOPAFTERCURRENT:
					stopAfterCurrent=!stopAfterCurrent;
					break;
				case ID_TIMEREMAIN:
					elapsedRemain();
					break;

				case ID_REPEAT:
					repeat= repeat==1 ? 0 : 1;
					checkMenus();
					break;
				case ID_REPEAT1:
					repeat= repeat==2 ? 0 : 2;
					checkMenus();
					break;
				case ID_REPEAT_STOP:
					repeat= repeat==3 ? 0 : 3;
					checkMenus();
					break;
				case ID_RANDOM:
					randomPlay=!randomPlay;
					checkMenus();
					break;
				case ID_SHUFFLE:
					if(state==ID_FILE_STOP && (currentFile==0 || playlist.len<3)){
						shuffle(0, playlist.len);
						currentFile=0;
					}
					else{
						shuffle(0, currentFile);
						shuffle(currentFile+1, playlist.len-currentFile-1);
					}
					setSel(-1, false);
					invalidateList();
					break;
				case ID_SHOW_CURRENT:
					ensureVisible();
					break;
				case ID_FILE_FULLSCREEN:
					setFullScreen(!fullscreen);
					break;
				case ID_GRAB:
					if(isVideo) grab();
					else msglng(782, "No video is playing");
					break;
				case ID_VISUAL:
					isVisual= !isVisual;
					showVisual();
					timersChanged();
					checkMenus();
					if(!isVisual) SetFocus(listbox);
					break;
				case ID_NEXTVISUAL:
					nextVisual();
					break;

				case ID_RATIO_ORIG:
					forceOrigRatio++;
					ratio(origW, origH);
					forceOrigRatio--;
					break;
				case ID_RATIO_SCREEN:
					ratio(scrW, scrH);
					break;
				case ID_RATIO169:
					ratio(16, 9);
					break;
				case ID_RATIO34:
					ratio(4, 3);
					break;

				case ID_FILE_SIZE_HALF:
					resizeVideo(1, 2);
					break;
				case ID_FILE_SIZE_NORMAL:
					if(fullscreen && videoWidth!=scrW && videoHeight!=scrH) sizeChanged();
					else resizeVideo(1, 1);
					break;
				case ID_FILE_SIZE_DOUBLE:
					resizeVideo(2, 1);
					break;
				case ID_FILE_SIZE_THREEQUARTER:
					resizeVideo(3, 4);
					break;
				case ID_FILE_SIZE_150:
					resizeVideo(3, 2);
					break;
				case ID_SINGLE_STEP:
					StepFrame();
					break;

				case ID_ONTOP:
				{
					bool b=isTop();
					SetWindowPos(hWin, b ? HWND_NOTOPMOST : HWND_TOPMOST,
						 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
					if(!fullscreen) SetWindowPos(videoWnd, b ? HWND_NOTOPMOST : HWND_TOPMOST,
						 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
					checkMenus();
					break;
				}
				case ID_ZOOMIN:
					resizeVideo(videoWidth+scrW/50);
					break;
				case ID_ZOOMOUT:
					resizeVideo(videoWidth-scrW/50);
					break;

				case ID_RATE_DECREASE:
					modifyRate(-0.05);
					break;
				case ID_RATE_INCREASE:
					modifyRate(0.05);
					break;
				case ID_RATE_NORMAL:
					setRate(1.0);
					break;
				case ID_RATE_HALF:
					setRate(0.5);
					break;
				case ID_RATE_DOUBLE:
					setRate(2.0);
					break;
				case ID_RATE_75:
					setRate(0.75);
					break;
				case ID_RATE_125:
					setRate(1.25);
					break;

				case ID_SUBTITLES_LOAD:
					noTopMost();
					openSubtitles();
					invalidateVideo();
					noHideWinOnFull--;
					hideWinOnFull();
					break;
				case ID_SUBTITLES_LATER:
					subtitles.delta-=SUBTDELTA;
					subtitles.reset();
					break;
				case ID_SUBTITLES_EARLIER:
					subtitles.delta+=SUBTDELTA;
					break;
				case ID_SUBTITLES_MOVEUP:
					if(subtitles.file){
						if(fullscreen) subtPosFull+=SUBTMOVE;
						else subtPosWin+=SUBTMOVE;
						invalidateVideo();
					}
					break;
				case ID_SUBTITLES_MOVEDOWN:
					if(subtitles.file){
						if(fullscreen) subtPosFull-=SUBTMOVE;
						else subtPosWin-=SUBTMOVE;
						invalidateVideo();
					}
					break;
				case ID_SUBTITLES_LARGER:
					subtFont.lfHeight++;
					invalidateVideo();
					break;
				case ID_SUBTITLES_SMALLER:
					if(subtFont.lfHeight>MINFONT){
						subtFont.lfHeight--;
						invalidateVideo();
					}
					break;
				case ID_SUBTITLES_OFF:
					subtitles.disabled= !subtitles.disabled;
					if(!subtitles.disabled && isVideo && !subtitles.file){
						findSubtitles();
					}
					invalidateVideo();
					checkMenus();
					break;
				case ID_SUBTITLES_TIMECORRECTION:
					DialogBox(inst, MAKEINTRESOURCE(IDD_SUBTTIME), hWin, (DLGPROC)SubtTimeDlgProc);
					break;

				case ID_SOUND_STEREO:
				case ID_SOUND_MONO:
				case ID_SOUND_LEFT:
				case ID_SOUND_RIGHT:
				case ID_SOUND_MUTE:
				case ID_SOUND_WAV:
					i= wParam;
					if(i==soundConversion) i=ID_SOUND_STEREO;
					soundConversion=i;
					checkSoundMenu();
					break;
				case ID_EQUALIZER:
					showEqualizer();
					break;
				case ID_DDCOLOR:
					showDDColor();
					break;

				case ID_DVD_FILE_OPENDVD:
					addFile(dvdFirstPlayName);
					break;
				case ID_DVD_SHOWCC:
					// enable or disable Line21 closed captioning
					g_bDisplayCC ^= 1;
					if(pLine21Dec){
						pLine21Dec->SetServiceState(g_bDisplayCC ? AM_L21_CCSTATE_On : AM_L21_CCSTATE_Off);
					}
					CheckMenuItem(menuDVD, ID_DVD_SHOWCC, g_bDisplayCC ? MF_CHECKED : MF_UNCHECKED);
					break;
				case ID_DVD_SHOWSUBPICTURE:
					g_bDisplaySubpicture ^= 1;
					if(pDvdControl){
						pDvdControl->SetSubpictureState(g_bDisplaySubpicture, DVD_CMD_FLAG_None, NULL);
					}
					EnableSubpicture();
					break;
			}
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);

	}
	return 0;
}

LRESULT CALLBACK WndMainProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	EnterCriticalSection(&infoCritSect);
	LRESULT result= WndMainProc1(hWnd, message, wParam, lParam);
	LeaveCriticalSection(&infoCritSect);
	return result;
}
//------------------------------------------------------------------

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR cmdLine, int cmdShow)
{
	MSG mesg;
	COPYDATASTRUCT d;

#ifdef UNICODE
	int argc;
	LPWSTR* argv=CommandLineToArgvW(GetCommandLine(), &argc);
#endif

	HKEY key;
	if(RegOpenKeyEx(HKEY_CURRENT_USER, subkey, 0, KEY_QUERY_VALUE, &key)==ERROR_SUCCESS){
		DWORD d=sizeof(int);
		RegQueryValueEx(key, _T("multInst"), 0, 0, (BYTE *)&multInst, &d);
		RegCloseKey(key);
	}
	// check if the player is already running
	HANDLE mutex = CreateMutex(0, FALSE, CLASSNAME);
	WaitForSingleObject(mutex, 50000);
	HWND w=FindWindow(CLASSNAME, 0);
	if(w){
		if(!multInst){
			if(cmdLine[0]){
#ifdef UNICODE
				d.lpData= argv[1];
#else
				convertT2W(cmdLine,cmdLineW);
				d.lpData=cmdLineW;
#endif
				d.cbData= (wcslen((WCHAR*)d.lpData)+1)*2;
				d.dwData= argMagic;
				if(SendMessage(w, WM_COPYDATA, (WPARAM)w, (LPARAM)&d)){
					SetForegroundWindow(w);
					w=FindWindow(VIDEOCLASSNAME, 0);
					if(w && IsWindowVisible(w)){
						Sleep(100);
						SetForegroundWindow(w);
					}
				}
			}
			else{
				SetForegroundWindow(w);
			}
			return 1;
		}
	}
	inst = hInstance;
	_tcscpy(subtExt, _T("sub;srt;txt;aqt;dks;stf"));
	_tcscpy(listExt, _T("m3u;wpl;asx;wax;wvx"));
	_tcscpy(outRender, _T("Default DirectSound Device"));
	_tcscpy(associated, _T("mp3;wma;wav;avi;mpg;mpeg;wmv"));
	_tcscpy(servername, _T("freedb.freedb.org"));
	_tcscpy(serverCgi, _T("/~cddb/cddb.cgi"));
	_tcscpy(serverSubmitCgi, _T("/~cddb/submit.cgi"));
	_tcscpy(fmtTitle, _T("[%ar - ]Tiny Player"));
	getExeDir(savedList, _T("tinyplay.m3u"));

	GetObject(GetStockObject(SYSTEM_FONT), sizeof(LOGFONT), &font);
	subtFont.lfHeight=35;
	subtFont.lfWeight=FW_BOLD;
	subtFont.lfCharSet=DEFAULT_CHARSET;
	lstrcpy(subtFont.lfFaceName, _T("Arial"));
	readini();
	// create the main window
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = WndMainProc;
	wc.hInstance = inst;
	wc.lpszClassName = CLASSNAME;
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU);
	wc.hbrBackground = bkgndBrush = CreateSolidBrush(colors[clBkgnd]);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = LoadIcon(inst, MAKEINTRESOURCE(IDI_PLAYWND));
	if(!RegisterClass(&wc)){
#ifdef UNICODE
		msg("This version cannot run on Windows 95/98/ME.");
#else
		msg("RegisterClass failed");
#endif
		return 2;
	}
	//screen coordinates
	scrW= GetSystemMetrics(SM_CXSCREEN);
	scrH= GetSystemMetrics(SM_CYSCREEN);
	RECT rc;
	if(multiMonitors()){
		rc.left= GetSystemMetrics(SM_XVIRTUALSCREEN);
		rc.top= GetSystemMetrics(SM_YVIRTUALSCREEN);
		rc.right= rc.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
		rc.bottom= rc.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
	}
	else{
		SetRect(&rc, 0, 0, scrW, scrH);
	}
	//adjust window positions if they are outside the screen
	aminmax(mainLeft, rc.left-5, rc.right-100);
	aminmax(mainTop, rc.top-5, rc.bottom-80);
	aminmax(videoLeft, rc.left-2, rc.right-60);
	aminmax(videoTop, rc.top-2, rc.bottom-40);
	aminmax(visualLeft, rc.left-10, rc.right-50);
	aminmax(visualTop, rc.top-10, rc.bottom-30);

	aminmax(subtPosFull, 0, scrH-10);
	aminmax(subtPosWin, 0, scrH-20);
	aminmax(subtFont.lfHeight, MINFONT, 200);

	InitializeCriticalSection(&infoCritSect);
	hWin = CreateWindow(CLASSNAME, winTitle,
		WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_CLIPCHILDREN,
		mainLeft, mainTop, mainW, mainH, 0, 0, inst, 0);
	if(!hWin){
		msg("CreateWindow failed");
		return 3;
	}
	ReleaseMutex(mutex);
	CloseHandle(mutex);

	memset(custom, 200, sizeof(custom));
	setPriority();
	initLang();
	timeH= HIWORD(GetDialogBaseUnits());

	// load common controls and COM 
#if _WIN32_IE >= 0x0300
	INITCOMMONCONTROLSEX iccs;
	iccs.dwSize= sizeof(INITCOMMONCONTROLSEX);
	iccs.dwICC= ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES;
	InitCommonControlsEx(&iccs);
#else
	InitCommonControls();
#endif
	if(FAILED(CoInitialize(0))){
		msg("CoInitialize failed");
		return 4;
	}
	setThreadExecutionState= (TsetExeState)GetProcAddress(GetModuleHandle(_T("KERNEL32.DLL")), "SetThreadExecutionState");
	//create video window, listbox, header and toolbar
	ZeroMemory(&wc, sizeof(wc));
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = videoWndProc;
	wc.hInstance = inst;
	wc.lpszClassName = VIDEOCLASSNAME;
	RegisterClass(&wc);
	videoWnd = CreateWindowEx(WS_EX_TOOLWINDOW,
		VIDEOCLASSNAME, winTitle,
		WS_THICKFRAME|WS_POPUP,
		videoLeft, videoTop, 1, 1, 0, 0, inst, 0);

	wc.style = 0;
	wc.lpfnWndProc = EqualizerProc;
	wc.lpszClassName = EQUALIZERCLASSNAME;
	wc.hbrBackground = (HBRUSH)COLOR_BTNSHADOW;
	RegisterClass(&wc);

	listbox = CreateWindow(_T("LISTBOX"), _T(""),
		LBS_NOTIFY | LBS_OWNERDRAWFIXED | WS_CHILD | LBS_EXTENDEDSEL |
		WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | LBS_NODATA | LBS_NOINTEGRALHEIGHT,
		0, 0, 100, 50, hWin, (HMENU)101, inst, NULL);
	listWndProc = (WNDPROC)SetWindowLong(listbox, GWL_WNDPROC, (LONG)listProc);
	header= CreateWindowEx(0, WC_HEADER, 0,
		WS_CHILD | WS_BORDER | HDS_BUTTONS | HDS_HORZ,
		0, 0, 0, 0, hWin, (HMENU)102, inst, 0);
	listBrush=CreateSolidBrush(colors[clBkList]);

	int n=sizeA(tbb);
	for(TBBUTTON *u=tbb; u<endA(tbb); u++){
		if(u->fsStyle==TBSTYLE_SEP) n--;
	}
	toolbar = CreateToolbarEx(hWin,
		WS_CHILD|TBSTYLE_TOOLTIPS|0x800|WS_VISIBLE, 2, n,
		inst, IDB_PLAYBTN, tbb, Ntool,
		16, 15, 16, 15, sizeof(TBBUTTON));
	TBSAVEPARAMS sp;
	sp.hkr=HKEY_CURRENT_USER;
	sp.pszSubKey=subkey;
	sp.pszValueName=_T("toolbar");
	SendMessage(toolbar, TB_SAVERESTORE, FALSE, (LPARAM)&sp);

	trackbar = CreateWindow(TRACKBAR_CLASS, _T(""),
		WS_CHILD|TBS_NOTICKS|TBS_BOTH, 0, 0, volumeW, volumeH, toolbar, (HMENU)103, inst, NULL);
	SendMessage(trackbar, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(0, 100));
	SendMessage(trackbar, TBM_SETPAGESIZE, 0, (LPARAM)5);
	toolbarWndProc = (WNDPROC)SetWindowLong(toolbar, GWL_WNDPROC, (LONG)toolbarClassProc);

	DDColorWnd = CreateDialog(inst, MAKEINTRESOURCE(IDD_DDCOLOR), 0, (DLGPROC)DDColorProc);

	setFont();
	setListSize();
	findDLL();
	accelChanged();
	langChanged();
	ShowWindow(hWin, SW_SHOW);
	DragAcceptFiles(hWin, TRUE);
	visualInit();
	getDefaultKeyColor();

	openList(savedList);
	if(currentFile>=playlist.len || currentFile<0){
		currentFile= playlist.len ? 0 : -1;
	}
	srand((unsigned)time(0));

#ifdef UNICODE
	if(argc>1) addFileArg(argv[1]);
#else
	addFileArg(cmdLine);
#endif

	UpdateWindow(hWin);
	if(eqVisible) showEqualizer();
	if(volumeType){
		controlVolume(-1);
		ShowWindow(trackbar, SW_SHOW);
	}
	if(regExtAtStartup) associateFiles(associated, _T(""));

	while(GetMessage(&mesg, NULL, 0, 0)==TRUE){
		if(mesg.hwnd==videoWnd ||
			!TranslateAccelerator(hWin, haccel, &mesg)){
			TranslateMessage(&mesg);
			DispatchMessage(&mesg);
		}
	}
	visualShut();
	playlist.reset();
	if(infoThread) WaitForSingleObject(infoThread, 20000);
	DeleteCriticalSection(&infoCritSect);
	if(hMixer) mixerClose((HMIXER)hMixer);
	DeleteObject(hFont);
	DeleteObject(listBrush);
	DeleteObject(bkgndBrush);
	DestroyAcceleratorTable(haccel);
	CoUninitialize();
	if(delreg) deleteini();
	return 0;
}
