#include "pingpong_example.h"
#include "pthread.h"
#include "semaphore.h"
#include "ThreadFileToDisk.h"
#include "../include/TraceLog.h"
#include <windows.h>
#include <conio.h> 
#include <stdlib.h>
#include <math.h>

#define MB *1024*1024
#define INTERRUPT 0
#define POLLING 1
#define WRITEFILE 1
#define BASE_8MB 8*1024*1024
#define single_interruption_duration 1000

Log_TraceLog g_Log(std::string("./logs/KunchiUpperMonitor.log"));
Log_TraceLog * pLog = &g_Log;

void printfLog(int nLevel, const char * fmt, ...)
{
	if (nLevel == 0)
		return;

	if (pLog == NULL)
		return;

	char buf[1024];
	va_list list;
	va_start(list, fmt);
	vsprintf(buf, fmt, list);
	va_end(list);
	pLog->Trace(nLevel, buf);
}

//全局变量
STXDMA_CARDINFO pstCardInfo;//存放PCIE板卡信息结构体

static sem_t gvar_program_exit;	//声明一个信号量
sem_t c2h_ping;		//ping信号量
sem_t c2h_pong;		//pong信号量

long once_readbytes = 8 MB;
long data_size = 0;

uint32_t flag_ping = 0;
uint32_t flag_pong = 0;

//函数声明
void * PollIntr(void * lParam);
void * datacollect(void * lParam);

struct data
{
	uint64_t DMATotolbytes;
	uint64_t allbytes;
};
struct data data1;

