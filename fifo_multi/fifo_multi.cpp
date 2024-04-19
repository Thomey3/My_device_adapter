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

//ȫ�ֱ���
STXDMA_CARDINFO pstCardInfo;//���PCIE�忨��Ϣ�ṹ��

static sem_t gvar_program_exit;	//����һ���ź���
sem_t c2h_ping;		//ping�ź���
sem_t c2h_pong;		//pong�ź���

long once_readbytes = 8 MB;
long data_size = 0;

uint32_t flag_ping = 0;
uint32_t flag_pong = 0;

//��������
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
	//�ź�����ʼ��
	sem_init(&gvar_program_exit, 0, 0);
	sem_init(&c2h_ping, 0, 0);
	sem_init(&c2h_pong, 0, 0);

#ifdef WRITEFILE
	//���г�ʼ������
	//���г�ʼ��
	int fifo_size = 16;

	puts("�������ļ��洢·��:");
	char filepath[128] = { "I:\\" };
	scanf("%s", &filepath);

	ThreadFileToDisk::Ins().m_vectorBuffer.resize(10240);

	//��ʼ��8G��ping�ڴ��
	for (int i = 0; i < 10240; i++)
	{
		ThreadFileToDisk::Ins().m_vectorBuffer[i] = new databuffer();

		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_bufferAddr = NULL;
		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_bAvailable = false;
		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_bAllocateMem = false;
		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_iBufferIndex = i;
		ThreadFileToDisk::Ins().m_vectorBuffer[i]->m_iBufferSize = 0;
	}

	//1�����ɼ� 2����д�� 3����д��
	int iGatherDataType = 3;
	ThreadFileToDisk::Ins().set_toDiskType(iGatherDataType);

	int iToFileType = 2;		//����д����ļ����ǵ����ļ�   2�����óɵ����ļ���ģʽ
	ThreadFileToDisk::Ins().initDataFileBufferPing(80, 200, fifo_size);	//��ʼ��buffer�飬�ĳ�5����8M
	ThreadFileToDisk::Ins().set_filePath_Ping(filepath);				//�����ļ�·��
	ThreadFileToDisk::Ins().set_fileBlockType(iToFileType);
	ThreadFileToDisk::Ins().filecount = 10;
	ThreadFileToDisk::Ins().StartPing();
