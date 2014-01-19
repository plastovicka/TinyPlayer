/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */

#define Mtrack 101

struct Ttrack {
	DWORD start;  //start position in frames
	DWORD length; //length in frames
	TCHAR *title;
	TCHAR *extt;
};

struct TmultipeResultInfo {
	TCHAR *category;
	TCHAR *discId;
	TCHAR *description;
};

struct TcdInfo {
	int trackN2;     //number of tracks
	DWORD total;     //disc length in seconds
	int revision;    //how many times has been freedb record modified
	TCHAR *Artist, *Album, *Year, *Genre, *Extd, *Order, *Category, *SubmittedVia;
	TCHAR *Dtitle;    //used during parsing the file
	TCHAR *inetCategory; //category read from freedb server
	TCHAR *inetDiscId;   //discId read from freedb server
	int inetRevision;    //revision read from freedb server
	TCHAR discId[12]; //freedb identifier
	TCHAR mediaIdHex[16];

	Darray<TmultipeResultInfo> results;
	bool isExact;
	bool modif;

	Ttrack Tracks[Mtrack];

	int aspiCommand(int action);
	int getTOC(){ return aspiCommand(0); }
	int getCDText(){ return aspiCommand(5); }
	int getSectors(){ return aspiCommand(1); }
	int setSpeed(){ return aspiCommand(2); }
	void findInfo();
	void reset();
	void resetResult();
	void resetFields();
	void readCdplayerIni();
	void readLocalCDDB(TCHAR *mask=0);
	void saveToLocalCDDB();
	int deleteFromLocalCDDB();
	void cddbQuery();
	int getFromFreedb(bool save);
	void submit();
	void computeDiscID();
	TCHAR *getArtistAlbum();
protected:
	void parseLine(char *key, char *value);
	void parse(int(*get)(void*), void *param, TCHAR *cat);
	void parse(FILE *f, TCHAR *cat);
	void openCDDBfile(TCHAR *fn, TCHAR *cat);
	void rdFromString(char *s, TCHAR *cat);
	void wrToString(Darray<char> &s);
	TCHAR *getCategoryFolder(TCHAR *fn);
	void addResult(char *&line);
	void saveToLocalCDDB(char *content);
	int driveCmd(int action);
	int aspiCmd(int action);
	int ntscsiCmd(int action);
	int parseCDText(BYTE *buf);

	int enc;
	int trackOffset;
};

int msf2s(DWORD t);
int msf2f(DWORD t);
bool isEmpty(TCHAR *s);
void skipEol(char *&s);
void skipSpace(char *&s);
int chooseFreedbResult(Darray<TmultipeResultInfo> &r);
void cdEditInfo();
void getSites();
void parseSites(HWND list, int i);

extern int port, proxyPort, CDDBenc, useProxy, autoConnect, inetTimeout;
extern char cdLetter;
extern TCHAR servername[128];
extern TCHAR serverCgi[128];
extern TCHAR serverSubmitCgi[128];
extern TCHAR email[256];
extern TCHAR proxy[256];
extern TCHAR proxyUser[128];
extern TCHAR proxyPassword[128];
extern char *sites;
class AudioFilter;
extern TcdInfo cd;
extern bool mciPlaying;