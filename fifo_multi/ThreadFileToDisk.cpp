// ThreadCCCEvent.cpp: implementation of the CThreadCCCEvent class.
//
//////////////////////////////////////////////////////////////////////
#define WIN32_LEAN_AND_MEAN
#include "ThreadFileToDisk.h"
#include "../include/TraceLog.h"
#include"../3rd/include/QTXdmaApi.h"
#include <time.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

extern void printfLog(int nLevel, const char * fmt, ...);

std::string ThreadFileToDisk::m_strFilePathPing;
std::string ThreadFileToDisk::m_strFilePathPong;

int ThreadFileToDisk::m_iBlockSize;
int ThreadFileToDisk::m_iFileBlockType;
int ThreadFileToDisk::m_iToDiskType;
bool ThreadFileToDisk::m_bInterrupt;

VECTOR_BUFFER ThreadFileToDisk::m_vectorBuffer;

LockFreeQueue<int> m_availListPing(1024);

//申请内存，返回的指针都放入一个链表进行管理

ThreadFileToDisk::ThreadFileToDisk()
:m_bIsRunPing(false)
,m_bIsRunPong(false)
{
 
}

ThreadFileToDisk::~ThreadFileToDisk()
{
    //m_availListPing.clear();
    m_freeListPing.clear();

	m_availListPong.clear();
	m_freeListPong.clear();
}

ThreadFileToDisk& ThreadFileToDisk::Ins()
{
    static ThreadFileToDisk theIns;
	return theIns;
}

void ThreadFileToDisk::CheckFreeBuffer(int& iBufferIndex)
{
	iBufferIndex = -1;

	for (int i = 0; i < m_iBlockSize; i++)
	{
		if (!m_vectorBuffer[i]->m_bAvailable)
		{
			iBufferIndex = i;
			return;
		}
	}
}

void ThreadFileToDisk::PopFreeFromListPing(int& iBufferIndex)
{
	printfLog(5, "[ThreadFileToDisk::PopFreeFromList], m_freeList size is %d", m_freeListPing.size());

	m_MutexFreePing.Lock();

	if (m_freeListPing.size() > 0)
	{
		iBufferIndex = m_freeListPing.front();

		m_freeListPing.pop_front();
	}
	else
	{
		iBufferIndex = -1;
	}

	m_MutexFreePing.Unlock();
}

void ThreadFileToDisk::PopFreeFromListPong(int& iBufferIndex)
{
	//printfLog(5, "[ThreadFileToDisk::PopFreeFromList], m_freeList size is %d", m_freeList.size());

	m_MutexFreePong.Lock();

	if (m_freeListPong.size() > 0)
	{
		iBufferIndex = m_freeListPong.front();

		m_freeListPong.pop_front();
	}
	else
	{
		iBufferIndex = -1;
	}

	m_MutexFreePong.Unlock();
}

void ThreadFileToDisk::PushFreeToListPing(const int& iBufferIndex)
{
	printfLog(5, "[ThreadFileToDisk::PushFreeToList], push free buffer...m_freeList size is %d", m_freeListPing.size());

	m_MutexFreePing.Lock();

	m_freeListPing.push_back(iBufferIndex);

	m_MutexFreePing.Unlock();
}

void ThreadFileToDisk::PushFreeToListPong(const int& iBufferIndex)
{
	printfLog(5, "[ThreadFileToDisk::PushFreeToList], push free buffer...m_freeList size is %d", m_freeListPong.size());

	m_MutexFreePong.Lock();

	m_freeListPong.push_back(iBufferIndex);

	m_MutexFreePong.Unlock();
}

