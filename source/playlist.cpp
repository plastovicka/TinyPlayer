/*
	(C) 2004-2012  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include <stdlib.h>

#include "assocext.h"
#include "tinyplay.h"
#include "xml.h"
#include "dvd.h"
//------------------------------------------------------------------

const int wheelVolumeIncr=5;

static TCHAR *colNames[]={_T(""), _T("File name"), _T("Type"),
_T("Bitrate"), _T("Length"), _T("Path"), _T("Size")};

struct Tcol{
	TCHAR name[3];
	int tag;
	TCHAR *Titem::*ptr;
	bool align;
} columnsTab[]={
	{_T("nu"), 0, 0, true},
	{_T("fi"), 1, 0, true},
	{_T("ex"), 2, 0, false},
	{_T("bi"), 3, 0, false},
	{_T("le"), 4, 0, false},
	{_T("pa"), 5, 0, true},
	{_T("si"), 6, 0, false},

	{_T("ti"), T_TITLE, &Titem::title, true},
	{_T("ar"), T_ARTIST, &Titem::artist, true},
	{_T("al"), T_ALBUM, &Titem::album, true},
	{_T("tr"), T_TRACK, &Titem::track, false},
	{_T("ye"), T_YEAR, &Titem::year, false},
	{_T("co"), T_COMMENT, &Titem::comment, true},
	{_T("ge"), T_GENRE, &Titem::genre, true},
	{_T("ta"), T_TAG, &Titem::tag, false},
};

bool titleDirty;
TCHAR *noExt=_T("");
TCHAR *columns[Mcolumns];

//------------------------------------------------------------------
void Titem::calcBitrate()
{
	if(length){
		bitrate= int(size*8/(length/10000));
		if(!bitrate && size>0) bitrate++;
	}
}

void Titem::getSize()
{
	if(!isIFO(this) && !isCDA(this)){
		LONGLONG oldSize = size;
		WIN32_FIND_DATA fd;
		HANDLE f= FindFirstFile(file, &fd);
		if(f!=INVALID_HANDLE_VALUE){
			size = (((LONGLONG)fd.nFileSizeHigh)<<32) + fd.nFileSizeLow;
			FindClose(f);
			if(size!=oldSize) length=0; //file has changed, new length must be calculated
		}
	}
}

void Titem::freeTags()
{
	delStr(title);
	delStr(artist);
	delStr(album);
	delStr(track);
	delStr(year);
	delStr(comment);
	delStr(genre);
	delStr(tag);
}

void Titem::free()
{
	freeTags();
	delete[] file;
	if(file==gaplessFile) gaplessFile=0;
}

void Titem::init(TCHAR *fn, int len)
{
	if(!len) len=_tcslen(fn)+1;
	file= new TCHAR[len];
	lstrcpy(file, fn);
	name= cutPath(file);
	TCHAR *e= cutExt(name);
	ext= e ? e+1 : noExt;
	title=artist=album=track=year=comment=genre=tag=0;
	size=length=0;
	bitrate=0;
}

void invalidateTitle()
{
	titleDirty=true;
	if(IsIconic(hWin)) PostMessage(listbox, WM_PAINT, 0, 0);
}

void invalidateItem(int i)
{
	RECT rc;
	if(i==currentFile) invalidateTitle();
	SendMessage(listbox, LB_GETITEMRECT, i, (LPARAM)&rc);
	InvalidateRect(listbox, &rc, FALSE);
}

void invalidateList()
{
	invalidateTitle();
	InvalidateRect(listbox, 0, FALSE);
}

void initList()
{
	SendMessage(listbox, LB_SETCOUNT, playlist.len, 0);
	invalidateList();
}

void ensureVisible()
{
	RECT rc;
	GetClientRect(listbox, &rc);
	int i= SendMessage(listbox, LB_GETITEMHEIGHT, 0, 0);
	if(!i) return;
	int n = rc.bottom/i;
	int top = SendMessage(listbox, LB_GETTOPINDEX, 0, 0);
	if(currentFile>=top+n || currentFile<top){
		top=currentFile-(n/2);
		if(top<0) top=0;
		SendMessage(listbox, LB_SETTOPINDEX, top, 0);
	}
}

void mvStr(TCHAR *&dest, TCHAR *&src)
{
	delete[] dest;
	dest=src;
	src=0;
}

void getTag(Titem *item)
{
	if(item->tag || isCDA(item) || item->size==0) return;

	TCHAR *t[MTAGS];
	memset(t, 0, sizeof(t));
	int result=getTags(item, t);
	if(result){
		mvStr(item->title, t[T_TITLE]);
		mvStr(item->artist, t[T_ARTIST]);
		mvStr(item->album, t[T_ALBUM]);
		mvStr(item->track, t[T_TRACK]);
		mvStr(item->year, t[T_YEAR]);
		mvStr(item->comment, t[T_COMMENT]);
		mvStr(item->genre, t[T_GENRE]);
		mvStr(item->tag, t[T_TAG]);
	}
	else{
		cpStr(item->tag, _T(""));
	}
	for(int i=0; i<MTAGS; i++){
		delete[] t[i];
	}
}

void printCol(Darray<TCHAR> &out, Titem *item, TCHAR *fmt, bool &alignLeft)
{
	TCHAR c, *s, *d;
	int i, j, bracketPos=0;
	bool bracketEmpty=false;
	TCHAR buf[32];

	for(;;){
		c= *fmt++;
		if(c=='%'){
			for(i=0; i<sizeA(columnsTab); i++){
				const Tcol *u=&columnsTab[i];
				if(fmt[0]==u->name[0] && fmt[1]==u->name[1]){
					if(u->align) alignLeft=true;
					if(u->ptr){
						//tag column
						if(!item){
							dtprintf(out, _T("%s"), lng(u->tag+1000, tagNames[u->tag]));
						}
						else{
							getTag(item);
							s=item->*(u->ptr);
							if(s){
								dtprintf(out, _T("%s"), s);
								bracketEmpty=false;
							}
							else if(u->tag==T_TITLE){
								j=1;
								goto l2;
							}
						}
					}
					else{
						//non-tag column
						j= u->tag;
					l2:
						bracketEmpty=false;
						if(!item){
							dtprintf(out, _T("%s"), lng(650+j, colNames[j]));
						}
						else switch(j){
							case 0: //%nu
								dtprintf(out, _T(" %d"), item-(Titem*)playlist+1);
								break;
							case 1: //%fi
								if(isCDA(item) && item->title){
									dtprintf(out, _T("%s - %s"), item->track, item->title);
								}
								else if(!_tcsicmp(item->name, dvdFirstPlayName)){
									dtprintf(out, _T("DVD menu"));
								}
								else{
									if(item->ext!=noExt) *(item->ext-1)=0;
									dtprintf(out, _T("%s"), item->name);
									if(item->ext!=noExt) *(item->ext-1)='.';
								}
								break;
							case 2: //%ex
								if(item->ext!=noExt){
									dtprintf(out, _T("%s"), item->ext);
								}
								break;
							case 3: //%bi
								if(item->bitrate) dtprintf(out, _T("%d"), item->bitrate);
								break;
							case 4: //%le
								printTime(buf, item->length);
								dtprintf(out, _T("%s"), buf);
								break;
							case 5: //%pa
								c= *item->name;
								*item->name=0;
								dtprintf(out, _T("%s"), item->file);
								*item->name=c;
								break;
							case 6: //%si
								dtprintf(out, _T("%I64u"), item->size);
								break;
						}
					}
				}
			}
			while(isalpha(*fmt)) fmt++;
		}
		else if(c=='['){
			bracketPos=out.len;
			bracketEmpty=true;
		}
		else if(c==']'){
			if(bracketEmpty && item){
				out.setLen(bracketPos-1);
				*out++=0;
			}
		}
		else{
			if(c=='\\') c=*fmt++;
			if(c=='\r' || c=='\n' || c=='\0') break;
			out--;
			*out++=c;
			*out++=0;
		}
	}
	if(convertUnderscores){
		for(s=d=out;; s++, d++){
			*d=*s;
			if(!*s) break;
			if(*s=='_') *d=' ';
			else if(*s=='%' && s[1]=='2' && s[2]=='0') *d=' ', s+=2;
		}
		out-=int(s-d);
	}
}

void setWindowTitle()
{
	Darray<TCHAR> buf;
	buf.reset();
	*buf++=0;
	if(currentFile>=0 && currentFile<playlist.len){
		bool align;
		printCol(buf, &playlist[currentFile], fmtTitle, align);
	}
	if(!buf[0]){
		dtprintf(buf, winTitle);
	}
	SetWindowText(hWin, buf);
}

void initColumns()
{
	int i;

lstart:
	Ncolumns=0;
	for(TCHAR *s=columnStr; *s && Ncolumns<Mcolumns;){
		columns[Ncolumns++]=s;
		while(*s!='\r' && *s!='\n' && *s) s++;
		while(*s=='\r' || *s=='\n') s++;
	}
	if(!Ncolumns){
		_tcscpy(columnStr, _T("%nu\r\n%fi\r\n%ex\r\n%bi\r\n%le\r\n"));
		goto lstart;
	}
	for(i=Header_GetItemCount(header); i>0; i--){
		Header_DeleteItem(header, 0);
	}
	Darray<TCHAR> buf;
	for(i=0; i<Ncolumns; i++){
		bool alignLeft=false;
		buf.reset();
		*buf++=0;
		HD_ITEM hdi;
		hdi.mask = HDI_TEXT | HDI_FORMAT | HDI_WIDTH;
		printCol(buf, 0, columns[i], alignLeft);
		hdi.pszText = buf;
		hdi.cchTextMax = buf.len;
		hdi.cxy = colWidth[i];
		hdi.fmt = alignLeft ? HDF_LEFT|HDF_STRING : HDF_CENTER|HDF_STRING;
		Header_InsertItem(header, i, &hdi);
	}
}

void drawItem(DRAWITEMSTRUCT *lpdis)
{
	Titem *item= &playlist[lpdis->itemID];
	bool sel= lpdis->itemState & ODS_SELECTED;
	bool cur= (int(lpdis->itemID) == currentFile);
	//set color and fill background
	COLORREF bk= colors[cur ? clBkCurItem : (sel ? clBkSelect : clBkList)];
	HBRUSH br=CreateSolidBrush(bk);
	SetTextColor(lpdis->hDC, colors[cur ? clCurItem : (sel ? clSelect : clList)]);
	SetBkMode(lpdis->hDC, TRANSPARENT);
	FillRect(lpdis->hDC, &lpdis->rcItem, br);
	DeleteObject(br);
	//prepare rectangle
	RECT rc;
	rc.top= lpdis->rcItem.top;
	rc.bottom= lpdis->rcItem.bottom;
	rc.right=3;

	Darray<TCHAR> buf;
	for(int i=0; i<Ncolumns; i++){
		buf.reset();
		*buf++=0;
		bool alignLeft=false;
		printCol(buf, item, columns[i], alignLeft);
		rc.left=rc.right;
		rc.right+=colWidth[i];
		DrawText(lpdis->hDC, buf, (int)lstrlen(buf), &rc,
			alignLeft ? DT_LEFT|DT_END_ELLIPSIS|DT_NOPREFIX :
			DT_CENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
	}
}
//------------------------------------------------------------------
TCHAR *getCurFile()
{
	if(currentFile<0 || currentFile>=playlist.len) return 0;
	return playlist[currentFile].file;
}

void setCurFile(TCHAR *name)
{
	for(int i=0; i<playlist.len; i++){
		if(playlist[i].file==name){
			currentFile=i;
			return;
		}
	}
	currentFile=-1;
}

//randomize the list
void shuffle(int from, int len)
{
	int i, a, b;

	if(len<2) return;
	TCHAR *curFile=getCurFile();
	for(a=from+len-1; a>=from; a--){
		i= (len==2) ? 0 : 100;
		do{
			b= from + rand()*len/(RAND_MAX+1);
			i--;
		} while(b==a && i>0);
		Titem w;
		w=playlist[a]; playlist[a]=playlist[b]; playlist[b]=w;
	}
	setCurFile(curFile);
}


int __cdecl cmpPath(const void *elem1, const void *elem2)
{
	return descending*lstrcmpi(((Titem*)elem1)->file, ((Titem*)elem2)->file);
}

int __cdecl cmpName(const void *elem1, const void *elem2)
{
	return descending*lstrcmpi(((Titem*)elem1)->name, ((Titem*)elem2)->name);
}

int __cdecl cmpType(const void *elem1, const void *elem2)
{
	return descending*lstrcmpi(((Titem*)elem1)->ext, ((Titem*)elem2)->ext);
}

int __cdecl cmpBitrate(const void *elem1, const void *elem2)
{
	return descending*(((Titem*)elem2)->bitrate - ((Titem*)elem1)->bitrate);
}

int __cdecl cmpLength(const void *elem1, const void *elem2)
{
	return ((Titem*)elem1)->length < ((Titem*)elem2)->length ? descending : -descending;
}

int __cdecl cmpSize(const void *elem1, const void *elem2)
{
	return ((Titem*)elem1)->size < ((Titem*)elem2)->size ? descending : -descending;
}

static int(__cdecl *F[])(const void *, const void *)={cmpPath, cmpName, cmpType, cmpBitrate, cmpLength, cmpPath, cmpSize};


int __cdecl cmpColumn(const void *elem1, const void *elem2)
{
	TCHAR c, *fmt, *s1, *s2;
	int i, result=0;
	Titem *item1=(Titem*)elem1, *item2=(Titem*)elem2;

	fmt= columns[sortedCol];
	do{
		c= *fmt++;
		if(c=='%'){
			for(i=0; i<sizeA(columnsTab); i++){
				const Tcol *u=&columnsTab[i];
				if(fmt[0]==u->name[0] && fmt[1]==u->name[1]){
					if(u->ptr){
						getTag(item1);
						getTag(item2);
						s1=item1->*(u->ptr);
						s2=item2->*(u->ptr);
						if(!s1) s1= (u->tag==T_TITLE) ? item1->name : _T("");
						if(!s2) s2= (u->tag==T_TITLE) ? item2->name : _T("");
						result= descending*((u->tag!=T_TRACK && u->tag!=T_YEAR) ?
							_tcsicmp(s1, s2) : (_ttoi(s1)-_ttoi(s2)));
					}
					else{
						result= F[u->tag](elem1, elem2);
					}
				}
			}
			while(isalpha(*fmt)) fmt++;
		}
		else{
			if(c=='\\') c=*fmt++;
			if(c=='\r' || c=='\n' || c=='\0') break;
		}
	} while(!result);
	return result;
}

void sortRange(int from, int len)
{
	TCHAR *curFile=getCurFile();
	aminmax(sortedCol, 0, Ncolumns-1);
	qsort(&playlist[from], len, sizeof(Titem), cmpColumn);
	setCurFile(curFile);
}

//sort the list by sortedCol column
void sortList()
{
	sortRange(0, playlist.len);
}


static int dragStart = -1;
static bool dragged;

int getListItem(LPARAM coord)
{
	return LOWORD(SendMessage(listbox, LB_ITEMFROMPOINT, 0, coord));
}

void moveItem(int i, int &dist)
{
	if(getSel(i)){
		int j=i+dist;
		if(j<0){ j=0; dist=-i; }
		if(j>=playlist.len){ j=playlist.len-1; dist=j-i; }
		Titem w=playlist[i]; playlist[i]=playlist[j]; playlist[j]=w;
		setSel(i, false);
		setSel(j, true);
	}
}

void moveSelected(int dist)
{
	TCHAR *curFile=getCurFile();
	int i;

	if(dist<0){
		for(i=0; i<playlist.len; i++){
			moveItem(i, dist);
		}
	}
	else{
		for(i=playlist.len-1; i>=0; i--){
			moveItem(i, dist);
		}
	}
	setCurFile(curFile);
}

void itemDrag(LPARAM coord)
{
	int i=getListItem(coord);
	SendMessage(listbox, LB_SETANCHORINDEX, i, 0);
	if(i!=dragStart){
		dragged=true;
		moveSelected(i-dragStart);
		dragStart=i;
	}
}

//window procedure for the listbox
LRESULT CALLBACK listProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	int i;
	POINT pt;
	RECT rc;

	switch(mesg){
		case WM_LBUTTONDOWN:
			//activate playlist
			if(GetFocus()!=hWnd){
				SetFocus(hWnd);
				if(isVideo) SetWindowPos(hWin, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
			}
			//start dragging selected items
			else if(GetKeyState(VK_CONTROL)>=0 && GetKeyState(VK_SHIFT)>=0){
				i=getListItem(lP);
				if(!getSel(i)){
					setSel(-1, false);
					setSel(i, true);
				}
				dragStart=i;
				dragged=false;
				SetCapture(hWnd);
				return 0;
			}
			break;
		case WM_LBUTTONUP:
			if(dragStart>=0){
				itemDrag(lP);
				dragStart= -1;
				ReleaseCapture();
				if(!dragged){
					int i= getListItem(lP);
					if(i>0) SendMessage(listbox, LB_SELITEMRANGE, FALSE, MAKELPARAM(0, i-1));
					SendMessage(listbox, LB_SELITEMRANGE, FALSE, MAKELPARAM(i+1, 65535));
				}
			}
			break;
		case WM_MOUSEMOVE:
			if(dragStart>=0){
				itemDrag(lP);
				if(SendMessage(hWnd, LB_GETTOPINDEX, 0, 0)==dragStart && dragStart>0){
					SendMessage(hWnd, LB_SETTOPINDEX, dragStart-1, 0);
				}
				return 0;
			}
			break;
		case WM_KEYDOWN:
			if(wP==VK_ESCAPE){
				if(seeking){
					ReleaseCapture();
					seeking=false;
					repaintTime(0);
				}
			}
			if(wP==VK_RETURN) gotoFile(SendMessage(listbox, LB_GETCURSEL, 0, 0));
			break;

		case WM_MOUSEWHEEL:
			GetWindowRect(trackbar, &rc);
			pt.x=LOWORD(lP);
			pt.y=HIWORD(lP);
			if(PtInRect(&rc, pt)){
				incrVolume(short(HIWORD(wP))*wheelVolumeIncr/WHEEL_DELTA);
				return 0;
			}
			break;
		case WM_PAINT:
			if(titleDirty){
				titleDirty=false;
				setWindowTitle();
			}
			break;
	}
	return CallWindowProc((WNDPROC)listWndProc, hWnd, mesg, wP, lP);
}

//------------------------------------------------------------------
static TCHAR path[MAX_PATH];
static int pathLen;

//save the playlist to file fn
void saveList(TCHAR *fn)
{
	lstrcpy(path, fn);
	pathLen=int(cutPath(path)-path);
	HANDLE f=CreateFile(fn, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if(f==INVALID_HANDLE_VALUE){
		if(fn!=savedList) msglng(733, "Cannot create file %s", fn);
	}
	else{
		bool err=false;
		for(int i=0; i<playlist.len; i++){
			Titem *item=&playlist[i];
			DWORD len, w;
			if(item->length){
				char buf[64];
				len= sprintf(buf, "#EXTINF:%d\r\n", int(item->length/1000));
				WriteFile(f, buf, len, &w, 0);
				if(w<len) err=true;
			}
			TCHAR *b=item->file;
			len=lstrlen(item->file);
			if(!memcmp(b, path, pathLen*sizeof(TCHAR))){
				b+=pathLen;
				len-=pathLen;
			}
			convertT2A(b, name);
			len=strlen(name);
			if(len>0){
				WriteFile(f, name, len, &w, 0);
				if(w<len) err=true;
				WriteFile(f, "\r\n", 2, &w, 0);
				if(w<2) err=true;
			}
		}
		if(!CloseHandle(f)) err=true;
		if(err) msglng(734, "Write failed: %s", fn);
	}
}

//read m3u file and add files to the playlist
void parseM3U(char *content)
{
	LONGLONG length=0;
	char *s, *b;

	for(s=content;; s++){
		while(*s=='\n' || *s=='\r' || *s==' ' || *s=='\t') s++;
		if(!*s) break;
		b=s;
		while(*s!='\n' && *s!='\r') s++;
		*s=0;
		if(*b=='#'){
			if(!strncmp(b, "#EXTINF:", 8)){
				char *e;
				length=strtol(b+8, &e, 10);
				length*= (length>10000) ? (UNITS/10000) : UNITS;
			}
		}
		else{
			int err;
			convertA2T(b, name);
			if(name[1]!=':' && name[0]!='\\' && name[0]!='/'
					&& name[3]!=':' && name[4]!=':' && name[5]!=':' &&
					_tcscmp(name, dvdFirstPlayName)){
				//relative path
				lstrcpyn(path+pathLen, name, sizeA(path)-pathLen);
				name=path;
			}
			err= addFile1(name);
			if(length>999 && !err){
				Titem *item= &playlist[playlist.len-1];
				item->length= length;
				item->calcBitrate();
				length=0;
			}
		}
	}
}

class TasxParser : public TxmlFileParser {
public:
	TasxParser(FILE *f) :TxmlFileParser(f){};
	TxmlElem *newElem(TCHAR *name, TxmlElem *parent);
};

struct TasxEntry : public TxmlElem {
	TCHAR *href, *title;
	TasxEntry() :TxmlElem(2), href(0), title(0){};
	void setText(TCHAR *);
};

struct TasxRef : public TxmlElem {
	TasxRef() :TxmlElem(3){};
	void addAttr(TCHAR *name, TCHAR *value);
};

struct TwplRef : public TxmlElem {
	TwplRef() :TxmlElem(4){};
	void addAttr(TCHAR *name, TCHAR *value);
};

struct TasxTitle : public TxmlElem {
	TasxTitle() :TxmlElem(5){};
	void setText(TCHAR *s);
};

void TwplRef::addAttr(TCHAR *name, TCHAR *value)
{
	if(!_tcsicmp(name, _T("src"))){
		addFile1(value);
	}
}

void TasxRef::addAttr(TCHAR *name, TCHAR *value)
{
	if(!_tcsicmp(name, _T("href")) && parent->tag==2){
		cpStr(((TasxEntry*)parent)->href, value);
	}
}

void TasxTitle::setText(TCHAR *s)
{
	if(s[0] && parent->tag==2){
		cpStr(((TasxEntry*)parent)->title, s);
	}
}

void TasxEntry::setText(TCHAR*)
{
	if(href){
		int err=addFile1(href);
		if(!err && title){
			Titem *item= &playlist[playlist.len-1];
			cpStr(item->title, title);
		}
	}
	delete[] href;
	delete[] title;
}

TxmlElem *TasxParser::newElem(TCHAR *name, TxmlElem *parent)
{
	if(parent){
		if(!_tcsicmp(name, _T("entry"))) return new TasxEntry;
		if(!_tcsicmp(name, _T("ref"))) return new TasxRef;
		if(!_tcsicmp(name, _T("media")) || !_tcsicmp(name, _T("audio")) || !_tcsicmp(name, _T("video"))) return new TwplRef;
		if(!_tcsicmp(name, _T("title"))) return new TasxTitle;
	}
	else if(!_tcsicmp(name, _T("asx"))) asx=true;
	return 0;
}

int openAsx(TCHAR *fn)
{
	FILE *f= _tfopen(fn, _T("rt"));
	if(!f) return 1;
	removeAll();
	numAdded=0;
	TasxParser p(f);
	TxmlElem *root= p.parse();
	fclose(f);
	if(!root) return 2;
	delete root;
	initList();
	noSort--;
	gotoFileN();
	noSort++;
	return 0;
}

//open and play playlist fn
void openList(TCHAR *fn)
{
	if(!openAsx(fn)) return;
	lstrcpy(path, fn);
	pathLen=int(cutPath(path)-path);
	HANDLE f=CreateFile(fn, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if(f==INVALID_HANDLE_VALUE){
		if(fn!=savedList) msglng(730, "Cannot open file %s", fn);
	}
	else{
		DWORD len=GetFileSize(f, 0);
		if(len>10000000){
			msglng(753, "File %s is too long", fn);
		}
		else{
			char *content= new char[len+16];
			DWORD r;
			ReadFile(f, content, len, &r, 0);
			if(r<len){
				msglng(754, "Error reading file %s", fn);
			}
			else{
				content[len]='\n';
				content[len+1]=0;
				removeAll();
				numAdded=0;
				parseM3U(content);
				initList();
				if(fn!=savedList){
					noSort--;
					gotoFileN();
					noSort++;
				}
				else{
					ensureVisible();
				}
			}
			delete[] content;
		}
		CloseHandle(f);
	}
}

void wrFileFilter(TCHAR *dst, TCHAR *src)
{
	TCHAR c;

	dst += lstrlen(dst)+1;
	do{
		if(!*src){ *dst++=0; break; }
		*dst++= '*';
		*dst++= '.';
		do{ c= *dst++ = *src++; } while(c!=';' && c);
	} while(c);
	lstrcpy(dst, lng(501, _T("All files")));
	dst += lstrlen(dst)+1;
	lstrcpy(dst, _T("*.*"));
	dst[4]=0;
}

//open or save playlist
void openSavePlaylist(bool save)
{
	static OPENFILENAME ofn;
	TCHAR buf[MAX_PATH];
	TCHAR filter[LISTEXT_LEN*2+128];

	lstrcpy(filter, lng(502, _T("Playlist")));
	wrFileFilter(filter, listExt);
	*buf = 0;

	ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner = hWin;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = sizeA(buf);
	ofn.lpstrDefExt = _T("*.m3u");
	if(save){
		ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_EXPLORER;
		if(!GetSaveFileName(&ofn)) return;
	}
	else{
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_READONLY | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_EXPLORER;
		if(!GetOpenFileName(&ofn)) return;
	}
	if(save) saveList(buf);
	else openList(buf);
}

//------------------------------------------------------------------
//play item which from the list
void gotoFile(int which)
{
	if(!playlist.len) return;
	if(which<0){
		if(repeat!=1) return;
		which=playlist.len-1;
	}
	if(which>=playlist.len){
		if(repeat!=1) return;
		which=0;
	}
	static LONG lock=0;
	if(InterlockedExchange(&lock, 1)) return;

	invalidateItem(currentFile);
	invalidateItem(which);
	currentFile=which;
	beginPlay();
	invalidateTime();
	ensureVisible();
	controlVolume(-1);
	lock=0;
}

//multiple files were added to the end of playlist
void gotoFileN()
{
	if(!numAdded) return;
	int from=playlist.len-numAdded;
	lastStartPos=from;
	if(randomPlay){
		shuffle(from, numAdded);
	}
	else if(sortOnLoad>0 && !noSort){
		sortedCol=0;
		descending=1;
		sortRange(from, numAdded);
	}
	initList();
	if(GetAsyncKeyState(VK_CONTROL)>=0 && GetAsyncKeyState('Q')>=0){
		gotoFile(from);
	}
	else{
		SendMessage(listbox, LB_SETTOPINDEX, playlist.len-1, 0);
	}
	if(autoInfo) PostMessage(hWin, WM_COMMAND, ID_ALLINFO, 0);
}

//------------------------------------------------------------------
void replaceItem(int which)
{
	int n= playlist.len-1;
	playlist[n].free();
	playlist[n]=playlist[which];
	if(currentFile>=which){
		if(currentFile==which) currentFile=n;
		currentFile--;
	}
	if(lastStartPos>=which) lastStartPos--;
	for(int s=which; s<n; s++){
		playlist[s]=playlist[s+1];
	}
	playlist--;
}

int addFile1(TCHAR *fn)
{
	if(!fn || !*fn) return 1;
	static WIN32_FIND_DATA fd;
	int len=lstrlen(fn)+1;
	if(len>4 && ((fn[len-4]=='c' && fn[len-3]=='d' && fn[len-2]=='a' ||
		fn[len-4]=='i' && fn[len-3]=='f' && fn[len-2]=='o') && fn[len-5]=='.'
		|| fn[1] != ':')){
		fd.nFileSizeHigh=fd.nFileSizeLow=0;
	}
	else{
		HANDLE f= FindFirstFile(fn, &fd);
		if(f==INVALID_HANDLE_VALUE){
			if(fn[1]==':'){
				//file not found
				if(fn[2] && _tcsicmp(fn+lstrlen(fn)-3, _T("cda"))) return 2;
			}
			//network file
			fd.nFileSizeHigh=fd.nFileSizeLow=0;
		}
		else{
			FindClose(f);
			if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
				openDir(fn);
				return -1;
			}
		}
	}
	Titem *item= playlist++;
	item->init(fn, len);
	if(scanExt(listExt, 0, item->ext)){
		//playlist
		item->free();
		playlist--;
		return 3;
	}
	if(_tcsicmp(item->ext, _T("cda"))){
		item->size = (((LONGLONG)fd.nFileSizeHigh)<<32) + fd.nFileSizeLow;
	}
	numAdded++;

	//remove duplicates
	int lenN= len*sizeof(TCHAR)-(int((char*)item->name-(char*)item->file));
	TCHAR *fnN= cutPath(fn);
	TCHAR c=fnN[0];
	for(Titem *i=&playlist[playlist.len-numAdded-1]; i>=&playlist[0]; i--){
		if(i->name[0]==c && !memcmp(i->name, fnN, lenN) && !_tcscmp(i->file, fn)){
			replaceItem(i-playlist);
			break;
		}
	}
	return 0;
}

void drop(HDROP drop)
{
	TCHAR buf[MAX_PATH];
	int i, k;

	if(!drop) return;
	numAdded=0;
	k= DragQueryFile(drop, 0xFFFFFFFF, 0, 0);
	for(i=0; i<k; i++){
		DragQueryFile(drop, i, buf, sizeA(buf));
		addFile1(buf);
	}
	gotoFileN();
}

//------------------------------------------------------------------
bool getSel(int i)
{
	return SendMessage(listbox, LB_GETSEL, i, 0)>0;
}

void setSel(int i, bool sel)
{
	SendMessage(listbox, LB_SETSEL, sel ? TRUE : FALSE, i);
}

//remove selected items from the list
void removeSelected(bool crop)
{
	int s, d, top, cur;

	top = SendMessage(listbox, LB_GETTOPINDEX, 0, 0);
	cur= SendMessage(listbox, LB_GETCARETINDEX, 0, 0);
	d=0;
	for(s=0; s<playlist.len; s++){
		if(s==lastStartPos) lastStartPos=d;
		if(getSel(s) ^ crop){
			//delete
			if(s==currentFile){ CloseClip(); currentFile=d-1; }
			if(s<top) top--;
			if(s<cur) cur--;
			playlist[s].free();
		}
		else{
			//move item
			if(s==currentFile) currentFile=d;
			playlist[d++]=playlist[s];
		}
	}
	playlist.setLen(d);
	initList();
	SendMessage(listbox, LB_SETCARETINDEX, cur, 0);
	SendMessage(listbox, LB_SETTOPINDEX, top, 0);
}

void removeCurrent()
{
	int i, top, cur;

	TCHAR *s=getCurFile();
	i=currentFile;
	if(s && msglng1(MB_YESNO, 791, "Do you want to delete file %s ?", s)==IDYES){
		top = SendMessage(listbox, LB_GETTOPINDEX, 0, 0);
		cur= SendMessage(listbox, LB_GETCARETINDEX, 0, 0);
		if(i<lastStartPos) lastStartPos--;
		if(i!=currentFile){
			if(i<currentFile) currentFile--;
		}
		else if(state==ID_FILE_PLAY && i<playlist.len-1){
			gotoFile(i+1);
			currentFile--;
		}
		else{
			CloseClip();
		}
		DeleteFile(s);
		playlist[i].free();
		memcpy(&playlist[i], &playlist[i+1], (playlist.len-i-1)*sizeof(Titem));
		playlist--;
		if(currentFile>=playlist.len) currentFile--;
		initList();
		SendMessage(listbox, LB_SETCARETINDEX, cur, 0);
		SendMessage(listbox, LB_SETTOPINDEX, top, 0);
	}
}

void removePrev()
{
	int s, d;
	if(currentFile<=0) return;

	for(s=0; s<currentFile; s++){
		playlist[s].free();
	}
	d=0;
	for(; s<playlist.len; s++){
		playlist[d++]=playlist[s];
	}
	currentFile=lastStartPos=0;
	playlist.setLen(d);
	initList();
}

//invert selection
void invertList()
{
	SendMessage(listbox, WM_SETREDRAW, FALSE, 0);
	for(int i=0; i<playlist.len; i++){
		setSel(i, !getSel(i));
	}
	SendMessage(listbox, WM_SETREDRAW, TRUE, 0);
}

//reverse the list
void reverseList()
{
	int i, j, n;
	bool bi, bj;

	SendMessage(listbox, WM_SETREDRAW, FALSE, 0);
	n=playlist.len;
	for(i=(n>>1)-1; i>=0; i--){
		Titem w;
		j=n-1-i;
		w=playlist[i]; playlist[i]=playlist[j]; playlist[j]=w;
		bi=getSel(i);
		bj=getSel(j);
		setSel(i, bj);
		setSel(j, bi);
	}
	if(currentFile>=0) currentFile= n-1-currentFile;
	invalidateList();
	SendMessage(listbox, WM_SETREDRAW, TRUE, 0);
}

//clear the list
void removeAll()
{
	setSel(-1, true);
	removeSelected(false);
}

//------------------------------------------------------------------
//add and play file fn
void addFile(TCHAR *fn)
{
	numAdded=0;
	int err= addFile1(fn);
	if(err==3) openList(fn);
	else gotoFileN();
}

//open file fn (command line parameter)
void addFileArg(TCHAR *fn)
{
	if(!fn) return;
	TCHAR *fn2=(TCHAR*)_alloca(MAX_PATH*sizeof(TCHAR));
	lstrcpyn(fn2, fn, MAX_PATH);
	if(fn2[0]=='"'){
		fn2[_tcslen(fn2)-1]=0;
		fn2++;
	}
	addFile(fn2);
}

//------------------------------------------------------------------
void openFiles()
{
	static OPENFILENAME ofn;
	TCHAR *filter, *buf;

	// create a filter from the list of supported extensions
	filter= new TCHAR[sizeA(extensions)*2];
	lstrcpy(filter, lng(500, _T("Media files")));
	wrFileFilter(filter, extensions);
	// allocate large buffer for chosen files
	const int M=16384;
	buf= new TCHAR[M];
	*buf = 0;

	ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner = hWin;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = M;
	ofn.lpstrDefExt = _T("*\0");
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_READONLY | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

	if(GetOpenFileName(&ofn)){
		TCHAR *s= buf+lstrlen(buf)+1;
		if(!*s){
			//one file
			addFile(buf);
		}
		else{
			//multiple files
			TCHAR path[MAX_PATH];
			lstrcpy(path, buf);
			TCHAR *e= path+lstrlen(path);
			if(e[-1]!='\\') *e++='\\';
			numAdded=0;
			for(; *s; s+=lstrlen(s)+1){
				lstrcpy(e, s);
				addFile1(path);
			}
			gotoFileN();
		}
	}
	delete[] filter;
	delete[] buf;
}

void openDir(TCHAR *dir)
{
	HANDLE h;
	TCHAR *oldDir= (TCHAR*)_alloca(4*MAX_PATH+sizeof(WIN32_FIND_DATA));
	TCHAR *buf= oldDir+MAX_PATH;
	WIN32_FIND_DATA &fd= *(WIN32_FIND_DATA*)(buf+MAX_PATH);
	TCHAR *e;

	GetCurrentDirectory(MAX_PATH, oldDir);
	if(dir) SetCurrentDirectory(dir);
	h = FindFirstFile(_T("*.*"), &fd);
	if(h!=INVALID_HANDLE_VALUE){
		do{
			if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
				if(fd.cFileName[0]!='.'){
					openDir(fd.cFileName);
				}
			}
			else{
				e=cutExt(fd.cFileName);
				if(e && extSupported(e+1)){
					if(GetFullPathName(fd.cFileName, MAX_PATH, buf, &e)){
						addFile1(buf);
					}
				}
			}
		} while(FindNextFile(h, &fd));
		FindClose(h);
	}
	SetCurrentDirectory(oldDir);
}

//-------------------------------------------------------------------------
