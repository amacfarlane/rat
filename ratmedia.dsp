# Microsoft Developer Studio Project File - Name="ratmedia" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=ratmedia - Win32 Debug Ipv6 Win2000
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ratmedia.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ratmedia.mak" CFG="ratmedia - Win32 Debug Ipv6 Win2000"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ratmedia - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "ratmedia - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "ratmedia - Win32 Debug Ipv6 Musica" (based on "Win32 (x86) Application")
!MESSAGE "ratmedia - Win32 Debug Ipv6 MSR" (based on "Win32 (x86) Application")
!MESSAGE "ratmedia - Win32 Debug Ipv6 Win2000" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ratmedia - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O2 /I "..\common\src" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "NEED_SNPRINTF" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 uclmm.lib uclaudio.lib uclcodec.lib uclsndfile.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /machine:I386 /libpath:"..\common\src\release" /libpath:".\release"

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ratmedia___Win32_Debug"
# PROP BASE Intermediate_Dir "ratmedia___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common\src" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /D "_WINDOWS" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "DEBUG_MEM" /D "NEED_SNPRINTF" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 uclmm.lib uclaudio.lib uclcodec.lib uclsndfile.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:".\Debug" /libpath:"..\common\src\Debug" /libpath:"..\tcl-8.0\win\Debug" /libpath:"..\tk-8.0\win\Debug"

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug Ipv6 Musica"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ratmedia___Win32_Debug_Ipv6_Musica"
# PROP BASE Intermediate_Dir "ratmedia___Win32_Debug_Ipv6_Musica"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_IPv6_Musica"
# PROP Intermediate_Dir "Debug_IPv6_Musica"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common\src" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /D "_WINDOWS" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "DEBUG_MEM" /D "NEED_SNPRINTF" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common\src" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /D "_WINDOWS" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "DEBUG_MEM" /D "NEED_SNPRINTF" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 uclaudio.lib uclcodec.lib uclsndfile.lib uclmm.lib winmm.lib wship6.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:".\Debug" /libpath:"..\common\src\Debug" /libpath:"..\tcl-8.0\win\Debug" /libpath:"..\tk-8.0\win\Debug" /libpath:"..\IPv6kit\lib"
# ADD LINK32 uclaudio.lib uclcodec.lib uclsndfile.lib uclmm.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib advapi32.lib lib44bsd.lib Resolv.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:".\Debug" /libpath:"..\tcl-8.0\win\Debug" /libpath:"..\tk-8.0\win\Debug" /libpath:"..\common\src\Debug" /libpath:"..\MUSICA\WINSOCK6"

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug Ipv6 MSR"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ratmedia___Win32_Debug_Ipv6_MSR"
# PROP BASE Intermediate_Dir "ratmedia___Win32_Debug_Ipv6_MSR"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_IPv6_MSR"
# PROP Intermediate_Dir "Debug_IPv6_MSR"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common\src" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /D "_WINDOWS" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "DEBUG_MEM" /D "NEED_SNPRINTF" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common\src" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "d:\DDK\inc" /D "_WINDOWS" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "DEBUG_MEM" /D "NEED_SNPRINTF" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 uclaudio.lib uclcodec.lib uclsndfile.lib uclmm.lib winmm.lib wship6.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:".\Debug" /libpath:"..\common\src\Debug_IPv6" /libpath:"..\tcl-8.0\win\Debug_IPv6" /libpath:"..\tk-8.0\win\Debug_IPv6" /libpath:"..\IPv6kit\lib"
# ADD LINK32 uclaudio.lib uclcodec.lib uclsndfile.lib uclmm.lib winmm.lib wship6.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:".\Debug" /libpath:"..\common\src\Debug_IPv6" /libpath:"..\tcl-8.0\win\Debug_IPv6" /libpath:"..\tk-8.0\win\Debug_IPv6" /libpath:"d:\IPv6kit\lib"

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug Ipv6 Win2000"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ratmedia___Win32_Debug_Ipv6_Win2000"
# PROP BASE Intermediate_Dir "ratmedia___Win32_Debug_Ipv6_Win2000"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_IPv6_Win2000"
# PROP Intermediate_Dir "Debug_IPv6_Win2000"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common\src" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /D "_WINDOWS" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "DEBUG_MEM" /D "NEED_SNPRINTF" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common\src" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /D "DEBUG_MEM" /D "WIN2K_IPV6" /D "_WINDOWS" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "NEED_SNPRINTF" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 uclaudio.lib uclcodec.lib uclsndfile.lib uclmm.lib winmm.lib wship6.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:".\Debug" /libpath:"..\common\src\Debug_IPv6" /libpath:"..\tcl-8.0\win\Debug_IPv6" /libpath:"..\tk-8.0\win\Debug_IPv6" /libpath:"..\IPv6kit\lib"
# ADD LINK32 uclaudio.lib uclcodec.lib uclsndfile.lib uclmm.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:".\Debug" /libpath:"..\common\src\Debug_IPv6" /libpath:"..\tcl-8.0\win\Debug_IPv6" /libpath:"..\tk-8.0\win\Debug_IPv6" /libpath:"..\IPv6kit\lib"

