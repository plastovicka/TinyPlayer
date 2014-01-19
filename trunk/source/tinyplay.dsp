# Microsoft Developer Studio Project File - Name="tinyplay" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=tinyplay - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "tinyplay.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "tinyplay.mak" CFG="tinyplay - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "tinyplay - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "tinyplay - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "tinyplay - Win32 Release Unicode" (based on "Win32 (x86) Application")
!MESSAGE "tinyplay - Win32 Debug Unicode" (based on "Win32 (x86) Application")
!MESSAGE "tinyplay - Win32 Debug Optimize Unicode" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "tinyplay - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /O1 /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "WIN32" /Yu"hdr.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG" /d "WIN32"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 quartz.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib advapi32.lib oleaut32.lib wsock32.lib version.lib comctl32.lib winmm.lib dxguid.lib strmiids.lib /nologo /stack:0x200000,0x20000 /subsystem:windows /machine:I386 /out:"..\tinyplay98.exe"
# SUBTRACT LINK32 /map /debug /nodefaultlib
# Begin Special Build Tool
TargetPath=\_Petr\CW\tinyplay\tinyplay98.exe
SOURCE="$(InputPath)"
PreLink_Cmds=c:\_petr\cw\hotkeyp\hotkeyp.exe -close window PlasPlayer
PostBuild_Cmds=c:\programy\upx\upx -q $(TargetPath)
# End Special Build Tool

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W4 /Gm /GX /Zi /Od /Gy /D "DEBUG" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "WIN32" /Yu"hdr.h" /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG" /d "WIN32"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 quartz.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib advapi32.lib oleaut32.lib wsock32.lib version.lib comctl32.lib winmm.lib dxguid.lib strmiids.lib /nologo /stack:0x200000,0x200000 /subsystem:windows /debug /machine:I386 /out:"..\tinyplayDBG.exe" /pdbtype:sept
# SUBTRACT LINK32 /incremental:no /map /nodefaultlib
# Begin Special Build Tool
SOURCE="$(InputPath)"
PreLink_Cmds=c:\_petr\cw\hotkeyp\hotkeyp.exe -close window PlasPlayer
# End Special Build Tool

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Release Unicode"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "tinyplay___Win32_Release_Unicode"
# PROP BASE Intermediate_Dir "tinyplay___Win32_Release_Unicode"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_Unicode"
# PROP Intermediate_Dir "Release_Unicode"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "_WINDOWS" /D "_MBCS" /D "WIN32" /D "NDEBUG" /D _WIN32_WINNT=0x400 /YX"hdr.h" /FD /c
# ADD CPP /nologo /MD /W4 /GX /O1 /D "NDEBUG" /D "UNICODE" /D "_UNICODE" /D "_WINDOWS" /D "_MBCS" /D "WIN32" /Yu"hdr.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "WIN32"
# ADD RSC /l 0x409 /d "NDEBUG" /d "WIN32"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winspool.lib odbc32.lib odbccp32.lib msvcrt.lib comctl32.lib shell32.lib quartz.lib winmm.lib msacm32.lib olepro32.lib strmiids.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib ole32.lib oleaut32.lib advapi32.lib uuid.lib /nologo /stack:0x200000,0x20000 /subsystem:windows /machine:I386 /nodefaultlib /out:"..\tinyplay.exe"
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 quartz.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib advapi32.lib oleaut32.lib wsock32.lib version.lib comctl32.lib winmm.lib dxguid.lib strmiids.lib /nologo /stack:0x200000,0x20000 /subsystem:windows /machine:I386 /out:"..\tinyplay.exe"
# SUBTRACT LINK32 /map /debug /nodefaultlib
# Begin Special Build Tool
TargetPath=\_Petr\CW\tinyplay\tinyplay.exe
SOURCE="$(InputPath)"
PreLink_Cmds=c:\_petr\cw\hotkeyp\hotkeyp.exe -close window PlasPlayer
PostBuild_Cmds=c:\programy\upx\upx -q $(TargetPath)
# End Special Build Tool

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug Unicode"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "tinyplay___Win32_Debug_Unicode"
# PROP BASE Intermediate_Dir "tinyplay___Win32_Debug_Unicode"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Debug_Unicode"
# PROP Intermediate_Dir "Debug_Unicode"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W4 /GX /O2 /D "_WINDOWS" /D "_MBCS" /D "WIN32" /D "NDEBUG" /D _WIN32_WINNT=0x400 /D "UNICODE" /D "_UNICODE" /YX"hdr.h" /FD /c
# ADD CPP /nologo /MDd /W4 /GX /ZI /Od /D "DEBUG" /D "_DEBUG" /D "UNICODE" /D "_UNICODE" /D "_WINDOWS" /D "_MBCS" /D "WIN32" /Yu"hdr.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "WIN32"
# ADD RSC /l 0x409 /d "NDEBUG" /d "WIN32"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winspool.lib odbc32.lib odbccp32.lib msvcrt.lib "C:\Program Files\Microsoft Visual Studio\DXSDK\Samples\C++\DirectShow\BaseClasses\Release\STRMBASE.lib" ddraw.lib comctl32.lib shell32.lib quartz.lib winmm.lib msacm32.lib olepro32.lib strmiids.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib ole32.lib oleaut32.lib advapi32.lib uuid.lib /nologo /stack:0x200000,0x20000 /subsystem:windows /map /machine:I386 /nodefaultlib /out:"..\tinyplayW.exe"
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 quartz.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib advapi32.lib oleaut32.lib wsock32.lib version.lib comctl32.lib winmm.lib dxguid.lib strmiids.lib /nologo /stack:0x200000,0x20000 /subsystem:windows /incremental:yes /debug /machine:I386 /out:"..\tinyplayDBGW.exe"
# SUBTRACT LINK32 /map /nodefaultlib
# Begin Special Build Tool
SOURCE="$(InputPath)"
PreLink_Cmds=c:\_petr\cw\hotkeyp\hotkeyp.exe -close window PlasPlayer
# End Special Build Tool

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug Optimize Unicode"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "tinyplay___Win32_Debug_Optimize_Unicode"
# PROP BASE Intermediate_Dir "tinyplay___Win32_Debug_Optimize_Unicode"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Debug_Optimize"
# PROP Intermediate_Dir "Debug_Optimize"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /GX /ZI /D "_WINDOWS" /D "_MBCS" /D "WIN32" /D "NDEBUG" /D _WIN32_WINNT=0x400 /D "UNICODE" /D "_UNICODE" /YX"hdr.h" /FD /c
# ADD CPP /nologo /MDd /W4 /GX /Z7 /O1 /D "NDEBUG" /D "UNICODE" /D "_UNICODE" /D "_WINDOWS" /D "_MBCS" /D "WIN32" /Yu"hdr.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "WIN32"
# ADD RSC /l 0x409 /d "NDEBUG" /d "WIN32"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 olepro32.lib oleaut32.lib wsock32.lib dxguid.lib version.lib comctl32.lib shell32.lib quartz.lib winmm.lib strmiids.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib ole32.lib advapi32.lib uuid.lib /nologo /stack:0x200000,0x20000 /subsystem:windows /incremental:yes /debug /machine:I386 /out:"..\tinyplayDBGW.exe"
# SUBTRACT BASE LINK32 /map /nodefaultlib
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib advapi32.lib oleaut32.lib wsock32.lib version.lib comctl32.lib winmm.lib dxguid.lib strmiids.lib quartz.lib /nologo /stack:0x200000,0x20000 /subsystem:windows /debug /machine:I386 /out:"..\tinyplayDBGW.exe"
# SUBTRACT LINK32 /incremental:yes /map /nodefaultlib
# Begin Special Build Tool
SOURCE="$(InputPath)"
PreLink_Cmds=c:\_petr\cw\hotkeyp\hotkeyp.exe -close window PlasPlayer
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "tinyplay - Win32 Release"
# Name "tinyplay - Win32 Debug"
# Name "tinyplay - Win32 Release Unicode"
# Name "tinyplay - Win32 Debug Unicode"
# Name "tinyplay - Win32 Debug Optimize Unicode"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\aspi.cpp