void ThreadFileToDisk::PopAvailFromListPing(int& iBufferIndex)
{
	//printfLog(5, "[ThreadFileToDisk::PopAvailFromList], m_availList size is %d", m_availListPing.size());

	//m_MutexAvailPing.Lock();

	//if (m_availListPing.size() > 0)
	//{
	//	iBufferIndex = m_availListPing.front();

	//	m_availListPing.pop_front();
	//}
	//else
	//{
	//	iBufferIndex = -1;
	//}

	//m_MutexAvailPing.Unlock();
	bool bret = m_availListPing.dequeue(iBufferIndex);
	if (bret == false)
	{
		iBufferIndex = -1;
	}

}

void ThreadFileToDisk::PushAvailToListPing(const int& iBufferIndex)
{
	//printfLog(5, "[ThreadFileToDisk::PushAvailToList], m_availList size is %d", m_availListPing.size());

	m_MutexAvailPing.Lock();

	m_availListPing.enqueue(iBufferIndex);

	m_MutexAvailPing.Unlock();
}

void ThreadFileToDisk::PopAvailFromListPong(int& iBufferIndex)
{
	//printfLog(5, "[ThreadFileToDisk::PopAvailFromList], m_availList size is %d", m_availList.size());

	m_MutexAvailPong.Lock();

	if (m_availListPong.size() > 0)
	{
		iBufferIndex = m_availListPong.front();

		m_availListPong.pop_front();
	}
	else
	{
		iBufferIndex = -1;
	}

	m_MutexAvailPong.Unlock();
}

int ThreadFileToDisk::GetAvailSizePing()
{
    //return m_availListPing.size();
	return 0;
}

int ThreadFileToDisk::GetFreeSizePing()
{
    return m_freeListPing.size();
}

int ThreadFileToDisk::GetAvailSizePong()
{
	return m_availListPong.size();
}

int ThreadFileToDisk::GetFreeSizePong()
{
	return m_freeListPong.size();
}

void ThreadFileToDisk::PushAvailToListPong(const int& iBufferIndex)
{
	//printfLog(5, "[ThreadFileToDisk::PushAvailToList], m_availList size is %d", m_availList.size());

	m_MutexAvailPong.Lock();

	m_availListPong.push_back(iBufferIndex);

	m_MutexAvailPong.Unlock();
}

bool ThreadFileToDisk::StartPing()
{
	if(m_bIsRunPing)
	{
		return false;
	}

	m_bIsRunPing = true;
    m_iThreadCount = 30;

    if(m_iFileBlockType != 2){
        //printfLog(5, "[ThreadFileToDisk::Start], multi file mode");
        for(int i = 0; i < m_iThreadCount; i++){
            m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)EventReporter, (LPVOID)i, NULL, &m_ulThreadID);
        }
    }else {
        //printfLog(5, "[ThreadFileToDisk::Start], single file mode");
        m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SingleFilePing, (LPVOID)999, NULL, &m_ulThreadID);
    }

    m_bInterrupt = false;

	return true;
}

bool ThreadFileToDisk::StartPong()
{
	if (m_bIsRunPong)
	{
		return false;
	}

	m_bIsRunPong = true;
	m_iThreadCount = 30;

	if (m_iFileBlockType != 2) {
		//printfLog(5, "[ThreadFileToDisk::Start], multi file mode");
		for (int i = 0; i < m_iThreadCount; i++) {
			m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)EventReporter, (LPVOID)i, NULL, &m_ulThreadID);
		}
	}
	else {
		//printfLog(5, "[ThreadFileToDisk::Start], single file mode");
		m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SingleFilePong, (LPVOID)999, NULL, &m_ulThreadID);
	}

	m_bInterrupt = false;

	return true;
}

void ThreadFileToDisk::Interrupt()
{
    m_bInterrupt = true;
}

bool ThreadFileToDisk::StopPing()
{
	if(!m_bIsRunPing)
		return false;

	m_bIsRunPing = false;
	

	CloseHandle(m_hThread);

	return true;
}

bool ThreadFileToDisk::StopPong()
{
	if (!m_bIsRunPong)
		return false;

	m_bIsRunPong = false;


	CloseHandle(m_hThread);

	return true;
}