!ENDIF 

# Begin Target

# Name "ratmedia - Win32 Release"
# Name "ratmedia - Win32 Debug"
# Name "ratmedia - Win32 Debug Ipv6 Musica"
# Name "ratmedia - Win32 Debug Ipv6 MSR"
# Name "ratmedia - Win32 Debug Ipv6 Win2000"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\auddev_trans.c
# End Source File
# Begin Source File

SOURCE=.\audio.c
# End Source File
# Begin Source File

SOURCE=.\cc_layered.c
# End Source File
# Begin Source File

SOURCE=.\cc_rdncy.c
# End Source File
# Begin Source File

SOURCE=.\cc_vanilla.c
# End Source File
# Begin Source File

SOURCE=.\channel.c
# End Source File
# Begin Source File

SOURCE=.\channel_types.c
# End Source File
# Begin Source File

SOURCE=.\cushion.c
# End Source File
# Begin Source File

SOURCE=.\fatal_error.c
# End Source File
# Begin Source File

SOURCE=.\main_engine.c
# End Source File
# Begin Source File

SOURCE=.\mbus_engine.c

!IF  "$(CFG)" == "ratmedia - Win32 Release"

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug"

# ADD CPP /W3

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug Ipv6 Musica"

# ADD BASE CPP /W3
# ADD CPP /W3

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug Ipv6 MSR"

# ADD BASE CPP /W3
# ADD CPP /W3

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug Ipv6 Win2000"

# ADD BASE CPP /W3
# ADD CPP /W3

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\mix.c
# End Source File
# Begin Source File

SOURCE=.\net.c
# End Source File
# Begin Source File

SOURCE=.\parameters.c
# End Source File
# Begin Source File

SOURCE=.\pdb.c
# End Source File
# Begin Source File

SOURCE=.\pktbuf.c
# End Source File
# Begin Source File

SOURCE=.\playout.c
# End Source File
# Begin Source File

SOURCE=.\playout_calc.c
# End Source File
# Begin Source File

SOURCE=.\render_3D.c
# End Source File
# Begin Source File

SOURCE=.\repair.c
# End Source File
# Begin Source File

SOURCE=.\rtp_callback.c
# End Source File
# Begin Source File

SOURCE=.\rtp_dump.c
# End Source File
# Begin Source File

SOURCE=.\session.c
# End Source File
# Begin Source File

SOURCE=.\settings.c
# End Source File
# Begin Source File

SOURCE=.\source.c
# End Source File
# Begin Source File

SOURCE=.\tonegen.c
# End Source File
# Begin Source File

SOURCE=.\transcoder.c
# End Source File
# Begin Source File

SOURCE=.\transmit.c
# End Source File
# Begin Source File

SOURCE=.\ts.c
# End Source File
# Begin Source File

SOURCE=.\ui_send_audio.c
# End Source File
# Begin Source File