!IF  "$(CFG)" == "tinyplay - Win32 Release"

# ADD CPP /Yc"hdr.h"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Release Unicode"

# ADD CPP /Yc"hdr.h"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug Unicode"

# ADD CPP /Yu

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug Optimize Unicode"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\assocext.cpp

!IF  "$(CFG)" == "tinyplay - Win32 Release"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug"

# ADD CPP /Yc"hdr.h"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Release Unicode"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug Unicode"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug Optimize Unicode"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cd.cpp
# End Source File
# Begin Source File

SOURCE=.\draw.cpp
# End Source File
# Begin Source File

SOURCE=.\dvd.cpp
# End Source File
# Begin Source File

SOURCE=.\dvdoptions.cpp
# End Source File
# Begin Source File

SOURCE=.\filter.cpp
# End Source File
# Begin Source File

SOURCE=.\freedb.cpp
# End Source File
# Begin Source File

SOURCE=.\lang.cpp

!IF  "$(CFG)" == "tinyplay - Win32 Release"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Release Unicode"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug Unicode"

# ADD CPP /Yc"hdr.h"

!ELSEIF  "$(CFG)" == "tinyplay - Win32 Debug Optimize Unicode"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\playctrl.cpp
# End Source File
# Begin Source File

SOURCE=.\playlist.cpp
# End Source File
# Begin Source File

SOURCE=.\sound.cpp
# End Source File
# Begin Source File

SOURCE=.\tag.cpp
# End Source File
# Begin Source File

SOURCE=.\tinyplay.cpp
# End Source File
# Begin Source File

SOURCE=.\tinyplay.rc
# End Source File
# Begin Source File

SOURCE=.\vsfilter.cpp
# End Source File
# Begin Source File

SOURCE=.\winamp.cpp
# End Source File
# Begin Source File

SOURCE=.\xml.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\Aspi.h
# End Source File
# Begin Source File

SOURCE=.\assocext.h
# End Source File
# Begin Source File

SOURCE=.\cd.h
# End Source File
# Begin Source File

SOURCE=.\darray.h
# End Source File
# Begin Source File

SOURCE=.\draw.h
# End Source File
# Begin Source File

SOURCE=.\dvd.h
# End Source File
# Begin Source File

SOURCE=.\filter.h
# End Source File
# Begin Source File

SOURCE=.\hdr.h
# End Source File
# Begin Source File

SOURCE=.\lang.h
# End Source File
# Begin Source File

SOURCE=.\sound.h
# End Source File
# Begin Source File

SOURCE=.\tinyplay.h
# End Source File
# Begin Source File

SOURCE=.\util.h
# End Source File
# Begin Source File

SOURCE=.\winamp.h
# End Source File
# Begin Source File

SOURCE=.\xml.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\Playbtn.bmp
# End Source File
# Begin Source File

SOURCE=.\tinyplay.ico
# End Source File
# End Group
# End Target
# End Project
