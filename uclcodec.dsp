# Microsoft Developer Studio Project File - Name="uclcodec" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=uclcodec - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "uclcodec.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "uclcodec.mak" CFG="uclcodec - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "uclcodec - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "uclcodec - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\common\src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "uclcodec___Win32_Debug"
# PROP BASE Intermediate_Dir "uclcodec___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common\src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "SASR" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "uclcodec - Win32 Release"
# Name "uclcodec - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\bitstream.c
# End Source File
# Begin Source File

SOURCE=.\codec.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_dvi.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_g711.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_g726.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_gsm.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_l16.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_l8.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_lpc.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_state.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_types.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_vdvi.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\codec_wbs.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\convert_acm.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\convert_extra.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\convert_linear.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\convert_sinc.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\convert_util.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\converter.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_dvi.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_g726.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_g726_16.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_g726_24.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_g726_32.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_g726_40.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_gsm.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_lpc.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_vdvi.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cx_wbs.c

!IF  "$(CFG)" == "uclcodec - Win32 Release"

# ADD CPP /D "SASR"

!ELSEIF  "$(CFG)" == "uclcodec - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\bitstream.h
# End Source File
# Begin Source File

SOURCE=.\codec.h
# End Source File
# Begin Source File

SOURCE=.\codec_acm.h
# End Source File
# Begin Source File

SOURCE=.\codec_dvi.h
# End Source File
# Begin Source File

SOURCE=.\codec_g711.h
# End Source File
# Begin Source File

SOURCE=.\codec_g726.h
# End Source File
# Begin Source File

SOURCE=.\codec_gsm.h
# End Source File
# Begin Source File

SOURCE=.\codec_l16.h
# End Source File
# Begin Source File

SOURCE=.\codec_l8.h
# End Source File
# Begin Source File

SOURCE=.\codec_lpc.h
# End Source File
# Begin Source File

SOURCE=.\codec_state.h
# End Source File
# Begin Source File

SOURCE=.\codec_types.h
# End Source File
# Begin Source File

SOURCE=.\codec_vdvi.h
# End Source File
# Begin Source File

SOURCE=.\codec_wbs.h
# End Source File
# Begin Source File

SOURCE=.\convert_acm.h
# End Source File
# Begin Source File

SOURCE=.\convert_extra.h
# End Source File
# Begin Source File

SOURCE=.\convert_linear.h
# End Source File
# Begin Source File

SOURCE=.\convert_sinc.h
# End Source File
# Begin Source File

SOURCE=.\convert_util.h
# End Source File
# Begin Source File

SOURCE=.\converter.h
# End Source File
# Begin Source File

SOURCE=.\converter_types.h
# End Source File
# Begin Source File

SOURCE=.\cx_dvi.h
# End Source File
# Begin Source File

SOURCE=.\cx_g726.h
# End Source File
# Begin Source File

SOURCE=.\cx_gsm.h
# End Source File
# Begin Source File

SOURCE=.\cx_lpc.h
# End Source File
# Begin Source File

SOURCE=.\cx_vdvi.h
# End Source File
# Begin Source File

SOURCE=.\cx_wbs.h
# End Source File
# End Group
# End Target
# End Project
