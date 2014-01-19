/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#define MENU_Title 0
#define MENU_Chapter (MENU_Title+1)
#define MENU_Angle 11
#define MENU_Audio (MENU_Angle+1)
#define MENU_MenuLanguage (MENU_Angle+2)
#define MENU_SubPicture (MENU_Angle+3)

#define dvdFirstPlayName _T("VIDEO_TS.ifo")

LRESULT CALLBACK dvdWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK dvdWndMainProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

HRESULT dvdRender(IGraphBuilder *pGB, TCHAR *fileName);
bool dvdRate(double rate);
void updateDVD();
void dvdHiliteTitle();
void dvdClose();
bool dvdDuration(LONGLONG *duration);
bool dvdSeek(LONGLONG pos);
void dvdMenu(DVD_MENU_ID which);
void EnableAngleMenu(BOOL bEnable);
void ResetRate();
void SetDVDPlaybackOptions();
LRESULT OnDvdEvent(LONG lEvent, LONG lParam1, LONG lParam2);
bool dvdItem(Titem *item);
void dvdRestore();

void ClearSubmenu(HMENU hMenu);
void EnableSubpicture();
void GetSubpictureInfo();
void GetAudioInfo();
void GetAngleInfo();
void GetTitleInfo();
void GetMenuLanguageInfo();

void UpdateAudioInfo();
void UpdateAngleInfo();
void UpdateSubpictureInfo();
void UpdateCurrentTitle();
void createChapterMenu();

void SetAudioStream(int nStream);
void SetAngle(int nAngle);
void SetTitle(int nTitle);
void SetChapter(int nChapter);
void SetMenuLanguage(int nLanguageIndex);
void SetSubpictureStream(int nStream);

extern HMENU menuDVD;
extern HMENU ghTitleMenu, ghChapterMenu;
extern HMENU ghAngleMenu, ghAudioMenu, ghMenuLanguageMenu;
extern HMENU ghSubpictureMenu;

extern IDvdControl2     *pDvdControl;
extern IDvdInfo2        *pDvdInfo;
extern IAMLine21Decoder *pLine21Dec;
extern LCID             *g_pLanguageList;

extern int g_ulCurTitle, g_ulNumTitles, noChangeTitle;
extern bool g_bStillOn, g_bMenuOn;
extern BOOL g_bDisplayCC, g_bDisplaySubpicture;
extern TCHAR *dvdPath;
