# Microsoft Developer Studio Project File - Name="oscilosc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=oscilosc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "oscilosc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "oscilosc.mak" CFG="oscilosc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "oscilosc - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "oscilosc - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "oscilosc - Win32 Release Unicode" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "oscilosc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "OSCILOSC_EXPORTS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /W4 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "OSCILOSC_EXPORTS" /YX"stdio.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x405 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 Msvcrt.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib /nologo /base:"0x10100000" /entry:"DllMain" /dll /machine:I386 /nodefaultlib /out:"..\..\oscilosc.dll"
# SUBTRACT LINK32 /map
# Begin Special Build Tool
TargetPath=\_Petr\CW\tinyplay\oscilosc.dll
SOURCE="$(InputPath)"
PostBuild_Cmds=c:\programy\upx\upx -q $(TargetPath)
# End Special Build Tool

!ELSEIF  "$(CFG)" == "oscilosc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "OSCILOSC_EXPORTS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /W4 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "OSCILOSC_EXPORTS" /YX"stdio.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x405 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 strmiids.lib quartz.lib winmm.lib uuid.lib Msvcrt.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib /nologo /base:"0x10100000" /entry:"DllMain" /dll /map /debug /machine:I386 /nodefaultlib /out:"..\..\oscilosc.dll" /pdbtype:sept

!ELSEIF  "$(CFG)" == "oscilosc - Win32 Release Unicode"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "oscilosc___Win32_Release_Unicode"
# PROP BASE Intermediate_Dir "oscilosc___Win32_Release_Unicode"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "oscilosc___Win32_Release_Unicode"
# PROP Intermediate_Dir "oscilosc___Win32_Release_Unicode"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W4 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "OSCILOSC_EXPORTS" /YX"dshow.h" /FD /c
# ADD CPP /nologo /W4 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "OSCILOSC_EXPORTS" /D "UNICODE" /D "_UNICODE" /YX"stdio.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 Msvcrt.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib odbc32.lib odbccp32.lib /nologo /entry:"DllMain" /dll /machine:I386 /nodefaultlib /out:"..\..\oscilosc.dll"
# SUBTRACT BASE LINK32 /map
# ADD LINK32 Msvcrt.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib /nologo /base:"0x10100000" /entry:"DllMain" /dll /machine:I386 /nodefaultlib /out:"..\..\oscilosc.dll"
# SUBTRACT LINK32 /map

!ENDIF 

# Begin Target

# Name "oscilosc - Win32 Release"
# Name "oscilosc - Win32 Debug"
# Name "oscilosc - Win32 Release Unicode"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\oscilosc.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\oscilosc.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\oscilosc.rc
# End Source File
# End Group
# End Target
# End Project
