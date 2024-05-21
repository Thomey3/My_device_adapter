//////////////////////////////////////////////////////////////////////////////
// head file
//
#include "DeviceBase.h"
#include "ImgBuffer.h"
#include "DeviceThreads.h"
#include "ModuleInterface.h"
#include "MMDevice.h"
#include "DeviceUtils.h"

#include <string>
#include <map>
#include <algorithm>
#include <stdint.h>
#include <future>
#include <vector>
#include <iostream>
#include <sstream>
#include <stdlib.h> 

#include "NIDAQmx.h"
#include <boost/lexical_cast.hpp>
#include <boost/utility.hpp>
#include "ETL.h"

#include "QTXdmaApi.h"
#include "pingpong_example.h"
#include "pthread.h"
#include "semaphore.h"
#include "ThreadFileToDisk.h"
#include "TraceLog.h"
#include "databuffer.h"
#include "Mutex.h"

//////////////////////////////////////////////////////////////////////////////
// DAQ define
//
#define single_interruption_duration 1000
#define MB *1024*1024
#define WRITEFILE 1
// NIDAQ
// 
// 
//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_UNKNOWN_MODE         102
#define ERR_UNKNOWN_POSITION     103
#define ERR_IN_SEQUENCE          104
#define ERR_SEQUENCE_INACTIVE    105
#define ERR_STAGE_MOVING         106
#define HUB_NOT_AVAILABLE        107

//////////////////////////////////////////////////////////////////////////////
// NIDAQ HUB
//

extern const char* g_DeviceNameNIDAQHub;
extern const char* g_DeviceNameNIDAQAOPortPrefix;
extern const char* g_DeviceNameNIDAQDOPortPrefix;
extern const char* g_On;
extern const char* g_Off;
extern const char* g_Low;
extern const char* g_High;
extern const char* g_Never;
extern const char* g_Pre;
extern const char* g_Post;
extern const char* g_UseHubSetting;

extern const int ERR_SEQUENCE_RUNNING;
extern const int ERR_SEQUENCE_TOO_LONG;
extern const int ERR_SEQUENCE_ZERO_LENGTH;
extern const int ERR_VOLTAGE_OUT_OF_RANGE;
extern const int ERR_NONUNIFORM_CHANNEL_VOLTAGE_RANGES;
extern const int ERR_VOLTAGE_RANGE_EXCEEDS_DEVICE_LIMITS;
extern const int ERR_UNKNOWN_PINS_PER_PORT;

inline std::string GetNIError(int32 nierr)
{
	char buf[1024];
	if (DAQmxGetErrorString(nierr, buf, sizeof(buf)))
		return "[failed to get DAQmx error code]";
	return buf;
}

inline std::string GetNIDetailedErrorForMostRecentCall()
{
	char buf[1024];
	if (DAQmxGetExtendedErrorInfo(buf, sizeof(buf)))
		return "[failed to get DAQmx extended error info]";
	return buf;
}

// Mix-in class for error code handling.
template <typename TDevice>
class ErrorTranslator
{
public:
	explicit ErrorTranslator(int minCode, int maxCode,
		void (TDevice::* setCodeFunc)(int, const char*)) :
		minErrorCode_(minCode),
		maxErrorCode_(maxCode),
		nextErrorCode_(minCode),
		setCodeFunc_(setCodeFunc)
	{}

	int NewErrorCode(const std::string& msg)
	{
		if (nextErrorCode_ > maxErrorCode_)
			nextErrorCode_ = minErrorCode_;
		int code = nextErrorCode_++;

		(static_cast<TDevice*>(this)->*setCodeFunc_)(code, msg.c_str());
		return code;
	}

	int TranslateNIError(int32 nierr)
	{
		char buf[1024];
		if (DAQmxGetErrorString(nierr, buf, sizeof(buf)))
			return NewErrorCode("[Cannot get DAQmx error message]");
		return NewErrorCode(buf);
	}

private:
	void (TDevice::* setCodeFunc_)(int, const char*);
	int minErrorCode_;
	int maxErrorCode_;
	int nextErrorCode_;
};

class NIDAQHub;

/**
 * Helper class for NIDAQHub using templates to deal with 8 pin and 32 pin DO ports without
 * too much code duplication.
 *
 * Each NIDAQHub is associated with one phtysical device.  Each hub instantiates versions of
 * NIDAQDOHUb for uInt8, uInt16, and uInt32, to handle 8, 16, and 32 pin ports.   *
 *
*/
template <class Tuint>
class NIDAQDOHub
{
public:
	NIDAQDOHub(NIDAQHub* hub);
	~NIDAQDOHub();