int main()
{
	//信号量初始化
	sem_init(&gvar_program_exit, 0, 0);
	sem_init(&c2h_ping, 0, 0);
	sem_init(&c2h_pong, 0, 0);

#ifdef WRITEFILE
	//队列初始化操作
	//队列初始化
	int fifo_size = 16;

	puts("请输入文件存储路径:");
	char filepath[128] = { "I:\\" };
	scanf("%s", &filepath);

	ThreadFileToDisk::Ins().m_vectorBuffer.resize(10240);

	//初始化8G的ping内存块
	for (int i = 0; i < 10240; i++)
	{
		ThreadFileToDisk::Ins().m_vectorBuffer[i] = new databuffer();

		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_bufferAddr = NULL;
		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_bAvailable = false;
		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_bAllocateMem = false;
		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_iBufferIndex = i;
		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_iBufferSize = 0;
	}

	//1触发采集 2单次写盘 3连续写盘
	int iGatherDataType = 3;
	ThreadFileToDisk::Ins().set_toDiskType(iGatherDataType);

	int iToFileType = 2;		//区分写多个文件还是单个文件   2是设置成单个文件的模式
	ThreadFileToDisk::Ins().initDataFileBufferPing(80, 200, fifo_size);	//初始化buffer块，改成5倍的8M
	ThreadFileToDisk::Ins().set_filePath_Ping(filepath);				//设置文件路径
	ThreadFileToDisk::Ins().set_fileBlockType(iToFileType);
	ThreadFileToDisk::Ins().filecount = 10;
	ThreadFileToDisk::Ins().StartPing();
#endif

	//执行打开板卡操作
	int iRet = QTXdmaOpenBoard(&pstCardInfo, 0);

	//获取板卡信息
	QT_BoardGetCardInfo();

	//停止采集
	QT_BoardSetADCStop();

	//DMA置0
	QT_BoardSetTransmitMode(0, 0);

	//清除中断
	QT_BoardSetInterruptClear();

	//执行上位机软件复位
	QT_BoardSetSoftReset();

	//模拟前端偏置设置
	int single_channel_id;
	printf("调节偏置通道ID(1:ch1 2:ch2 3:ch3 4:ch4):");
	scanf("%d", &single_channel_id);
	double single_channel_offset = 0;
	printf("输入通道偏置值:");
	scanf("%f", &single_channel_offset);
	QT_BoardSetOffset(single_channel_id, single_channel_offset);

	//预触发设置
	uint32_t pre_trig_length = 1;
	printf("设置预触发(单位:点数):");
	scanf("%d", &pre_trig_length);
	QT_BoardSetPerTrigger(pre_trig_length);

	//帧头设置
	uint32_t frame = 1;
	printf("设置帧头(0:禁用  1:使能):");
	scanf("%d", &frame);
	QT_BoardSetFrameheader(frame);

	//时钟模式设置
	uint64_t clockmode = 0;
	printf("                 0:内参考时钟\n");
	printf("                 1:外采样时钟\n");
	printf("                 2:外参考时钟\n");
	printf("设置时钟模式:");
	scanf("%d", &clockmode);
	QT_BoardSetClockMode(clockmode);

	//设置触发模式和触发次数
	uint32_t triggercount = 0;
	uint32_t trigmode = 0;
	uint32_t pulse_period = 0;
	uint32_t pulse_width = 0;
	printf("                  0 软件触发\n");
	printf("                  1 内部脉冲触发\n");
	printf("                  2 外部脉冲上升沿触发\n");
	printf("                  3 外部脉冲下降沿触发\n");
	printf("                  4 通道上升沿触发\n");
	printf("                  5 通道下降沿触发\n");
	printf("                  6 通道双沿触发\n");
	printf("                  7 外部脉冲双沿触发\n");
	printf("设置触发模式: ");
	scanf("%d", &trigmode);
	if (trigmode != 0 && trigmode != 1 && trigmode != 2 && trigmode != 3 && trigmode != 4 && trigmode != 5 && trigmode != 6 && trigmode != 7)
	{
		printf("触发模式设置错误!\n");
		trigmode = 0;
		printf("默认设置为软件触发\n\n");
	}
	if (trigmode == 0)
	{
		//软件触发
		QT_BoardSoftTrigger();
	}
	else if (trigmode == 1)
	{
		//内部脉冲触发
		printf("设置内部脉冲触发周期(单位:ns):");
		scanf("%d", &pulse_period);
		printf("设置内部脉冲触发宽度(单位:ns):");
		scanf("%d", &pulse_width);
		QT_BoardInternalPulseTrigger(triggercount, pulse_period, pulse_width);
	}
	else if (trigmode == 2)
	{
		//外部脉冲上升沿触发
		QT_BoardExternalTrigger(2, triggercount);
	}
	else if (trigmode == 3)
	{
		//外部脉冲下降沿触发
		QT_BoardExternalTrigger(3, triggercount);
	}
	else if (trigmode == 7)
	{
		//外部脉冲双沿触发
		QT_BoardExternalTrigger(7, triggercount);
	}
	else if (trigmode == 4 || trigmode == 5 || trigmode == 6)
	{
		//触发迟滞
		uint32_t arm_hysteresis = 70;
		printf("设置触发迟滞(单位:量化值): ");
		scanf("%d", &arm_hysteresis);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x3C, arm_hysteresis);

		uint32_t rasing_codevalue = 0;//双边沿触发使用的是上升沿阈值
		uint32_t falling_codevalue = -0;
		uint32_t trigchannelID = 1;
		printf("                  1 通道1\n");
		printf("                  2 通道2\n");
		printf("                  3 通道3\n");
		printf("                  4 通道4\n");
		printf("设置通道触发通道ID:");
		scanf("%d", &trigchannelID);

		if (trigmode == 4)
		{
			//通道上升沿触发
			printf("设置通道触发上升沿阈值:");
			scanf("%d", &rasing_codevalue);
			QT_BoardChannelTrigger(4, triggercount, trigchannelID, rasing_codevalue, falling_codevalue);
		}
		else if (trigmode == 5)
		{
			//通道下降沿触发
			printf("设置通道触发下降沿阈值:");
			scanf("%d", &falling_codevalue);
			QT_BoardChannelTrigger(5, triggercount, trigchannelID, rasing_codevalue, falling_codevalue);
		}
		else if (trigmode == 6)
		{
			//通道双沿触发
			printf("设置通道触发双边沿阈值:");
			scanf("%d", &rasing_codevalue);
			QT_BoardChannelTrigger(6, triggercount, trigchannelID, rasing_codevalue, falling_codevalue);
		}
	}

	//设置DMA参数
	uint64_t once_trig_bytes = 0;
	uint32_t smaplerate = 1000;//板卡采样率MHz
	uint32_t channelcount = 4;//板卡通道数

	double SegmentDuration = 0;
	printf("单次触发段时长(单位:us): ");
	scanf("%lf", &SegmentDuration);
	if (SegmentDuration > 536870.904)
	{
		printf("单次触发段时长超过DDR大小！请减小触发段时长！\n");
		return 0;
	}
	//通过单次触发段时长计算单次触发字节数
	once_trig_bytes = SegmentDuration * smaplerate * channelcount * 2;//字节
	//判断单次触发的字节数是否满足64字节的倍数
	if (once_trig_bytes % 512 == 0)
	{
		once_trig_bytes = once_trig_bytes;
	}
	else
	{
		once_trig_bytes = (once_trig_bytes / 512) * 512;
	}

	if (once_trig_bytes > 4294967232)
	{
		printf("单次触发数据量超过DDR大小！请减小触发段时长或者降低触发频率！\n");
		return 0;
	}
	printf("单次触发数据量(单位:字节): %d\n", once_trig_bytes);

	//通过单次触发段时长计算的最大触发频率的理论值
	double TriggerFrequency = (1 / SegmentDuration) * 1000000;
	uint64_t GB = 4000000000;
	//依据流盘速度不大于4GB/S计算处推荐的最大触发频率
	if (trigmode != 0 && trigmode != 1)
	{
		if (trigmode == 6 || trigmode == 7)
		{
			TriggerFrequency = (GB / once_trig_bytes) / 2;
		}
		else
		{
			TriggerFrequency = GB / once_trig_bytes;
		}
		printf("最大触发频率(单位:Hz): %f\n", TriggerFrequency);
	}

	//触发时长和1000ms做比较，使上位机获取中断时间间隔大于等于1000ms
	//如果是双边沿触发，获取中断的时间会缩短一半，因为一个周期内会被触发两次
	double triggerduration = 0;
	if (trigmode == 1)
	{
		triggerduration = pulse_period;
		triggerduration = triggerduration / 1000;//us
		triggerduration = triggerduration / 1000;//ms
	}
	else
	{
		double RepetitionFrequency = 0;
		printf("输入触发频率(单位:Hz): ");
		scanf("%lf", &RepetitionFrequency);
		if (RepetitionFrequency < 800)
		{
			printf("触发频率过低！请减小触发段时长或者加大触发频率！\n");
			return 0;
		}

		double dataspeed = (RepetitionFrequency * once_trig_bytes) / 1024 / 1024;
		if (trigmode == 6 || trigmode == 7)
		{
			dataspeed = dataspeed * 2;
		}
		if (dataspeed > 4000)
		{
			printf("流盘速度大于4GB/S！建议降低触发段时长或者提高触发周期！\n");
			return 0;
		}

		triggerduration = (1 / RepetitionFrequency) * 1000;
	}
	printf("触发时长(单位:ms): %lf\n", triggerduration);

	if (triggerduration > single_interruption_duration)
	{
		data1.DMATotolbytes = once_trig_bytes;
	}
	else
	{
		double x = single_interruption_duration / triggerduration;
		if (x - int(x) > .5)x += 1;
		int xx = x;
		printf("帧头个数 = %d\n", xx);
		data1.DMATotolbytes = once_trig_bytes * xx;
	}

	if (data1.DMATotolbytes % 512 == 0)
	{
		data1.DMATotolbytes = data1.DMATotolbytes;
	}
	else
	{
		data1.DMATotolbytes = (data1.DMATotolbytes / 512) * 512;
	}

	if (data1.DMATotolbytes > 4294967232)
	{
		printf("单次中断数据量超过DDR大小！请减小触发段时长或者降低触发频率！\n");
		return 0;
	}

	printf("单次中断数据量(单位:字节): %lld\n", data1.DMATotolbytes);
	data1.allbytes = data1.DMATotolbytes;

	//DMA参数设置
	QT_BoardSetFifoMultiDMAParameter(once_trig_bytes, data1.DMATotolbytes);
	int aaa = ((data1.DMATotolbytes + 83886080 - 1) / 83886080);
	ThreadFileToDisk::Ins().filecount = aaa;

	//DMA传输模式设置
	QT_BoardSetTransmitMode(1, 0);

	//使能PCIE中断
	QT_BoardSetInterruptSwitch();

	//数据采集线程
	pthread_t data_collect;
	pthread_create(&data_collect, NULL, datacollect, &data1);

	//ADC开始采集
	QT_BoardSetADCStart();

	//等中断线程
	pthread_t wait_intr_c2h_0;
	pthread_create(&wait_intr_c2h_0, NULL, PollIntr, NULL);

	char ch;
	int is_enter;
	while (1)
	{
		printf("\nPress 'q' + 'Enter' to exit!\n");
		while ('\n' == (ch = (char)getchar()));
		is_enter = getchar();
		printf("Now is_enter=%d\n", is_enter);
		if ('q' == ch && is_enter == 10)
		{
			QT_BoardSetADCStop();
			printf("set adc stop......\n");
			QT_BoardSetTransmitMode(0, 0);
			printf("set DMA stop......\n");
			QTXdmaCloseBoard(&pstCardInfo);
			printf("close board......\n");
			return 0;
		}
		else
		{
			printf("printf\n");
			while ('\n' != (ch = (char)getchar()));
			printf("input invaild!please try again\n");
		}
	}

	sem_wait(&gvar_program_exit);
	sem_destroy(&gvar_program_exit);

