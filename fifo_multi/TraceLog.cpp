// TraceLog.cpp: implementation of the TraceLog class.
//
//////////////////////////////////////////////////////////////////////
//#include "StdAfx.h"
#include "TraceLog.h"
#include <sstream>
#include <time.h>
#include "Log_SingleLock.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DWORD WINAPI ThreadFun_DeleteExpiredFile(LPVOID parm )
{
	Log_TraceLog* pLog = (Log_TraceLog*) parm ;
	pLog->Working();
	return 0;
}

std::string Log_TraceLog::GetLogName()
{
	return m_Logname;
}

int Log_TraceLog::GetLogLevel()
{
	return m_Level;
}

unsigned long Log_TraceLog::GetMaxSize()
{
	return m_MaxSize;
}

bool Log_TraceLog::SetMaxSize(unsigned long nSize)
{
	m_MaxSize = nSize;
	return true;
}

Log_TraceLog::Log_TraceLog(std::string fname,bool bAddDateInFileName)
{
	m_Level = 5;
	m_Logname = fname;
	
	char driver[256],dir[256],filename[256],ext[256];
	_splitpath((LPCSTR)m_Logname.c_str(),driver, dir, filename, ext); 
	
	std::string backname;
	std::stringstream stream;
	
	m_strCurDate = GetDateString();
	m_strPrefix = filename;
	
	stream << driver;
	stream << dir;
	stream << m_strPrefix;
	if(bAddDateInFileName)
	{
		stream << "-";
		stream << m_strCurDate;
	}
	stream << ext;
	
	//backname = stream.str();
	
	m_Logname = stream.str();
	
	_chmod(m_Logname.data(), S_IREAD | S_IWRITE);
	
	//rename((LPCTSTR)m_Logname.c_str(), (LPCTSTR)backname.c_str());
	
	m_Handle = fopen(m_Logname.data(),"a+");
	
	m_Thread = NULL;
}

Log_TraceLog::Log_TraceLog(std::string fname,long level,unsigned long maxsize)
{
	m_Logname = fname;
	m_Level = level;
	m_MaxSize = maxsize;

	char driver[256],dir[256],filename[256],ext[256];
	_splitpath((LPCSTR)m_Logname.c_str(),driver, dir, filename, ext); 

	std::string backname;
	std::stringstream stream;

	m_strCurDate = GetDateString();
	m_strPrefix = filename;
	
	stream << driver;
	stream << dir;
	stream << m_strPrefix;
	stream << "-";
	stream << m_strCurDate;
	stream << ext;
	
	//backname = stream.str();

	m_Logname = stream.str();
	
	_chmod(m_Logname.data(), S_IREAD | S_IWRITE);
	
	//rename((LPCTSTR)m_Logname.c_str(), (LPCTSTR)backname.c_str());

	m_Handle = fopen(m_Logname.data(),"a+");

	m_Thread = NULL;

	//m_Thread = ::CreateThread(NULL, 0, &ThreadFun_DeleteExpiredFile, this, 0, &m_ThreadID );
}

void Log_TraceLog::LogTime(long level)
{
	if(!m_Handle) 
		return;
	
	if(!Passed(level)) 
		return ;

	CLog_SingleLock SingleLock(&m_Lock);
	SingleLock.Lock();

	std::string tmpstring = GetDateTimeString();
	
	fprintf(m_Handle,"[%s]\n",(LPCTSTR) tmpstring.c_str());

	fflush(m_Handle);
}

void Log_TraceLog::TraceInfo(long level, const char *format ,...)
{
	if(!m_Handle) 
		return;
	
	if(!Passed(level)) 
		return ;
	
	CLog_SingleLock SingleLock(&m_Lock);
	SingleLock.Lock();
	
	va_list mark;
	va_start(mark,format);
	
	//std::string tmpstring = GetDateTimeString(TIMESTRING_UNNORMAL);
	//fprintf(m_Handle,"[%s]",(LPCTSTR) tmpstring.c_str());
	
	vfprintf(m_Handle ,format , mark);
	
	fprintf(m_Handle,"\n");
	
	va_end(mark);
	
	fflush(m_Handle);
	
	//struct stat buf;
	//fstat(m_Handle->_file,&buf);

	int size = _filelength(_fileno(m_Handle));
	
	std::string strDateString = GetDateString();
	
	if(strDateString != m_strCurDate)
	{
		fclose(m_Handle);
		
		char driver[256],dir[256],filename[256],ext[256];
		_splitpath((LPCSTR)m_Logname.c_str(),driver, dir, filename, ext); 
		
		std::string backname;
		std::stringstream stream;
		
		stream << driver;
		stream << dir;
		stream << m_strPrefix;
		stream << "-";
		stream << strDateString;
		stream << ext;
		
		m_Logname = stream.str();
		
		m_Handle = fopen(m_Logname.data(),"a+");
		
		m_strCurDate = strDateString;
	}
	
    if((unsigned)size > m_MaxSize)//单个日志文件超出最大日志文件大小时切换文件
	{
		fclose(m_Handle);
		
		char driver[256],dir[256],filename[256],ext[256];
		_splitpath((LPCSTR)m_Logname.c_str(),driver, dir, filename, ext); 
		
		std::string backname;
		std::stringstream stream;
		
		stream << driver;
		stream << dir;
		stream << filename;
		stream << "-";
		stream << GetTimeString();
		stream << ext;
		
		backname = stream.str();
		
		rename(m_Logname.data(), backname.data());
		
		m_Handle = fopen(m_Logname.data(),"a+");
	}
}

