# Microsoft Developer Studio Project File - Name="mysql_client_test" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysql_client_test - WinIA64 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysql_client_test_ia64.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysql_client_test_ia64.mak" CFG="mysql_client_test - WinIA64 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysql_client_test - WinIA64 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe
# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE MTL /nologo /tlb".\Release\mysql_client_test.tlb" /win32
# ADD MTL /nologo /tlb".\Release\mysql_client_test.tlb" /win32
# ADD BASE CPP /nologo /G6 /MTd /W3 /GX /Ob1 /Gy /I "../include" /I "../" /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /GF /c
# ADD CPP /nologo /G6 /MTd /W3 /GX /Ob1 /Gy /I "../include" /I "../" /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /GF /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib Ws2_32.lib /nologo /subsystem:console  /out:"..\mysql_client_test.exe" /machine:IA64
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\libmysql.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib Ws2_32.lib bufferoverflowU.lib /nologo /subsystem:console  /nodefaultlib /out:"..\mysql_client_test.exe" /machine:IA64 /machine:IA64
# SUBTRACT LINK32 /pdb:none
# Begin Target

# Name "mysql_client_test - WinIA64 Release"
# Begin Source File

SOURCE=mysql_client_test.c
DEP_CPP_MYSQL=\
	"..\include\config-netware.h"\
	"..\include\config-os2.h"\
	"..\include\config-win.h"\
	"..\include\errmsg.h"\
	"..\include\m_ctype.h"\
	"..\include\m_string.h"\
	"..\include\my_alloc.h"\
	"..\include\my_config.h"\
	"..\include\my_dbug.h"\
	"..\include\my_dir.h"\
	"..\include\my_getopt.h"\
	"..\include\my_global.h"\
	"..\include\my_list.h"\
	"..\include\my_pthread.h"\
	"..\include\my_sys.h"\
	"..\include\mysql.h"\
	"..\include\mysql_com.h"\
	"..\include\mysql_time.h"\
	"..\include\mysql_version.h"\
	"..\include\raid.h"\
	"..\include\t_ctype.h"\
	"..\include\typelib.h"\
	
# End Source File
# End Target
# End Project