EXIT:
	QT_BoardSetADCStop();
	QT_BoardSetTransmitMode(0, 0);
	QTXdmaCloseBoard(&pstCardInfo);
	system("pause\n");
	return 0;

}

void * PollIntr(void *lParam)
{
	pthread_detach(pthread_self());
	int intr_cnt = 1;
	int intr_ping = 0;
	int intr_pong = 0;

	clock_t start, finish;
	double time1 = 0;
	start = clock();

	while (true)
	{
		QT_BoardInterruptGatherType();

		finish = clock();
		time1 = (double)(finish - start) / CLOCKS_PER_SEC;
		printf("intr time is %f\n", time1);
		start = finish;

		if (intr_cnt % 2 == 0)
		{
			sem_post(&c2h_pong);
			intr_pong++;
			//printf("pong is %d free list size %d\n", intr_pong, ThreadFileToDisk::Ins().GetFreeSizePing());
		}
		else
		{
			sem_post(&c2h_ping);
			intr_ping++;
			//printf("ping is %d free list size %d\n", intr_ping, ThreadFileToDisk::Ins().GetFreeSizePing());
		}

		intr_cnt++;
	}

EXIT:
	pthread_exit(NULL);
	return 0;
}

void * datacollect(void * lParam)
{
	pthread_detach(pthread_self());

	long ping_getdata = 0;
	long pong_getdata = 0;

	while (1)
	{
		//ping数据搬移
		{
			sem_wait(&c2h_ping);
			int iBufferIndex = -1;
			int64_t remain_size = data1.DMATotolbytes;
			uint64_t offsetaddr_ping = 0x0;

			while (remain_size > 0)
			{
#ifdef WRITEFILE
				//ThreadFileToDisk::Ins().PopFreeFromListPing(iBufferIndex);
				ThreadFileToDisk::Ins().CheckFreeBuffer(iBufferIndex);

				if (iBufferIndex == -1)		//判断buffer是否可用
				{
					printf("buffer is null %d!\n", ThreadFileToDisk::Ins().GetFreeSizePing());
					printfLog(5, "ping buffer is null %d!", ThreadFileToDisk::Ins().GetFreeSizePing());
					continue;
				}

				ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize = 0;
#endif
				int iLoopCount = 0;
				int iWriteBytes = 0;

				do
				{
					if (remain_size >= once_readbytes)
					{
						QTXdmaGetDataBuffer(offsetaddr_ping, &pstCardInfo,
							ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bufferAddr + iLoopCount * once_readbytes, once_readbytes, 0);

						ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize += (unsigned int)once_readbytes;

						iWriteBytes += once_readbytes;
					}
					else if (remain_size > 0)
					{
						QTXdmaGetDataBuffer(offsetaddr_ping, &pstCardInfo,
							ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bufferAddr + iLoopCount * once_readbytes, remain_size, 0);

						ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize += (unsigned int)remain_size;
						iWriteBytes += remain_size;
					}

					offsetaddr_ping += once_readbytes;
					remain_size -= once_readbytes;

					if (remain_size <= 0)
					{
						printfLog(5, "break ");
						break;
					}
					iLoopCount++;
				} while (iWriteBytes < ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iTotalSize);

#ifdef WRITEFILE

				//执行写文件的操作
				ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bAvailable = true;

				ThreadFileToDisk::Ins().PushAvailToListPing(iBufferIndex);
#endif

			}
		}
		ping_getdata++;

		//pong数据搬移开始
		{
			sem_wait(&c2h_pong);
			int iBufferIndex = -1;
			int64_t remain_size = data1.DMATotolbytes;
			uint64_t offsetaddr_pong = 0x100000000;

			while (remain_size > 0)
			{
#ifdef WRITEFILE
				//ThreadFileToDisk::Ins().PopFreeFromListPing(iBufferIndex);

				ThreadFileToDisk::Ins().CheckFreeBuffer(iBufferIndex);

				if (iBufferIndex == -1)		//判断buffer是否可用
				{
					printf("buffer is null! %d\n", ThreadFileToDisk::Ins().GetFreeSizePing());
					printfLog(5, "pong buffer is null %d!", ThreadFileToDisk::Ins().GetFreeSizePing());
					continue;
				}

				ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize = 0;

				int iLoopCount = 0;
				int iWriteBytes = 0;

				do
				{
					if (remain_size >= once_readbytes)
					{
						QTXdmaGetDataBuffer(offsetaddr_pong, &pstCardInfo,
							ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bufferAddr + iLoopCount * once_readbytes, once_readbytes, 0);

						ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize += (unsigned int)once_readbytes;

						iWriteBytes += once_readbytes;
					}
					else if (remain_size > 0)
					{
						QTXdmaGetDataBuffer(offsetaddr_pong, &pstCardInfo,
							ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bufferAddr + iLoopCount * once_readbytes, remain_size, 0);

						ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize += (unsigned int)remain_size;

						iWriteBytes += remain_size;
					}

					iLoopCount++;

					offsetaddr_pong += once_readbytes;
					remain_size -= once_readbytes;

					if (remain_size <= 0)
					{
						break;
					}

				} while (iWriteBytes < ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iTotalSize);
#endif

#ifdef WRITEFILE
				//执行写文件的操作
				ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bAvailable = true;

				ThreadFileToDisk::Ins().PushAvailToListPing(iBufferIndex);
#endif
			}
		}
		pong_getdata++;
		printf("ping_getdata=%d  pong_getdata=%d\n", ping_getdata, pong_getdata);
	}
	puts("thread exit while!");
EXIT:
	pthread_exit(NULL);
	sem_destroy(&c2h_ping);
	sem_destroy(&c2h_pong);
	return 0;
}