void ThreadFileToDisk::set_blockSize(int iSize)
{
    m_iBlockSize = iSize;
}

void ThreadFileToDisk::set_filePath_Ping(const std::string &filepath)
{
    m_strFilePathPing = filepath;
}

void ThreadFileToDisk::set_filePath_Pong(const std::string &filepath)
{
	m_strFilePathPong = filepath;
}

void ThreadFileToDisk::set_fileBlockType(int iType)
{
    //printfLog(5, "[ThreadFileToDisk::set_fileBlockType], iType is %d", iType);
    m_iFileBlockType = iType;
}

void ThreadFileToDisk::set_toDiskType(int iType)
{
    m_iToDiskType = iType;//1触发采集 2单次写盘 3连续写盘
}

//UINT ThreadFileToDisk::SingleFilePing(LPVOID lParam)
//{
//	databuffer databuf;
//
//	databuf.m_bAvailable = false;
//
//	int iThreadId = (int)lParam;
//
//	int file_wr_cnt = 0;
//
//	do {
//
//	} while (file_wr_cnt < 768 * 2);
//
//	char datafilename[32] = { 0 };
//	sprintf(datafilename, "%sxdma.bin", m_strFilePathPing.data());
//
//	HANDLE f = CreateFileA(datafilename,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_FLAG_NO_BUFFERING,NULL);
//
//	if (f == INVALID_HANDLE_VALUE) {
//		printfLog(5, "[ThreadFileToDisk::SingleFile], fopen %s error(%s)\n", datafilename, strerror(errno));
//		return -1;
//	}
//
//	//qt获取当前秒数
//
//	SYSTEMTIME sys;
//	GetLocalTime(&sys);
//
//	int iCurTimeSec = time(0);
//
//	int iCurSecond = sys.wSecond;
//
//	uint64_t iBufferCount = 0;
//	DWORD written;
//	ThreadFileToDisk::Ins().m_bIsRunPing = true;
//	while (ThreadFileToDisk::Ins().m_bIsRunPing)
//	{
//		if (m_bInterrupt && ThreadFileToDisk::Ins().GetAvailSizePing() == 0)
//			break;
//
//		ThreadFileToDisk::Ins().PopAvailFromListPing(databuf);
//
//		if (databuf.m_bAvailable)
//		{
//			printfLog(5, "[ThreadFileToDisk::SingleFile], Threadid [%d] find available buffer [%d] \n", iThreadId, databuf.m_iBufferIndex);
//
//			//fwrite(databuf.m_bufferAddr, 1, databuf.m_iBufferSize, fp_data);
//
//			WriteFile(f, databuf.m_bufferAddr, databuf.m_iBufferSize,&written,NULL);
//
//			printfLog(5, "fwrite ok.");
//
//			iBufferCount += (uint64_t)databuf.m_iBufferSize;
//
//			//如果是单次写盘，不再释放空闲空间，写满为止
//			if (m_iToDiskType != 2)
//				ThreadFileToDisk::Ins().PushFreeToListPing(databuf);
//			else
//			{
//				//free(databuf.m_bufferAddr);
//				//databuf.m_bufferAddr = NULL;
//			}
//		}
//		else
//		{
//			//printf("Threadid【%d】没有找到可用的缓冲区\n");
//			Sleep(1);
//		}
//
//		GetLocalTime(&sys);
//
//		int iTmpSec = time(0);
//
//		int iTempSecond = sys.wSecond;
//
//		if ((iTmpSec - iCurTimeSec) >= 20 && iBufferCount != 0)
//		{
//			//计算写入数据量
//			ThreadFileToDisk::Ins().m_iDataSpeed = (int)(iBufferCount / (1024 * 1024 * (iTmpSec - iCurTimeSec)));
//			iCurTimeSec = iTmpSec;
//			iBufferCount = 0;
//		}
//
//		//        if(iTempSecond != iCurSecond && iBufferCount != 0)
//		//        {
//		//            //计算写入数据量
//		//            ThreadFileToDisk::Ins().m_iDataSpeed = iBufferCount / (1024 * 1024);
//		//            iCurSecond = iTempSecond;
//		//            iBufferCount = 0;
//		//        }
//	}
//
//	CloseHandle(f);
//
//	printfLog(5, "[ThreadFileToDisk::SingleFile],关闭文件 %s", datafilename);
//	return 0;
//}