#endif

	//ִ�д򿪰忨����
	int iRet = QTXdmaOpenBoard(&pstCardInfo, 0);

	//��ȡ�忨��Ϣ
	QT_BoardGetCardInfo();

	//ֹͣ�ɼ�
	QT_BoardSetADCStop();

	//DMA��0
	QT_BoardSetTransmitMode(0, 0);

	//����ж�
	QT_BoardSetInterruptClear();

	//ִ����λ�������λ
	QT_BoardSetSoftReset();

	//ģ��ǰ��ƫ������
	int single_channel_id;
	printf("����ƫ��ͨ��ID(1:ch1 2:ch2 3:ch3 4:ch4):");
	scanf("%d", &single_channel_id);
	double single_channel_offset = 0;
	printf("����ͨ��ƫ��ֵ:");
	scanf("%f", &single_channel_offset);
	QT_BoardSetOffset(single_channel_id, single_channel_offset);

	//Ԥ��������
	uint32_t pre_trig_length = 1;
	printf("����Ԥ����(��λ:����):");
	scanf("%d", &pre_trig_length);
	QT_BoardSetPerTrigger(pre_trig_length);

	//֡ͷ����
	uint32_t frame = 1;
	printf("����֡ͷ(0:����  1:ʹ��):");
	scanf("%d", &frame);
	QT_BoardSetFrameheader(frame);

	//ʱ��ģʽ����
	uint64_t clockmode = 0;
	printf("                 0:�ڲο�ʱ��\n");
	printf("                 1:�����ʱ��\n");
	printf("                 2:��ο�ʱ��\n");
	printf("����ʱ��ģʽ:");
	scanf("%d", &clockmode);
	QT_BoardSetClockMode(clockmode);

	//���ô���ģʽ�ʹ�������
	uint32_t triggercount = 0;
	uint32_t trigmode = 0;
	uint32_t pulse_period = 0;
	uint32_t pulse_width = 0;
	printf("                  0 �������\n");
	printf("                  1 �ڲ����崥��\n");
	printf("                  2 �ⲿ���������ش���\n");
	printf("                  3 �ⲿ�����½��ش���\n");
	printf("                  4 ͨ�������ش���\n");
	printf("                  5 ͨ���½��ش���\n");
	printf("                  6 ͨ��˫�ش���\n");
	printf("                  7 �ⲿ����˫�ش���\n");
	printf("���ô���ģʽ: ");
	scanf("%d", &trigmode);
	if (trigmode != 0 && trigmode != 1 && trigmode != 2 && trigmode != 3 && trigmode != 4 && trigmode != 5 && trigmode != 6 && trigmode != 7)
	{
		printf("����ģʽ���ô���!\n");
		trigmode = 0;
		printf("Ĭ������Ϊ�������\n\n");
	}
	if (trigmode == 0)
	{
		//�������
		QT_BoardSoftTrigger();
	}
	else if (trigmode == 1)
	{
		//�ڲ����崥��
		printf("�����ڲ����崥������(��λ:ns):");
		scanf("%d", &pulse_period);
		printf("�����ڲ����崥�����(��λ:ns):");
		scanf("%d", &pulse_width);
		QT_BoardInternalPulseTrigger(triggercount, pulse_period, pulse_width);
	}
	else if (trigmode == 2)
	{
		//�ⲿ���������ش���
		QT_BoardExternalTrigger(2, triggercount);
	}
	else if (trigmode == 3)
	{
		//�ⲿ�����½��ش���
		QT_BoardExternalTrigger(3, triggercount);
	}
	else if (trigmode == 7)
	{
		//�ⲿ����˫�ش���
		QT_BoardExternalTrigger(7, triggercount);
	}
	else if (trigmode == 4 || trigmode == 5 || trigmode == 6)
	{
		//��������
		uint32_t arm_hysteresis = 70;
		printf("���ô�������(��λ:����ֵ): ");
		scanf("%d", &arm_hysteresis);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x3C, arm_hysteresis);

		uint32_t rasing_codevalue = 0;//˫���ش���ʹ�õ�����������ֵ
		uint32_t falling_codevalue = -0;
		uint32_t trigchannelID = 1;
		printf("                  1 ͨ��1\n");
		printf("                  2 ͨ��2\n");
		printf("                  3 ͨ��3\n");
		printf("                  4 ͨ��4\n");
		printf("����ͨ������ͨ��ID:");
		scanf("%d", &trigchannelID);

		if (trigmode == 4)
		{
			//ͨ�������ش���
			printf("����ͨ��������������ֵ:");
			scanf("%d", &rasing_codevalue);
			QT_BoardChannelTrigger(4, triggercount, trigchannelID, rasing_codevalue, falling_codevalue);
		}
		else if (trigmode == 5)
		{
			//ͨ���½��ش���
			printf("����ͨ�������½�����ֵ:");
			scanf("%d", &falling_codevalue);
			QT_BoardChannelTrigger(5, triggercount, trigchannelID, rasing_codevalue, falling_codevalue);
		}
		else if (trigmode == 6)
		{
			//ͨ��˫�ش���
			printf("����ͨ������˫������ֵ:");
			scanf("%d", &rasing_codevalue);
			QT_BoardChannelTrigger(6, triggercount, trigchannelID, rasing_codevalue, falling_codevalue);
		}
	}

	//����DMA����
	uint64_t once_trig_bytes = 0;
	uint32_t smaplerate = 1000;//�忨������MHz
	uint32_t channelcount = 4;//�忨ͨ����

	double SegmentDuration = 0;
	printf("���δ�����ʱ��(��λ:us): ");
	scanf("%lf", &SegmentDuration);
	if (SegmentDuration > 536870.904)
	{
		printf("���δ�����ʱ������DDR��С�����С������ʱ����\n");
		return 0;
	}
	//ͨ�����δ�����ʱ�����㵥�δ����ֽ���
	once_trig_bytes = SegmentDuration * smaplerate * channelcount * 2;//�ֽ�
	//�жϵ��δ������ֽ����Ƿ�����64�ֽڵı���
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
		printf("���δ�������������DDR��С�����С������ʱ�����߽��ʹ���Ƶ�ʣ�\n");
		return 0;
	}
	printf("���δ���������(��λ:�ֽ�): %d\n", once_trig_bytes);

	//ͨ�����δ�����ʱ���������󴥷�Ƶ�ʵ�����ֵ
	double TriggerFrequency = (1 / SegmentDuration) * 1000000;
	uint64_t GB = 4000000000;
	//���������ٶȲ�����4GB/S���㴦�Ƽ�����󴥷�Ƶ��
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
		printf("��󴥷�Ƶ��(��λ:Hz): %f\n", TriggerFrequency);
	}

	//����ʱ����1000ms���Ƚϣ�ʹ��λ����ȡ�ж�ʱ�������ڵ���1000ms
	//�����˫���ش�������ȡ�жϵ�ʱ�������һ�룬��Ϊһ�������ڻᱻ��������
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
		printf("���봥��Ƶ��(��λ:Hz): ");
		scanf("%lf", &RepetitionFrequency);
		if (RepetitionFrequency < 800)
		{
			printf("����Ƶ�ʹ��ͣ����С������ʱ�����߼Ӵ󴥷�Ƶ�ʣ�\n");
			return 0;
		}

		double dataspeed = (RepetitionFrequency * once_trig_bytes) / 1024 / 1024;
		if (trigmode == 6 || trigmode == 7)
		{
			dataspeed = dataspeed * 2;
		}
		if (dataspeed > 4000)
		{
			printf("�����ٶȴ���4GB/S�����齵�ʹ�����ʱ��������ߴ������ڣ�\n");
			return 0;
		}

		triggerduration = (1 / RepetitionFrequency) * 1000;
	}
	printf("����ʱ��(��λ:ms): %lf\n", triggerduration);

	if (triggerduration > single_interruption_duration)
	{
		data1.DMATotolbytes = once_trig_bytes;
	}
	else
	{
		double x = single_interruption_duration / triggerduration;
		if (x - int(x) > .5)x += 1;
		int xx = x;
		printf("֡ͷ���� = %d\n", xx);
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
		printf("�����ж�����������DDR��С�����С������ʱ�����߽��ʹ���Ƶ�ʣ�\n");
		return 0;
	}

	printf("�����ж�������(��λ:�ֽ�): %lld\n", data1.DMATotolbytes);
	data1.allbytes = data1.DMATotolbytes;

	//DMA��������
	QT_BoardSetFifoMultiDMAParameter(once_trig_bytes, data1.DMATotolbytes);
	int aaa = ((data1.DMATotolbytes + 83886080 - 1) / 83886080);
	ThreadFileToDisk::Ins().filecount = aaa;

	//DMA����ģʽ����
	QT_BoardSetTransmitMode(1, 0);

	//ʹ��PCIE�ж�
	QT_BoardSetInterruptSwitch();

	//���ݲɼ��߳�
	pthread_t data_collect;
	pthread_create(&data_collect, NULL, datacollect, &data1);

	//ADC��ʼ�ɼ�
	QT_BoardSetADCStart();

	//���ж��߳�
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
		//ping���ݰ���
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

				if (iBufferIndex == -1)		//�ж�buffer�Ƿ����
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

				//ִ��д�ļ��Ĳ���
				ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bAvailable = true;

				ThreadFileToDisk::Ins().PushAvailToListPing(iBufferIndex);
#endif

			}
		}
		ping_getdata++;

		//pong���ݰ��ƿ�ʼ
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

				if (iBufferIndex == -1)		//�ж�buffer�Ƿ����
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
				//ִ��д�ļ��Ĳ���
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

