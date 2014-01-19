/*
	(C) 2004  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "assocext.h"

static TCHAR *execmd;
static DWORD execmdLen, regclassLen, regcmdLen;

//scan list of extensions separated by semicolon
bool scanExt(TCHAR *list, bool(*f)(TCHAR*, TCHAR*), TCHAR *param)
{
	TCHAR *s, *b, c;

	for(s=list; *s; s++){
		for(b=s;; s++){
			c=*s;
			if(c==';' || c==0) break;
		}
		*s=0;
		if(f ? f(b, param) : !_tcsicmp(b, param)){
			*s=c;
			return true;
		}
		if(!c) break;
		*s=';';
	}
	return false;
}

bool extSupported(TCHAR *ext)
{
	return scanExt(extensions, 0, ext);
}

bool assocCommand(TCHAR *name)
{
	HKEY root, shell, tinyplay, command;
	bool result=false;

	if(RegCreateKey(HKEY_CLASSES_ROOT, name, &root)==ERROR_SUCCESS){
		if(RegCreateKey(root, _T("shell"), &shell)==ERROR_SUCCESS){
			if(RegCreateKey(shell, REGCMDNAME, &tinyplay)==ERROR_SUCCESS){
				if(RegCreateKey(tinyplay, _T("command"), &command)==ERROR_SUCCESS){
					if(RegSetValueEx(command, 0, 0, REG_SZ, (BYTE *)execmd, execmdLen)==ERROR_SUCCESS){
						RegSetValueEx(shell, 0, 0, REG_SZ, (BYTE *)REGCMDNAME, regcmdLen);
						result=true;
					}
					RegCloseKey(command);
				}
				RegCloseKey(tinyplay);
			}
			RegCloseKey(shell);
		}
		RegCloseKey(root);
	}
	return result;
}


void deassocCommand(TCHAR *name)
{
	HKEY root, shell, tinyplay;
	if(RegOpenKeyEx(HKEY_CLASSES_ROOT, name, 0, KEY_ALL_ACCESS, &root)==ERROR_SUCCESS){
		if(RegOpenKeyEx(root, _T("shell"), 0, KEY_ALL_ACCESS, &shell)==ERROR_SUCCESS){
			if(RegOpenKeyEx(shell, REGCMDNAME, 0, KEY_ALL_ACCESS, &tinyplay)==ERROR_SUCCESS){
				RegDeleteKey(tinyplay, _T("command"));
				RegCloseKey(tinyplay);
				RegDeleteKey(shell, REGCMDNAME);
				RegSetValueEx(shell, 0, 0, REG_SZ, (BYTE *)_T(""), sizeof(TCHAR));
			}
			RegCloseKey(shell);
		}
		RegCloseKey(root);
	}
}

bool associate(TCHAR *ext0, TCHAR *)
{
	TCHAR ext[16];
	TCHAR name[80];
	DWORD d;
	HKEY extkey;

	ext[0]='.';
	lstrcpyn(ext+1, ext0, sizeA(ext)-1);
	if(RegCreateKey(HKEY_CLASSES_ROOT, ext, &extkey)==ERROR_SUCCESS){
		d=sizeof(name);
		if(RegQueryValueEx(extkey, 0, 0, 0, (BYTE *)name, &d)!=ERROR_SUCCESS
			|| d==0 || !*name || !assocCommand(name)){
			RegSetValueEx(extkey, 0, 0, REG_SZ, (BYTE*)REGCLASSESNAME, regclassLen);
		}
		RegCloseKey(extkey);
	}

	lstrcpy(name, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\."));
	lstrcpy(name+61, ext0);
	if(RegOpenKeyEx(HKEY_CURRENT_USER, name, 0, KEY_SET_VALUE, &extkey)==ERROR_SUCCESS){
		RegDeleteValue(extkey, _T("Application"));
		RegCloseKey(extkey);
	}
	return false;
}

bool deassociate(TCHAR *ext0, TCHAR *)
{
	TCHAR ext[16];
	TCHAR name[64];
	DWORD d;
	HKEY extkey;

	ext[0]='.';
	lstrcpyn(ext+1, ext0, sizeA(ext)-1);
	if(RegOpenKeyEx(HKEY_CLASSES_ROOT, ext, 0, KEY_ALL_ACCESS, &extkey)==ERROR_SUCCESS){
		d=sizeof(name);
		if(RegQueryValueEx(extkey, 0, 0, 0, (BYTE *)name, &d)==ERROR_SUCCESS &&
				!_tcscmp(name, REGCLASSESNAME)){
			RegCloseKey(extkey);
			RegDeleteKey(HKEY_CLASSES_ROOT, ext);
		}
		else{
			RegCloseKey(extkey);
			deassocCommand(name);
		}
	}
	return false;
}

bool checkExt(TCHAR *ext, TCHAR *assoclist)
{
	if(!scanExt(assoclist, 0, ext)) deassociate(ext, 0);
	return false;
}

void associateFiles(TCHAR *newList, TCHAR *oldList)
{
	execmd= new TCHAR[MAX_PATH+8];
	execmd[0]='"';
	GetModuleFileName(0, execmd+1, MAX_PATH);
	lstrcat(execmd, _T("\" \"%L\""));
	execmdLen= (lstrlen(execmd)+1)*sizeof(TCHAR);
	regclassLen= (lstrlen(REGCLASSESNAME)+1)*sizeof(TCHAR);
	regcmdLen= (lstrlen(REGCMDNAME)+1)*sizeof(TCHAR);
	assocCommand(REGCLASSESNAME);
	//delete old
	scanExt(oldList, checkExt, newList);
	//associate new
	scanExt(newList, associate, 0);
	//icon
	HKEY root;
	if(RegCreateKey(HKEY_CLASSES_ROOT, REGCLASSESNAME, &root)==ERROR_SUCCESS){
		HKEY icon;
		if(RegCreateKey(root, _T("DefaultIcon"), &icon)==ERROR_SUCCESS){
			execmdLen -= 5*sizeof(TCHAR);
			execmd[(execmdLen-1)/sizeof(TCHAR)]=0;
			RegSetValueEx(icon, 0, 0, REG_SZ, (BYTE *)execmd, execmdLen);
		}
	}
	delete[] execmd;
}

void deleteAssoc()
{
	HKEY key;
	scanExt(associated, deassociate, 0);
	deassocCommand(REGCLASSESNAME);
	if(RegOpenKeyEx(HKEY_CLASSES_ROOT, REGCLASSESNAME, 0, KEY_ALL_ACCESS, &key)==ERROR_SUCCESS){
		RegDeleteKey(key, _T("shell"));
		RegCloseKey(key);
		RegDeleteKey(HKEY_CLASSES_ROOT, REGCLASSESNAME);
	}
}