void Log_TraceLog::Trace(long level, const char *format ,...)
{
	if(!m_Handle) 
		return;
	
	if(!Passed(level)) 
		return ;

	CLog_SingleLock SingleLock(&m_Lock);
	SingleLock.Lock();

	va_list mark;
	va_start(mark,format);
	
	std::string tmpstring = GetDateTimeString(TIMESTRING_UNNORMAL);
	fprintf(m_Handle,"[%s]",(LPCTSTR) tmpstring.c_str());

	vfprintf(m_Handle ,format , mark);
	
	fprintf(m_Handle,"\n");

	va_end(mark);

 	fflush(m_Handle);

	//struct stat buf;
	//fstat(m_Handle->_file,&buf);

	int size = _filelength(_fileno(m_Handle));

	std::string strDateString = GetDateString();

	if(strDateString != m_strCurDate)
	{
		fclose(m_Handle);
		
		char driver[256],dir[256],filename[256],ext[256];
		_splitpath((LPCSTR)m_Logname.c_str(),driver, dir, filename, ext); 
		
		std::string backname;
		std::stringstream stream;
		
		stream << driver;
		stream << dir;
		stream << m_strPrefix;
		stream << "-";
		stream << strDateString;
		stream << ext;
		
		m_Logname = stream.str();
		
		m_Handle = fopen(m_Logname.data(),"a+");

		m_strCurDate = strDateString;
	}

    if((unsigned)size > m_MaxSize)//单个日志文件超出最大日志文件大小时切换文件
	{
		fclose(m_Handle);
		
		char driver[256],dir[256],filename[256],ext[256];
		_splitpath((LPCSTR)m_Logname.c_str(),driver, dir, filename, ext); 

		std::string backname;
		std::stringstream stream;

		stream << driver;
		stream << dir;
		stream << filename;
		stream << "-";
		stream << GetTimeString();
		stream << ext;

		backname = stream.str();

		rename(m_Logname.data(), backname.data());

		m_Handle = fopen(m_Logname.data(),"a+");
	}
}

void Log_TraceLog::TraceNoTime(long level, const char *format ,...)
{
	if(!m_Handle) 
		return;
	
	if(!Passed(level)) 
		return ;

	CLog_SingleLock SingleLock(&m_Lock);
	SingleLock.Lock();

	va_list mark;
	va_start(mark,format);

	vfprintf(m_Handle,format,mark);

	va_end(mark);
	
	fprintf(m_Handle,"\n");
	
	fflush(m_Handle);

	//struct stat buf;

	int size = _filelength(_fileno(m_Handle));
	//fstat(m_Handle->_file,&buf);

    //if(unsigned(buf.st_size) > m_MaxSize)
	if (unsigned(size) > m_MaxSize)
	{
		fclose(m_Handle);
		std::string bakName = m_Logname + ".bak";
		_unlink(bakName.data());
		rename(m_Logname.data(), bakName.data());
		m_Handle = fopen(m_Logname.data(),"w");
	}
}

Log_TraceLog::~Log_TraceLog()
{
	//TRACE("Log_TraceLog::~Log_TraceLog\n");
	::CloseHandle( m_Thread );

	Sleep( 100 );
	
	if(m_Handle)
	{
		fclose(m_Handle);
	}
	//delete this;
}

std::string Log_TraceLog::GetTimeString()
{
	SYSTEMTIME lpSystemTime;
	GetLocalTime(&lpSystemTime);
	
	int millsecond,second,minitues,hour,day,month,year;
	
	millsecond	= lpSystemTime.wMilliseconds;
	second		= lpSystemTime.wSecond;
	minitues	= lpSystemTime.wMinute;
	hour		= lpSystemTime.wHour;
	day			= lpSystemTime.wDay;
	month		= lpSystemTime.wMonth;
	year		= lpSystemTime.wYear;
	
	char tempchar[20];
	
	sprintf(tempchar,"%02d%02d%02d\0",hour,minitues,second);
	
	return std::string(tempchar);
}