UINT ThreadFileToDisk::SingleFilePing(LPVOID lParam)
{
	int iBufferIndex = -1;

	int iThreadId = (int)lParam;

	int file_wr_cnt = 0;
	int file_cnt = 0;
	ThreadFileToDisk::Ins().m_bIsRunPing = true;

	while (ThreadFileToDisk::Ins().m_bIsRunPing)
	{

		char datafilename[32] = { 0 };
		sprintf(datafilename, "%s/testdata%d.bin", m_strFilePathPing.data(), file_cnt);

		HANDLE f = CreateFileA(datafilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL);

		if (f == INVALID_HANDLE_VALUE) 
		{
			//printfLog(5, "[ThreadFileToDisk::SingleFile], fopen %s error(%s)\n", datafilename, strerror(errno));
			return -1;
		}
		if (m_bInterrupt && ThreadFileToDisk::Ins().GetAvailSizePing() == 0)
			break;
		do {
			ThreadFileToDisk::Ins().PopAvailFromListPing(iBufferIndex);
			if (iBufferIndex != -1)
			{
				//printfLog(5, "[ThreadFileToDisk::SingleFile], Threadid [%d] find available buffer [%d] \n", iThreadId, iBufferIndex);
				//printfLog(5, "[ThreadFileToDisk::SingleFile], iBufferIndex %d m_iBufferSize %d\n", iBufferIndex, m_vectorBuffer[iBufferIndex]->m_iBufferSize);
				//fwrite(databuf.m_bufferAddr, 1, databuf.m_iBufferSize, fp_data);
				DWORD written;
				int ret = WriteFile(f, m_vectorBuffer[iBufferIndex]->m_bufferAddr, 
							m_vectorBuffer[iBufferIndex]->m_iBufferSize, &written, NULL);
				
				printfLog(5, "written = %d\n", written);

				//如果是单次写盘，不再释放空闲空间，写满为止
				if (m_iToDiskType != 2)
				{
					m_vectorBuffer[iBufferIndex]->m_bAvailable = false;
					//ThreadFileToDisk::Ins().PushFreeToListPing(iBufferIndex);
				}
				else
				{
					//free(databuf.m_bufferAddr);
					//databuf.m_bufferAddr = NULL;
				}
				file_wr_cnt++;
			}
			else
			{
				//printf("Threadid【%d】没有找到可用的缓冲区\n");
				Sleep(1);
			}

		} while (file_wr_cnt < ThreadFileToDisk::Ins().filecount);
		file_wr_cnt = 0;
		FlushFileBuffers(f);
		CloseHandle(f);
		printfLog(5, "[ThreadFileToDisk::SingleFile],关闭文件 %s", datafilename);
		file_cnt++;

	}
	return 0;
}

