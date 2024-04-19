#ifndef XDMADATATHREAD_H
#define XDMADATATHREAD_H

#include <string>
using namespace std;

#include "../../3rd/include/pthread.h"

class xdmadatathread
{
public:
    explicit xdmadatathread();
	void start();
    void stop();
    void setFileName(const string& filename);
    int getResult();
    void resetResult();
    void set_dmaTotalBytes(const int& iBytes);
    void set_filePath(const string& filepath);
    void set_runningstate(bool bstate);
    void set_triggertype(const int& iTriggerType);
    void set_triggercount(const int& iTriggerCount);
    void set_hostdatablock(const int& iHostDataBlock);
    void set_GatherType(int iType);				//设置采集方式（中断或轮询）
    void set_BoardIndex(int iBoardIndex);
protected:
	static void * run(void* Param);

private:
    volatile bool stopped;
    static string m_filename;			//文件名
	static string m_filepath;			//文件路径
	static int iResult;					//？？
	static int m_iDMATotalBytes;		//DMA传输总字节
	static bool m_bRunningState;		//运行状态
	static int m_iTriggerType;			//触发类型
	static int m_iTriggerCount;			//触发计数
	static int m_iHostDataBlockSize;	//主机传输数据块大小

	static int m_iGatherType;//采集模式 1-interrupt 中断 2-polling 轮询

	static int m_iBoardIndex;//板卡编号
};

#endif // XDMADATATHREAD_H
