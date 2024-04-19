// ThreadCCCEvent.h: interface for the CThreadCCCEvent class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_THREADCCCEVENT_H__CDFFBC73_D69C_433E_BB3C_E39552C67E58__INCLUDED_)
#define AFX_THREADCCCEVENT_H__CDFFBC73_D69C_433E_BB3C_E39552C67E58__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <windows.h>
#include <list>
#include <map>
#include <stdint.h>
#include "Mutex.h"
#include <string>
#include <deque>
#include <vector>

#include "databuffer.h"

#include "lock_free_queue.h"
#include <iostream>
#include <thread>
#include <atomic>


typedef struct
{
	int m_msgflag;
	unsigned char * m_memAddr;//缓存地址
}HWCCCEVENT;

//typedef std::list <databuffer> CCCEventList;
typedef std::deque <int> CCCEventList;

typedef std::vector<databuffer *> VECTOR_BUFFER;

class ThreadFileToDisk
{
public:
    ThreadFileToDisk();
    virtual ~ThreadFileToDisk();
    static ThreadFileToDisk& Ins();
    void set_blockSize(int iSize);
    void set_filePath_Ping(const std::string &filepath);
	void set_filePath_Pong(const std::string &filepath);
	void set_fileBlockType(int iType);
    void set_toDiskType(int iType);
public:
	bool StartPing();
	bool StopPing();

	bool StartPong();
	bool StopPong();

    void Interrupt();
	int filecount;
public:
	void PushAvailToListPing(const int& iBufferIndex);
	void PopAvailFromListPing(int& iBufferIndex);
	void PushAvailToListPong(const int& iBufferIndex);
	void PopAvailFromListPong(int& iBufferIndex);

	void CheckFreeBuffer(int& iBufferIndex);

    int GetAvailSizePing();
    int GetFreeSizePing();
	int GetAvailSizePong();
	int GetFreeSizePong();

	void PushFreeToListPing(const int& iBufferIndex);
	void PopFreeFromListPing(int& iBufferIndex);
	void PushFreeToListPong(const int& iBufferIndex);
	void PopFreeFromListPong(int& iBufferIndex);

	static UINT EventReporter(LPVOID lParam);
	static UINT SingleFilePing(LPVOID lParam);
	static UINT SingleFilePong(LPVOID lParam);

    void initDataFileBufferPing(int iBlockSize, int iBlockCount, int iTotalSize);
	void initDataFileBufferPong(int iBlockSize, int iTotalSize);
public:
    //databuffer m_databufferPing[10240];
	//databuffer m_databufferPong[10240];

	static VECTOR_BUFFER m_vectorBuffer;

    int m_iDataSpeed;
private:
    //CCCEventList m_availListPing;

    CCCEventList m_freeListPing;

	CCCEventList m_availListPong;
	CCCEventList m_freeListPong;

	HANDLE m_hThread;
    ULONG m_ulThreadID;
	
    mt::Mutex m_MutexFreePing;
    mt::Mutex m_MutexAvailPing;
	mt::Mutex m_MutexFreePong;
	mt::Mutex m_MutexAvailPong;
    bool m_bIsRunPing;
    bool m_bIsRunPong;
public:
    static int m_iBlockSize;
    static std::string m_strFilePathPing;
	static std::string m_strFilePathPong;

    int m_iThreadCount;//线程数
    static int m_iFileBlockType;//
    static int m_iToDiskType;//连续写盘1 单次写盘2

    static bool m_bInterrupt;
};

#endif // !defined(AFX_THREADCCCEVENT_H__CDFFBC73_D69C_433E_BB3C_E39552C67E58__INCLUDED_)
