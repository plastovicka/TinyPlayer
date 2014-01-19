/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#ifndef assocextH
#define assocextH

#define REGCLASSESNAME _T("TinyPlayFile")
#define REGCMDNAME _T("TinyPlayer")

#define EXTENSIONS_LEN 256
extern TCHAR extensions[EXTENSIONS_LEN];
extern TCHAR associated[EXTENSIONS_LEN];
extern TCHAR audioExt[EXTENSIONS_LEN];

bool scanExt(TCHAR *list, bool(*f)(TCHAR*, TCHAR*), TCHAR *param);
bool extSupported(TCHAR *ext);
void associateFiles(TCHAR *newList, TCHAR *oldList);
void deleteAssoc();

#define sizeA(A) (sizeof(A)/sizeof(*A))

#endif