#ifndef QT71357020_H
#define QT71357020_H

#include <string>
#include <vector>
using namespace std;

#define QString string 
#define QVector vector

class qtpciexdma
{
public:
    qtpciexdma();
public:
    int OpenBoard();
    int CloseBoard();
    bool CheckCardStatePCIe();
    int ResetBoard();

    QString getCardType();
    QString getVersion();
    int getClockType();
    double getFrequencyRate();
    QVector<double> getFreqList();
    QVector<double> getVRangeList();
    int getSigDataType();
    int getChannelCount();		//AD通道计数
    int getDAChannelCount();
    double getVoltageRange();	//获取量程范围
    double getDefaultVoltage();	//获取默认量程
    int getCardStateVal();		//获取板卡状态值
    int getTriggerType();		//获取触发类型
    int getTransmitTotalbbytes();	//获取传输总字节
    QString getFreqPattern();		//获取频率模式
    int getDataResolution();		//获取数据处理方法
    int getDDSCount();				//DDS计数
    int getFreqOutput();
    int getFreqPatternType();
    QString getCardState();			//获取板卡类型

    int setClokType(int iClockType);//设置时钟类型
    int setFrequency(double dbFreq, double dbVoltageRangeCh1, double dbVoltageRangeCh2, double dbVoltageRangeCh3, double dbVoltageRangeCh4);//设置频率
    int setGatherDataLength(uint32_t DMA_TransmitOnceBytes, uint32_t DMA_TransmitTotalTime);//设置数据采集长度
    int setGatherDataTrigger(QString qstrTrigType, double uFlexibleVpp);//设置数据采集方式
    int setFreqPattern(QString qstrFreqPattern);//设置频率模式
    void setHostDataBlockSize(int iBlockSize);
    void setDDSCount(int iDDSCount);
	void setChannelNum(int iChnum,int iChId);
    void setFreqOutput(int iFreq);
    void setPreTrigPara(bool bEnable, uint32_t uPreTrigCount);

    int waitRegisterConfig();
private:
    QString m_strCardType;//卡类型
    QString m_strVersion;//卡版本号
    QString m_strFreqPattern;//采集模式 71267012 单通道5G 双通道2.5G模式

    int m_iDefaultClockType;//时钟类型
    double m_dbDefaultSampleFreqrate;//缺省采样率
    int m_iPlaySigType;//数据类型
    int m_iPlayChannelCount;//通道数
    int m_iDAChannelCount;//DA通道数
    double m_dbVoltageRange;//量程电压
    int m_iCardStateVal;//板卡状态值
    int m_iTriggerType;//采集数据触发类型
    uint32_t m_iTransmitTotalbytes;//采集数据长度
    int m_iDataResolution;//数据分辨率
    int m_iHostDataBlockSize;//主机内存块大小M
	int m_iChnum;	//设置使能通道数，1：1通道。2：双通道（默认使能12通道，不可选）4：使能4通道。
	int m_iChId;	//设置使能通道ID
	int m_iDDSCount;//用于DA卡
    int m_iFreqOutput;//输出频率

    bool m_bNeedInit;//重新配置时钟和频率后要重新初始化板卡并检查状态

};

#endif // QT71357020_H
