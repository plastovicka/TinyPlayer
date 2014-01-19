/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#include "darray.h"
#include "lang.h"
#include "resource.h"

#define CLASSNAME _T("PlasPlayer")
#define VIDEOCLASSNAME _T("PlasVideo")
#define VISUALCLASSNAME _T("PlasVisual")
#define EQUALIZERCLASSNAME _T("PlasEqualizer")
#define WM_GRAPHNOTIFY WM_APP+1
#define WM_VISUAL_PROPERTY WM_APP+3
#define WM_AUDIO_PROPERTY WM_APP+4
#define WM_GETSTATE WM_APP+5
#define VISUAL_FILTER_NAME _T("Visualization ...")
#define VISUAL_FILTER_NAMEW L"Visualization ..."
#define AUDIO_FILTER_NAME _T("Audio renderer...")

#define MENU_SUBTITLES 5
#define MENU_FILTERS 6
#define MENU_DVD 7

#define minW 40
#define minH 30
#define SUBTEXT_LEN 128
#define LISTEXT_LEN 128
#define ID_VISUALS 40000
const int MAXVISUALS=16;
const int Mequalizer=20;
#define ID_SOUND_FIRST ID_SOUND_WAV
#define ID_SOUND_LAST ID_SOUND_MUTE
#define UNITS 10000000
const int Mcolumns=16;

enum{
	T_TITLE, T_ARTIST, T_ALBUM, T_TRACK,
	T_YEAR, T_COMMENT, T_GENRE, T_COPYRIGHT, T_ENCODER,
T_URL, T_DESCRIPTION, T_COMPOSER, T_ORIG_ARTIST, T_TAG, MTAGS
};

typedef EXECUTION_STATE(WINAPI *TsetExeState)(EXECUTION_STATE);
typedef HRESULT(WINAPI *TddrawCreate)(GUID*, LPDIRECTDRAW*, IUnknown*);
typedef HRESULT(WINAPI *TdirectDrawEnumerateEx)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);
typedef HMONITOR(WINAPI* TmonitorFromRect)(LPCRECT, DWORD);
typedef BOOL(WINAPI* TgetMonitorInfo)(HMONITOR, LPMONITORINFO);

extern TsetExeState setThreadExecutionState;
extern TmonitorFromRect monitorFromRect;
extern TgetMonitorInfo getMonitorInfo;

enum{
	clList, clBkList, clSelect, clBkSelect, clCurItem, clBkCurItem,
	clProgress, clBkgnd, clTime, clBkTime, clSubtFill, clSubtOut,
clBkSubt, Ncl
};
extern COLORREF colors[Ncl];

struct Titem {
	TCHAR *file;
	TCHAR *name;
	TCHAR *ext;
	TCHAR *title, *artist, *album, *track, *year, *comment, *genre, *tag;
	int bitrate;
	LONGLONG size;
	LONGLONG length;
	void init(TCHAR *fn, int len);
	void free();
	void freeTags();
	void getSize();
	void calcBitrate();
};

struct TeqParam {
	float freq;
	float value;
	int index;
};

template <class T> inline void aminmax(T &x, int l, int h){
	if(x<l) x=l;
	if(x>h) x=h;
}

template <class T> inline void amax(T &x, int h){
	if(x>h) x=h;
}

template <class T> inline void amin(T &x, int l){
	if(x<l) x=l;
}

#define SAFE_RELEASE(x) { if(x) x->Release(); x = NULL; }
#define RELEASE(x) { x->Release(); x = NULL; }

//------------------------------------------------------------------
#ifdef UNICODE 
#define convertT2W(t,w) \
	WCHAR *w=t
#define convertW2T(w,t) \
	TCHAR *t=w