SOURCE=.\ui_send_prefs.c
# End Source File
# Begin Source File

SOURCE=.\ui_send_rtp.c
# End Source File
# Begin Source File

SOURCE=.\ui_send_stats.c
# End Source File
# Begin Source File

SOURCE=.\usleep.c
# End Source File
# Begin Source File

SOURCE=.\voxlet.c
# End Source File
# Begin Source File

SOURCE=.\win32.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\auddev_trans.h
# End Source File
# Begin Source File

SOURCE=.\audio.h
# End Source File
# Begin Source File

SOURCE=.\audio_types.h
# End Source File
# Begin Source File

SOURCE=.\cc_layered.h
# End Source File
# Begin Source File

SOURCE=.\cc_rdncy.h
# End Source File
# Begin Source File

SOURCE=.\cc_vanilla.h
# End Source File
# Begin Source File

SOURCE=.\channel.h
# End Source File
# Begin Source File

SOURCE=.\channel_types.h
# End Source File
# Begin Source File

SOURCE=.\config_unix.h
# End Source File
# Begin Source File

SOURCE=.\config_win32.h
# End Source File
# Begin Source File

SOURCE=.\crypt.h
# End Source File
# Begin Source File

SOURCE=.\crypt_global.h
# End Source File
# Begin Source File

SOURCE=.\crypt_random.h
# End Source File
# Begin Source File

SOURCE=.\cushion.h
# End Source File
# Begin Source File

SOURCE=.\mbus_engine.h
# End Source File
# Begin Source File

SOURCE=.\mix.h
# End Source File
# Begin Source File

SOURCE=.\net.h
# End Source File
# Begin Source File

SOURCE=.\parameters.h
# End Source File
# Begin Source File

SOURCE=.\pckt_queue.h
# End Source File
# Begin Source File

SOURCE=.\pdb.h
# End Source File
# Begin Source File

SOURCE=.\pktbuf.h
# End Source File
# Begin Source File

SOURCE=.\playout.h
# End Source File
# Begin Source File

SOURCE=.\playout_calc.h
# End Source File
# Begin Source File

SOURCE=.\render_3D.h
# End Source File
# Begin Source File

SOURCE=.\repair.h
# End Source File
# Begin Source File

SOURCE=.\rtcp.h
# End Source File
# Begin Source File

SOURCE=.\rtcp_db.h
# End Source File
# Begin Source File

SOURCE=.\rtcp_pckt.h
# End Source File
# Begin Source File

SOURCE=.\rtp_callback.h
# End Source File
# Begin Source File

SOURCE=.\rtp_dump.h
# End Source File
# Begin Source File

SOURCE=.\session.h
# End Source File
# Begin Source File

SOURCE=.\settings.h
# End Source File
# Begin Source File

SOURCE=.\source.h
# End Source File
# Begin Source File

SOURCE=.\statistics.h
# End Source File
# Begin Source File

SOURCE=.\tonegen.h
# End Source File
# Begin Source File

SOURCE=.\transcoder.h
# End Source File
# Begin Source File

SOURCE=.\transmit.h
# End Source File
# Begin Source File

SOURCE=.\ts.h
# End Source File
# Begin Source File

SOURCE=.\ui.h
# End Source File
# Begin Source File

SOURCE=.\ui_send_audio.h
# End Source File
# Begin Source File

SOURCE=.\ui_send_prefs.h
# End Source File
# Begin Source File

SOURCE=.\ui_send_rtp.h
# End Source File
# Begin Source File

SOURCE=.\ui_send_stats.h
# End Source File
# Begin Source File

SOURCE=.\usleep.h
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# Begin Source File

SOURCE=.\voxlet.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\VERSION

!IF  "$(CFG)" == "ratmedia - Win32 Release"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\VERSION

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\VERSION

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug Ipv6 Musica"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\VERSION

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug Ipv6 MSR"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\VERSION

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ratmedia - Win32 Debug Ipv6 Win2000"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\VERSION

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