UINT ThreadFileToDisk::SingleFilePong(LPVOID lParam)
{
	int iBufferIndex = -1;

	int iThreadId = (int)lParam;

	char datafilename[32] = { 0 };
	sprintf(datafilename, "%sxdma0.bin", m_strFilePathPong.data());

	HANDLE f = CreateFileA(datafilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL);

	if (f == INVALID_HANDLE_VALUE) {
		printfLog(5, "[ThreadFileToDisk::SingleFile], fopen %s error(%s)\n", datafilename, strerror(errno));
		return -1;
	}

	//qt获取当前秒数

	SYSTEMTIME sys;
	GetLocalTime(&sys);

	int iCurTimeSec = time(0);

	int iCurSecond = sys.wSecond;

	uint64_t iBufferCount = 0;
	DWORD written;

	ThreadFileToDisk::Ins().m_bIsRunPong = true;

	while (ThreadFileToDisk::Ins().m_bIsRunPong)
	{
		if (m_bInterrupt && ThreadFileToDisk::Ins().GetAvailSizePong() == 0)
			break;

		ThreadFileToDisk::Ins().PopAvailFromListPong(iBufferIndex);

		if (iBufferIndex != -1)
		{
			printfLog(5, "[ThreadFileToDisk::SingleFile], Threadid [%d] find available buffer [%d] \n", iThreadId, iBufferIndex);

			//fwrite(databuf.m_bufferAddr, 1, databuf.m_iBufferSize, fp_data);

			WriteFile(f, m_vectorBuffer[iBufferIndex]->m_bufferAddr, 
				m_vectorBuffer[iBufferIndex]->m_iBufferSize, &written, NULL);

			printfLog(5, "fwrite ok.");

			iBufferCount += (uint64_t)m_vectorBuffer[iBufferIndex]->m_iBufferSize;

			//如果是单次写盘，不再释放空闲空间，写满为止
			if (m_iToDiskType != 2)
				ThreadFileToDisk::Ins().PushFreeToListPong(iBufferIndex);
			else
			{
				//free(databuf.m_bufferAddr);
				//databuf.m_bufferAddr = NULL;
			}
		}
		else
		{
			//printf("Threadid【%d】没有找到可用的缓冲区\n");
			Sleep(1);
		}

		GetLocalTime(&sys);

		int iTmpSec = time(0);

		int iTempSecond = sys.wSecond;

		if ((iTmpSec - iCurTimeSec) >= 20 && iBufferCount != 0)
		{
			//计算写入数据量
			ThreadFileToDisk::Ins().m_iDataSpeed = (int)(iBufferCount / (1024 * 1024 * (iTmpSec - iCurTimeSec)));
			iCurTimeSec = iTmpSec;
			iBufferCount = 0;
		}

		//        if(iTempSecond != iCurSecond && iBufferCount != 0)
		//        {
		//            //计算写入数据量
		//            ThreadFileToDisk::Ins().m_iDataSpeed = iBufferCount / (1024 * 1024);
		//            iCurSecond = iTempSecond;
		//            iBufferCount = 0;
		//        }
	}

	CloseHandle(f);

	printfLog(5, "[ThreadFileToDisk::SingleFile],关闭文件 %s", datafilename);
	return 0;
}


UINT ThreadFileToDisk::EventReporter(LPVOID lParam)
{
	int iBufferIndex = -1;

    int iThreadId = (int)lParam;

    while(ThreadFileToDisk::Ins().m_bIsRunPing)
	{
        databuffer databuf;
        ThreadFileToDisk::Ins().PopAvailFromListPing(iBufferIndex);

        if (iBufferIndex != -1)
        {
            //printfLog(5, "[ThreadFileToDisk::EventReporter],Threadid [%d] find available buffer [%d] \n", iThreadId, databuf.m_iBufferIndex);

            char datafilename[32] = { 0 };
            sprintf(datafilename, "%sxdma%d.bin", m_strFilePathPing.data(), databuf.m_iBufferIndex);

            FILE *fp_data = fopen(datafilename, "wb+");
            if (NULL == fp_data)
            {
                //printfLog(5, "[ThreadFileToDisk::EventReporter],fopen %s error(%s)\n", datafilename, strerror(errno));
                continue;
            }

            fwrite(databuf.m_bufferAddr, 1, databuf.m_iBufferSize, fp_data);

            fclose(fp_data);

            //如果是单次写盘，不再释放空闲空间，写满为止
            if(m_iToDiskType != 2)
                ThreadFileToDisk::Ins().PushFreeToListPing(iBufferIndex);
            else
            {
                //free(databuf.m_bufferAddr);
                //databuf.m_bufferAddr = NULL;
            }
        }
        else
        {
            //printf("Threadid【%d】没有找到可用的缓冲区\n");
            Sleep(1);
        }

	}
	return 0;
}