std::string Log_TraceLog::GetDateString()
{
	SYSTEMTIME lpSystemTime;
	GetLocalTime(&lpSystemTime);
	
	int millsecond,second,minitues,hour,day,month,year;
	
	millsecond	= lpSystemTime.wMilliseconds;
	second		= lpSystemTime.wSecond;
	minitues	= lpSystemTime.wMinute;
	hour		= lpSystemTime.wHour;
	day			= lpSystemTime.wDay;
	month		= lpSystemTime.wMonth;
	year		= lpSystemTime.wYear;
	
	char tempchar[20];

	sprintf(tempchar,"%04d%02d%02d\0",year,month,day);
	
	return std::string(tempchar);
}

std::string Log_TraceLog::GetDateTimeString(long timestringtype)
{
	SYSTEMTIME lpSystemTime;
	GetLocalTime(&lpSystemTime);

	int millsecond,second,minitues,hour,day,month,year;
	
	millsecond	= lpSystemTime.wMilliseconds;
	second		= lpSystemTime.wSecond;
	minitues	= lpSystemTime.wMinute;
	hour		= lpSystemTime.wHour;
	day			= lpSystemTime.wDay;
	month		= lpSystemTime.wMonth;
	year		= lpSystemTime.wYear;

	char tempchar[20];

	if( timestringtype == TIMESTRING_NORMAL )
	{
		sprintf(tempchar,"%04d-%02d-%02d %02d:%02d:%02d\0",	year,month,day,hour,minitues,second );
	}
	else if( timestringtype == TIMESTRING_NORMAL2 )
	{
		sprintf(tempchar,"%04d%02d%02d%02d%02d%02d\0", year,month,day,hour,minitues,second );
	}
	else//TIMESTRING_UNNORMAL
	{
		sprintf(tempchar,"%02d-%02d %02d:%02d:%02d.%03d\0",	month,day,hour,minitues,second,millsecond );
	}

	return std::string(tempchar);
}

std::string Log_TraceLog::GetTraceLevelTypeString()
{
	std::string strRetVal;
	switch( m_Level ) 
	{
	case 1:
		strRetVal = "严重错误日志";//"严重错误日志";
		break;
	case 2:
		strRetVal = "失败日志";//"失败日志";
		break;
	case 3:
		strRetVal = "警告信息日志";//"警告信息日志";
		break;
	case 4:
		strRetVal = "一般信息日志";//"一般信息日志";
		break;
	case 5:
		strRetVal = "调试日志";//"调试日志";
		break;
	default:
		{
			std::stringstream stream;
			stream << m_Level;
			strRetVal = stream.str();
		}
		//strRetVal.Format("%d", m_Level );
		break;
	}
	return strRetVal;
}

/****************************************************************************
函数名:Working()
参数:
返回数值:
函数功能描述: 3 分钟循环一次，把修改时间与当前时间比较，超过 7 天的日志删除。
*****************************************************************************/

void Log_TraceLog::Working()
{
	char driver[256],dir[256],filename[256],ext[256];
	_splitpath((LPCSTR)m_Logname.c_str(),driver, dir, filename, ext); 

	std::string strFilePath, strFileFindPath, strFileName;
	std::stringstream stream;
	stream << driver;
	stream << dir;
	strFilePath = stream.str();
	//strFilePath.Format("%s%s", driver, dir );
	strFileFindPath = strFilePath + "*.*";

//	BOOL bIsFinded;
	time_t tmFileCreateTime;
	time_t tmCurrTime;

	while(1)
	{
		tmCurrTime = time(NULL);

		struct _finddata_t c_file;
		long hFile;

		if( (hFile = _findfirst( strFileFindPath.c_str(), &c_file )) != -1L )
		{
			//if((c_file.attrib & _A_SUBDIR) && (c_file.name[0] != '.'))//如果是文件夹的话进行处理
			//{
			//}

			while( _findnext( hFile, &c_file ) == 0 )
			{
				if(!(c_file.attrib & _A_SUBDIR) && !(c_file.name[0] != '.'))//如果是文件夹的话进行处理
				{
					strFileName = c_file.name;

					strFileName = strFilePath + strFileName;

					tmFileCreateTime = c_file.time_create;

					if((tmCurrTime - tmFileCreateTime) > 7 *60*60*24 )	//删除掉超过 7 天的日志
					{
						if(strFileName != m_Logname )
						{
							_unlink(strFileName.data());
						}
					}
				}
				Sleep(1000);
			}
			
			_findclose( hFile );
		}
		
		Sleep( 3*1000*60 );
	}
}