	int StartDOBlankingAndOrSequence(const std::string& port, const bool blankingOn,
		const bool sequenceOn, const long& pos, const bool blankingDirection, const std::string triggerPort);
	int StartDOBlankingAndOrSequenceWithoutTrigger(const std::string& port, const bool blankingOn,
		const bool sequenceOn, const long& pos, const bool blankingDirection, int32 num);
	int StopDOBlankingAndSequence();
	int AddDOPortToSequencing(const std::string& port, const std::vector<Tuint> sequence);
	void RemoveDOPortFromSequencing(const std::string& port);

private:

	int GetPinState(const std::string pinDesignation, bool& state);
	int HandleTaskError(int32 niError);


	int DaqmxWriteDigital(TaskHandle doTask_, int32 samplesPerChar, const Tuint* samples, int32* numWritten);

	NIDAQHub* hub_;
	uInt32 portWidth_;
	TaskHandle diTask_;
	TaskHandle doTask_;
	std::vector<std::string> physicalDOChannels_;
	std::vector<std::vector<Tuint>> doChannelSequences_;
};


/**
 * A hub - peripheral device set for driving multiple analog output ports,
 * possibly with hardware-triggered sequencing using a shared trigger input.
 * Each Hub is associated with one NIDAQ device, usually named Dev1, Dev2, etc..
*/
class NIDAQHub : public HubBase<NIDAQHub>,
	public ErrorTranslator<NIDAQHub>,
	boost::noncopyable
{
	friend NIDAQDOHub<uInt32>;
	friend NIDAQDOHub<uInt16>;
	friend NIDAQDOHub<uInt8>;
public:
	NIDAQHub();
	virtual ~NIDAQHub();

	virtual int Initialize();
	virtual int Shutdown();

	virtual void GetName(char* name) const;
	virtual bool Busy() { return false; }

	virtual int DetectInstalledDevices();

	// Interface for individual ports
	virtual int GetVoltageLimits(double& minVolts, double& maxVolts);
	virtual int StartAOSequenceForPort(const std::string& port,
		const std::vector<double> sequence);
	virtual int StopAOSequenceForPort(const std::string& port);

	virtual int IsSequencingEnabled(bool& flag) const;
	virtual int GetSequenceMaxLength(long& maxLength) const;

	int StartDOBlankingAndOrSequence(const std::string& port, const uInt32 portWidth, const bool blankingOn, const bool sequenceOn,
		const long& pos, const bool blankingDirection, const std::string triggerPort);
	int StartDOBlankingAndOrSequenceWithoutTrigger(const std::string& port, const bool blankingOn,
		const bool sequenceOn, const long& pos, const bool blankingDirection, int32 num);
	int StopDOBlankingAndSequence(const uInt32 portWidth);

	int SetDOPortState(const std::string port, uInt32 portWidth, long state);
	const std::string GetTriggerPort() { return niTriggerPort_; }

	NIDAQDOHub<uInt8>* getDOHub8() { return doHub8_; }
	NIDAQDOHub<uInt16>* getDOHub16() { return doHub16_; }
	NIDAQDOHub<uInt32>* getDOHub32() { return doHub32_; }

	int StopTask(TaskHandle& task);

private:
	int AddAOPortToSequencing(const std::string& port, const std::vector<double> sequence);
	void RemoveAOPortFromSequencing(const std::string& port);

	int GetVoltageRangeForDevice(const std::string& device, double& minVolts, double& maxVolts);
	std::vector<std::string> GetAOTriggerTerminalsForDevice(const std::string& device);
	std::vector<std::string> GetAnalogPortsForDevice(const std::string& device);
	std::vector<std::string> GetDigitalPortsForDevice(const std::string& device);
	std::string GetPhysicalChannelListForSequencing(std::vector<std::string> channels) const;
	template<typename T> int GetLCMSamplesPerChannel(size_t& seqLen, std::vector<std::vector<T>>) const;
	template<typename T> void GetLCMSequence(T* buffer, std::vector<std::vector<T>> sequences) const;

	int SwitchTriggerPortToReadMode();

	int StartAOSequencingTask();

	// Action handlers
	int OnDevice(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMaxSequenceLength(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnSequencingEnabled(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTriggerInputPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSampleRate(MM::PropertyBase* pProp, MM::ActionType eAct);


	bool initialized_;
	size_t maxSequenceLength_;
	bool sequencingEnabled_;
	bool sequenceRunning_;

	std::string niDeviceName_;
	std::string niTriggerPort_;
	std::string niChangeDetection_;
	std::string niSampleClock_;

	double minVolts_; // Min possible for device
	double maxVolts_; // Max possible for device
	double sampleRateHz_;

	TaskHandle aoTask_;
	TaskHandle doTask_;

	NIDAQDOHub<uInt8>* doHub8_;
	NIDAQDOHub<uInt16>* doHub16_;
	NIDAQDOHub<uInt32>* doHub32_;

	// "Loaded" sequences for each channel
	// Invariant: physicalChannels_.size() == channelSequences_.size()
	std::vector<std::string> physicalAOChannels_; // Invariant: all unique
	std::vector<std::vector<double>> aoChannelSequences_;

};


/**
* Represents analog output ports.   Each port is one pin, corresponding to a0, a1, etc.. on the NIDAQ board.
*/

class NIAnalogOutputPort : public CSignalIOBase<NIAnalogOutputPort>,
	ErrorTranslator<NIAnalogOutputPort>,
	boost::noncopyable
{
public:
	NIAnalogOutputPort(const std::string& port);
	virtual ~NIAnalogOutputPort();

	virtual int Initialize();
	virtual int Shutdown();

	virtual void GetName(char* name) const;
	virtual bool Busy() { return false; }

	virtual int SetGateOpen(bool open);
	virtual int GetGateOpen(bool& open);
	virtual int SetSignal(double volts);
	virtual int GetSignal(double& /* volts */) { return DEVICE_UNSUPPORTED_COMMAND; }
	virtual int GetLimits(double& minVolts, double& maxVolts);

	virtual int IsDASequenceable(bool& isSequenceable) const;
	virtual int GetDASequenceMaxLength(long& maxLength) const;
	virtual int StartDASequence();
	virtual int StopDASequence();
	virtual int ClearDASequence();
	virtual int AddToDASequence(double);
	virtual int SendDASequence();

private:
	// Pre-init property action handlers
	int OnMinVolts(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMaxVolts(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSequenceable(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSequenceTransition(MM::PropertyBase* pProp, MM::ActionType eAct);

	// Post-init property action handlers
	int OnVoltage(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	NIDAQHub* GetHub() const
	{
		return static_cast<NIDAQHub*>(GetParentHub());
	}
	int TranslateHubError(int err);
	int StartOnDemandTask(double voltage);
	int StopTask();

private:
	const std::string niPort_;

	bool initialized_;

	bool gateOpen_;
	double gatedVoltage_;
	bool sequenceRunning_;

	double minVolts_; // User-selected for this port
	double maxVolts_; // User-selected for this port
	bool neverSequenceable_;
	bool transitionPostExposure_; // when to transition in a sequence, not that we always transition on a rising flank
	// it can be advantaguous to transition post exposure, in which case we have to modify our sequence 

	TaskHandle task_;

	std::vector<double> unsentSequence_;
	std::vector<double> sentSequence_; // Pretend "sent" to device
};


/**
* Class that provides the digital output port (p0, p1, etc..).  Each port (8, 16, or 32 pins) will get one instance of these
*/
class DigitalOutputPort : public CStateDeviceBase<DigitalOutputPort>,
	ErrorTranslator<DigitalOutputPort>
{
public:
	DigitalOutputPort(const std::string& port);
	virtual ~DigitalOutputPort();

	virtual int Initialize();
	virtual int Shutdown();

	virtual void GetName(char* name) const;
	virtual bool Busy() { return false; }

	virtual unsigned long GetNumberOfPositions()const { return numPos_; } // replace with numPos_ once API allows not creating Labels

	// action interface
	// ----------------
	int OnState(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBlanking(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBlankingTriggerDirection(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFirstStateSlider(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnNrOfStateSliders(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnLine(MM::PropertyBase* pProp, MM::ActionType eActEx, long ttlNr);
	int OnInputLine(MM::PropertyBase* pProp, MM::ActionType eAct);


private:
	int OnSequenceable(MM::PropertyBase* pProp, MM::ActionType eAct);
	NIDAQHub* GetHub() const
	{
		return static_cast<NIDAQHub*>(GetParentHub());
	}
	int TranslateHubError(int err);
	int SetState(long state);
	int StopTask();

	std::string niPort_;
	std::string triggerTerminal_;
	bool initialized_;
	bool sequenceRunning_;
	bool blanking_;
	bool blankOnLow_;
	bool open_;
	long pos_;
	long numPos_;
	uInt32 portWidth_;
	long inputLine_;
	long firstStateSlider_;
	long nrOfStateSliders_;
	bool neverSequenceable_;
	bool supportsBlankingAndSequencing_;

	// this can probably be done more elegantly using templates
	std::vector<uInt8> sequence8_;
	std::vector<uInt16> sequence16_;
	std::vector<uInt32> sequence32_;

	TaskHandle task_;
};

//////////////////////////////////////////////////////////////////////////////
// DAQ
//


class kcDAQ : public CSignalIOBase<kcDAQ>
{
public:
	kcDAQ();
	~kcDAQ();

	virtual int Initialize();
	virtual int Shutdown();

	virtual void GetName(char* name) const;
	virtual bool Busy() { return false; }

	virtual int SetGateOpen(bool open);
	virtual int GetGateOpen(bool& open);
	virtual int SetSignal(double /*volts*/) { return DEVICE_UNSUPPORTED_COMMAND; }
	virtual int GetSignal(double& volts);
	virtual int GetLimits(double& minVolts, double& maxVolts);

	virtual int IsDASequenceable(bool& isSequenceable) const;
	virtual int GetDASequenceMaxLength(long& maxLength) const;
	virtual int StartDASequence();
	virtual int StopDASequence();
	virtual int ClearDASequence();
	virtual int AddToDASequence(double voltage);
	virtual int SendDASequence();

private:
	bool initialized_;

	bool gateOpen_;
	double gatedVoltage_;
	bool sequenceRunning_;
	bool supportsTriggering_ = true;
	size_t maxSequenceLength_;

	double minVolts_;
	double maxVolts_;

	double offset1;
	double offset2;
	double offset3;
	double offset4;
	double pretriglength;
	int frameheader;
	int clockmode;
	int triggermode;
	double triggercount;

	double pulseperiod;
	double pulsewidth;

	double armhysteresis;
	double triggerchannel;
	double rasingcodevalue;
	double fallingcodevalue;

	double segmentduration;
	uint64_t once_trig_bytes;
	double smaplerate;
	double channelcount;
	double repetitionfrequency;

	std::vector<double> unsentSequence_;
	std::vector<double> sentSequence_;

	sem_t c2h_ping;		//ping信号量
	sem_t c2h_pong;		//pong信号量
	long once_readbytes = 8 MB;


private:
	int OnOffset(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPreTrigLength(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFrameHeader(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnClockMode(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTriggerMode(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPulsePeriod(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPulseWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnArmHysteresis(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTriggerChannel(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRasingCodevalue(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFallingCodevalue(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSegmentDuration(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRepetitionFrequency(MM::PropertyBase* pProp, MM::ActionType eAct);

	//int initializeBoard();
	int ChannelTriggerConfig();
	int dataConfig();
	static void* PollIntrEntry(void* arg) {
		kcDAQ* self = static_cast<kcDAQ*>(arg);
		self->PollIntr(nullptr);
		return nullptr;
	}
	static void* DatacollectEntry(void* arg) {
		kcDAQ* self = static_cast<kcDAQ*>(arg);
		self->datacollect(nullptr);
		return nullptr;
	}
	void* PollIntr(void* lParam);
	void* datacollect(void* lParam);
	int initializeTheadtoDisk();
	void printfLog(int nLevel, const char* fmt, ...);
private:


	struct data
	{
		uint64_t DMATotolbytes;
		uint64_t allbytes;
	};
	struct data data1;

};

//////////////////////////////////////////////////////////////////////////////
// ETL
//

class optotune : public CGenericBase < optotune >
{
public:
	optotune();
	virtual ~optotune();

	virtual int Initialize();
	virtual int Shutdown();
	void GetName(char* name) const;
	bool Busy() { return false; };

private:
	int		OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int		Baudrate(MM::PropertyBase* pProp, MM::ActionType eAct);
	int		onSetFP(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	std::string sendStart();
	float	getFPMin();
	float	getFPMax();
	float	getFP();
private:
	bool initialized_;

	std::string port;

	float FPMin = -293;
	float FPMax = 293;
};



//////////////////////////////////////////////////////////////////////////////
// CREATE HUB
//

class TPM : public HubBase<TPM>
{
public:
	TPM() :
		initialized_(false),
		busy_(false)
	{}
	~TPM() {}

	// Device API
	// ---------
	int Initialize();
	int Shutdown() { return DEVICE_OK; };
	void GetName(char* pName) const;
	bool Busy() { return busy_; };

	// HUB api
	int DetectInstalledDevices();

	int OnPortName(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDOPortName(MM::PropertyBase* pProp, MM::ActionType eAct);
	void SetPortName(const std::string& portName) { portName_ = portName; }
	void GetPortName(std::string& portName) const { portName = portName_; }
	void SetDOPortName(const std::string& portName) { DOportName_ = portName; }
	void GetDOPortName(std::string& portName) const { portName = DOportName_; }

	int OnTriggerAOSequence(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTriggerDOSequence(MM::PropertyBase* pProp, MM::ActionType eAct);
	NIDAQHub* GetNIDAQHubSafe();

private:
	std::string portName_;  // 用于存储端口名
	std::string DOportName_;  // 用于存储端口名

	int TriggerAOSequence();
	int StopAOSequence();

	int TriggerDOSequence();
	int StopDOSequence();

	void GetPeripheralInventory();

	std::vector<std::string> peripherals_;
	bool initialized_;
	bool busy_;
};