void ThreadFileToDisk::initDataFileBufferPing(int iBlockSize, int iBlockCount, int iTotalSizeGB)
{
    //如果是单次写盘，即先将所有数据写入内存，再写到硬盘的模式，使用的buffer数量为使用的计算机内存大小
    //如果是连续写盘，即无限制的采集数据，使用的buffer数量为
    m_freeListPing.clear();
    //m_availListPing.clear();
	//m_availListPing.set_size(1024);

    int iGBByte = iTotalSizeGB * 1024;

	//int iBlockCount = 30;// iGBByte / iBlockSize;

	m_iBlockSize = iBlockCount;

    //printfLog(5, "[ThreadFileToDisk::initDataFileBuffer], iTotalSizeGB is %d iBlockSize size is %d iBlockCount is %d", iGBByte, iBlockSize, iBlockCount);

    for(int i = 0; i < iBlockCount; i++)
    {
//        if(m_databuffer[i].m_bufferAddr != NULL){
//            free(m_databuffer[i].m_bufferAddr);
//            m_databuffer[i].m_bufferAddr = NULL;
//        }

		if (m_vectorBuffer[i]->m_bufferAddr == NULL)
		{
			//m_databufferPong[i].m_bufferAddr = (uint8_t *)VirtualAlloc(NULL, iBlockSize * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);
			m_vectorBuffer[i]->m_bufferAddr = (uint8_t *)malloc(iBlockSize * 1024 * 1024);
		}
		m_vectorBuffer[i]->m_bAvailable = false;
		m_vectorBuffer[i]->m_bAllocateMem = true;
		m_vectorBuffer[i]->m_iBufferSize = 0;
		m_vectorBuffer[i]->m_iBufferIndex = i;
		m_vectorBuffer[i]->m_iTotalSize = iBlockSize * 1024 * 1024;

		PushFreeToListPing(m_vectorBuffer[i]->m_iBufferIndex);
    }
}

void ThreadFileToDisk::initDataFileBufferPong(int iBlockSize, int iTotalSizeGB)
{
	//如果是单次写盘，即先将所有数据写入内存，再写到硬盘的模式，使用的buffer数量为使用的计算机内存大小
	//如果是连续写盘，即无限制的采集数据，使用的buffer数量为
	m_freeListPong.clear();
	m_availListPong.clear();

	int iGBByte = iTotalSizeGB * 1024;

	int iBlockCount = iGBByte / iBlockSize;

	//printfLog(5, "[ThreadFileToDisk::initDataFileBuffer], iTotalSizeGB is %d iBlockSize size is %d iBlockCount is %d", iGBByte, iBlockSize, iBlockCount);

	for (int i = 0; i < iBlockCount; i++)
	{
		//        if(m_databuffer[i].m_bufferAddr != NULL){
		//            free(m_databuffer[i].m_bufferAddr);
		//            m_databuffer[i].m_bufferAddr = NULL;
		//        }

		if (m_vectorBuffer[i]->m_bufferAddr == NULL)
		{
			//m_databufferPong[i].m_bufferAddr = (uint8_t *)VirtualAlloc(NULL, iBlockSize * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);
			m_vectorBuffer[i]->m_bufferAddr = (uint8_t *)malloc(iBlockSize * 1024 * 1024);
		}
		m_vectorBuffer[i]->m_bAvailable = false;
		m_vectorBuffer[i]->m_bAllocateMem = true;
		m_vectorBuffer[i]->m_iBufferSize = 0;
		m_vectorBuffer[i]->m_iBufferIndex = i;
		m_vectorBuffer[i]->m_iTotalSize = iBlockSize * 1024 * 1024;

		PushFreeToListPong(m_vectorBuffer[i]->m_iBufferIndex);
	}
}