#define convertA2T(a,t) \
	int cnvlen##t=strlen(a)+1;\
	TCHAR *t=(TCHAR*)_alloca(2*cnvlen##t);\
	MultiByteToWideChar(CP_ACP, 0, a, -1, t, cnvlen##t);
#define convertT2A(t,a) \
	int cnvlen##a=wcslen(t)*2+1;\
	char *a=(char*)_alloca(cnvlen##a);\
	a[0]=0;\
	WideCharToMultiByte(CP_ACP, 0, t, -1, a, cnvlen##a, 0,0);
#else
#define convertT2A(t,a) \
	char *a=t
#define convertA2T(a,t) \
	TCHAR *t=a
#define convertW2T(w,t) \
	int cnvlen##t=wcslen(w)*2+1;\
	TCHAR *t=(TCHAR*)_alloca(cnvlen##t);\
	WideCharToMultiByte(CP_ACP, 0, w, -1, t, cnvlen##t, 0,0);
#define convertT2W(t,w) \
	int cnvlen##w=strlen(t)+1;\
	WCHAR *w=(WCHAR*)_alloca(2*cnvlen##w);\
	MultiByteToWideChar(CP_ACP, 0, t, -1, w, cnvlen##w);
#endif

#define convertA2W(cp,a,w) \
	int cnvlen##w=strlen(a)+1;\
	WCHAR *w=(WCHAR*)_alloca(2*cnvlen##w);\
	MultiByteToWideChar(cp, 0, a, -1, w, cnvlen##w);


//------------------------------------------------------------------
extern IGraphBuilder *pGB;
extern IMediaControl *pMC;
extern IMediaEventEx *pME;
extern IVideoWindow  *pVW;
extern IBasicVideo   *pBV;
extern IBasicAudio   *pBA;
extern IMediaSeeking *pMS;
extern IMediaPosition *pMP;
class VisualizationFilter;
extern VisualizationFilter *visualFilter;
class AudioFilter;
extern AudioFilter *cdAudioFilter;
extern AudioFilter *waAudioFilter;
extern IDDrawExclModeVideo *pDDXM;
extern IGraphBuilder *pNextGB;
extern IMixerPinConfig2 *pColor;

extern double playbackRate, frameRate, seekCorrection;
extern bool stopAfterCurrent, isVideo, isCD, isDVD, winOnFull, isGrabbingWAV, mciPlaying, isAvi, seeking;
extern void *visualData;
extern long ratio1, ratio2, origW, origH;
extern int state, repeat, visualTimer, visualDialogX, visualDialogY, tick, numAdded, soundConversion, sortOnLoad, smoothPause, smoothStop, smoothSeek, Ncolumns, convertUnderscores;
extern int isVisual, fullscreen, forceOrigRatio, fullMoves, videoWidth, videoHeight, unixCDDB, defaultSpeed, visualPriority, audioDialogX, audioDialogY, eqX, eqY, eqVisible;
extern int videoW, videoFrameLeft, videoFrameTop, videoTop, videoLeft, startFullScreen, visualStartFullScreen, scrW, scrH, resizing, currentFile, descending, sortedCol, lastStartPos;
extern int subtMaxDelay, useOverlayMixer, subtAutoLoad, subtOpaque, gapless, crossfade, audioSaver, screenSaver1, cdNext, cdUseOrder, Nequalizer, randomPlay;
extern int visualLeft, visualTop, visualW, visualH, port, noMsg, audioFilter, dsBufferSize, dsPreBuffer, dsUseNotif, dsNnotif, dsSleep, dsSeekDelay, dsEq, noSort;
extern int cdMethod, cdBigEndian, cdSwapLR, cdSpeed, cdReadSectors, cdTextOn, cdda, iso88591, notMpgOverlay, autoInfo, hideMouse, cdPeriodicSpeed, cdSpeedPeriod, dsCheckUnderrun;
extern LONGLONG position, posCorrection;
extern DWORD cdStartTime, wavFmtSize;
extern Darray<Titem> playlist;
extern HWND hWin, dlgWin, videoWnd, visualWnd, listbox, header, toolbar, visualDialog, audioDialog, equalizerWnd, trackbar, DDColorWnd;
extern WNDPROC listWndProc;
extern HACCEL haccel;
extern HANDLE wavFile, paintThread, infoThread;
extern CRITICAL_SECTION critSect, infoCritSect;
extern MCIDEVICEID cdDevice;
extern TCHAR outRender[128], visualDLL[64];
extern TCHAR *propertyActive, *gaplessFile;
extern TCHAR *visualNames[MAXVISUALS], *tagNames[MTAGS];
extern TCHAR cddbFolder[256], eqFreqStr[128], subtExt[SUBTEXT_LEN];
extern char *genres[126];
extern TeqParam eqParam[Mequalizer];
extern short colWidth[Mcolumns];
extern TCHAR fmtTitle[128], *winTitle;
extern TCHAR columnStr[512];
extern TCHAR savedList[MAX_PATH];
extern TCHAR listExt[LISTEXT_LEN];
//------------------------------------------------------------------
int dtprintf(Darray<char> &buf, char *fmt, ...);
int dtprintf(Darray<WCHAR> &buf, WCHAR *fmt, ...);
TCHAR *cutExt(TCHAR *fn);
char *dupStr(char *s);
WCHAR *dupStr(WCHAR *s);
bool multiMonitors();
void checkMenuItem(UINT cmd, int check);
void gotoFile(int which);
void gotoFileN();
int addFile1(TCHAR *fn);
void addFile(TCHAR *fn);
void openDir(TCHAR *dir);
void initList();
void removeAll();
void checkSizeMenu();
void checkRateMenu();
void checkSoundMenu();
void invalidateList();
void invalidateTime();
void invalidateItem(int i);
void grab();
void snapWnd1(RECT *rc, RECT *d);
void snapWnd2(RECT *rc, HWND w, RECT *d);
void hideWinOnFull();
void writeini1();
DWORD getVer();
void printTime(TCHAR *buf, LONGLONG t);
void repaintTime(HDC hdc);
int msglng1(int btn, int id, char *text, ...);
void ensureVisible();
void incrVolume(int incr);
void controlVolume(int value);
void setMouse();

//playctrl
void PlayClip();
void CloseClip();
void nextClip();
HRESULT renderFile(IGraphBuilder *pGB, TCHAR *fileName);
void setRate(double rate);
void modifyRate(double incr);
void StepFrame();
void pause();
void fadeOut(int b);
void restart();
void seekRel(int sec);
bool seek(LONGLONG pos);
bool getPosition();
void ratio(int rx, int ry);
void resizeVideo(int nMultiplier, int nDivider);
void resizeVideo(int newWidth);
void blackSides(HDC dc);
void setFullScreen(int b);
void sizeChanged();
void showVisual();
void mouseMoveFull();
bool onVideoMoved();
void playBtn(int cmd);
void beginPlay();
void HandleGraphEvent();
void filterProperty(UINT id);
void enumFilters();
void enumAudioRenderers(int action, void *param);
void allInfo();
void switchAudioStream();
void fileInfo(int which);
HRESULT connectFilter(IBaseFilter *pOvM, WCHAR *toWhom);
void releaseGB(IGraphBuilder *&g);
bool processMessages();

//vsfilter
LRESULT CALLBACK visualWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void addVisual();
void freeVisual();
void nextVisual();
void visualProperty(WPARAM activ);
void visualInit();
void visualShut();
void visualMenu();
void findDLL();
void checkVisualMenu();
void visualSetPriority();

//sound
void audioProperty(WPARAM activ);
LRESULT CALLBACK AudioFilterProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EqualizerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void showEqualizer();
void parseEqualizerFreq();

//cd
bool isCDA(Titem *item);
bool cdPos();
MCIERROR cdPlay(DWORD from);
int openCD(TCHAR d);
void cdErr(int e);
void closeCD();
void playCD();
void stopCD();
void updateCD();
void cdInfo(Darray<TCHAR> &buf);
int getCDtrack();
int getCDtrack(Titem *item);
void cpStr(char *&dest, char *src);
void cpStr(WCHAR *&dest, WCHAR *src);
void delStr(TCHAR *&s);
void cdEditInfo();

//aspi
int ripStart();
bool ripPos();
bool ripSeek(LONG frame);
void ripShut();

//tag
int getTags(Titem *item, TCHAR **t);
bool mp3Duration(Titem *item);

//winamp
void waNext();

//dvd
bool isIFO(Titem *item);

//playlist
void saveList(TCHAR *fn);
void wrFileFilter(TCHAR *dst, TCHAR *src);
void initColumns();
void drop(HDROP drop);
void sortList();
void addFileArg(TCHAR *fn);
bool getSel(int i);
void setSel(int i, bool sel);
void removeCurrent();
void openSavePlaylist(bool save);
void removeSelected(bool crop);
void removePrev();
void reverseList();
void invertList();
void shuffle(int from, int len);
LRESULT CALLBACK listProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP);
void openList(TCHAR *fn);
void drawItem(DRAWITEMSTRUCT *lpdis);
void openFiles();

//draw
void showDDColor();
LRESULT CALLBACK DDColorProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
