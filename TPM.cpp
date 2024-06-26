#include "TPM.h"
#include "DeviceBase.h"
#include "DeviceUtils.h"
#include "MMDevice.h"

#include "ModuleInterface.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/math/common_factor_rt.hpp>
#include <boost/scoped_array.hpp>

#include "pingpong_example.h"
#include "pthread.h"
#include "semaphore.h"
#include "../include/TraceLog.h"

const char* g_HubDeviceName = "TPM";
const char* g_DeviceNameNIDAQHub = "NIDAQHub";
const char* g_DeviceNameNIDAQAOPortPrefix = "NIDAQAO-";
const char* g_DeviceNameNIDAQDOPortPrefix = "NIDAQDO-";
const char* g_DeviceNameDAQ = "DAQ";
const char* g_DeviceNameoptotune = "optotune";
const char* g_DeviceNameTPMCamera = "TPMCamera";

const char* g_On = "On";
const char* g_Off = "Off";
const char* g_Low = "Low";
const char* g_High = "High";
const char* g_Never = "Never";
const char* g_UseHubSetting = "Use hub setting";
const char* g_Post = "Post";
const char* g_Pre = "Pre";

const int ERR_SEQUENCE_RUNNING = 2001;
const int ERR_SEQUENCE_TOO_LONG = 2002;
const int ERR_SEQUENCE_ZERO_LENGTH = 2003;
const int ERR_VOLTAGE_OUT_OF_RANGE = 2004;
const int ERR_NONUNIFORM_CHANNEL_VOLTAGE_RANGES = 2005;
const int ERR_VOLTAGE_RANGE_EXCEEDS_DEVICE_LIMITS = 2006;
const int ERR_UNKNOWN_PINS_PER_PORT = 2007;
const int ERR_INVALID_REQUEST = 2008;

///////////////////////////////////////////////////////////////////////////////////
///   adapter Essential component
///

MODULE_API void InitializeModuleData()
{
    RegisterDevice(g_DeviceNameNIDAQHub, MM::HubDevice, "NIDAQHub");
    RegisterDevice(g_HubDeviceName, MM::HubDevice, "TPM");
    RegisterDevice(g_DeviceNameDAQ, MM::SignalIODevice, "DAQ");
    RegisterDevice(g_DeviceNameoptotune, MM::GenericDevice, "optotune");
    RegisterDevice(g_DeviceNameTPMCamera, MM::CameraDevice, "Camera");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
    if (deviceName == 0)
        return 0;

    // decide which device class to create based on the deviceName parameter
    if (strcmp(deviceName, g_DeviceNameNIDAQHub) == 0)
    {
        // create NIDAQ
        return new NIDAQHub();
    }
    else if (std::string(deviceName).
        substr(0, strlen(g_DeviceNameNIDAQAOPortPrefix)) ==
        g_DeviceNameNIDAQAOPortPrefix)
    {
        return new NIAnalogOutputPort(std::string(deviceName).
            substr(strlen(g_DeviceNameNIDAQAOPortPrefix)));
    }
    else if (std::string(deviceName).substr(0, strlen(g_DeviceNameNIDAQDOPortPrefix)) ==
        g_DeviceNameNIDAQDOPortPrefix)
    {
        return new DigitalOutputPort(std::string(deviceName).
            substr(strlen(g_DeviceNameNIDAQDOPortPrefix)));
    }
    else if (strcmp(deviceName, g_HubDeviceName) == 0)
    {
        return new TPM();
    }
    else if (strcmp(deviceName, g_DeviceNameDAQ) == 0)
    {
        return new kcDAQ();
    }
    else if (strcmp(deviceName, g_DeviceNameoptotune) == 0)
    {
        return new optotune();
    }
    else if (strcmp(deviceName, g_DeviceNameTPMCamera) == 0)
    {
        return new TPMCamera();
    }

    // ...supplied name not recognized
    return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
    if (pDevice != nullptr) {
        // 仅当指针指向 kcDAQ 类型的对象时才删除
        if (dynamic_cast<kcDAQ*>(pDevice) != nullptr) {
            std::cout << "kcDAQ Device don't deleted" << std::endl;
        }
        else {
            delete pDevice;
            std::cout << "Pointer is not pointing to kcDAQ, skipping delete." << std::endl;
        }
    }
}


///////////////////////////////////////////////////////////////////////////////////
///   NIDAQ
///

NIDAQHub::NIDAQHub() :
    ErrorTranslator(20000, 20999, &NIDAQHub::SetErrorText),
    initialized_(false),
    maxSequenceLength_(1024),
    sequencingEnabled_(false),
    sequenceRunning_(false),
    minVolts_(0.0),
    maxVolts_(5.0),
    sampleRateHz_(10000.0),
    aoTask_(0),
    doTask_(0),
    doHub8_(0),
    doHub16_(0),
    doHub32_(0)
{
    // discover devices available on this computer and list them here
    std::string defaultDeviceName = "";
    int32 stringLength = DAQmxGetSysDevNames(NULL, 0);
    std::vector<std::string> result;
    if (stringLength > 0)
    {
        char* deviceNames = new char[stringLength];
        int32 nierr = DAQmxGetSysDevNames(deviceNames, stringLength);
        if (nierr == 0)
        {
            LogMessage(deviceNames, false);
            boost::split(result, deviceNames, boost::is_any_of(", "),
                boost::token_compress_on);
            defaultDeviceName = result[0];
        }
        else
        {
            LogMessage("No NIDAQ devicename found, false");
        }
        delete[] deviceNames;
    }


    CPropertyAction* pAct = new CPropertyAction(this, &NIDAQHub::OnDevice);
    int err = CreateStringProperty("Device", defaultDeviceName.c_str(), false, pAct, true);
    if (result.size() > 0)
    {
        for (std::string device : result)
        {
            AddAllowedValue("Device", device.c_str());
        }
    }

    pAct = new CPropertyAction(this, &NIDAQHub::OnMaxSequenceLength);
    err = CreateIntegerProperty("MaxSequenceLength",
        static_cast<long>(maxSequenceLength_), false, pAct, true);
    Initialize();
}


NIDAQHub::~NIDAQHub()
{
    Shutdown();
}


int NIDAQHub::Initialize()
{
    if (initialized_)
        return DEVICE_OK;

    if (!GetParentHub())
        return DEVICE_ERR;


    // Dynamically determine name of ChangeDetectionEvent for this device
    niChangeDetection_ = "/" + niDeviceName_ + "/ChangeDetectionEvent";
    niSampleClock_ = "/" + niDeviceName_ + "/do/SampleClock";

    // Determine the possible voltage range
    int err = GetVoltageRangeForDevice(niDeviceName_, minVolts_, maxVolts_);
    if (err != DEVICE_OK)
        return err;

    CPropertyAction* pAct = new CPropertyAction(this, &NIDAQHub::OnSequencingEnabled);
    err = CreateStringProperty("Sequence", sequencingEnabled_ ? g_On : g_Off, false, pAct);
    if (err != DEVICE_OK)
        return err;
    AddAllowedValue("Sequence", g_On);
    AddAllowedValue("Sequence", g_Off);

    std::vector<std::string> doPorts = GetDigitalPortsForDevice(niDeviceName_);
    if (doPorts.size() > 0)
    {
        // we could check if we actually have ports of these kinds, but the cost of instantiating all is low
        doHub8_ = new NIDAQDOHub<uInt8>(this);
        doHub16_ = new NIDAQDOHub<uInt16>(this);
        doHub32_ = new NIDAQDOHub<uInt32>(this);
    }

    std::vector<std::string> triggerPorts = GetAOTriggerTerminalsForDevice(niDeviceName_);
    if (!triggerPorts.empty())
    {
        niTriggerPort_ = triggerPorts[0];
        pAct = new CPropertyAction(this, &NIDAQHub::OnTriggerInputPort);
        err = CreateStringProperty("AOTriggerInputPort", niTriggerPort_.c_str(), false, pAct);
        if (err != DEVICE_OK)
            return err;
        for (std::vector<std::string>::const_iterator it = triggerPorts.begin(),
            end = triggerPorts.end();
            it != end; ++it)
        {
            AddAllowedValue("AOTriggerInputPort", it->c_str());
        }

        pAct = new CPropertyAction(this, &NIDAQHub::OnSampleRate);
        err = CreateFloatProperty("SampleRateHz", sampleRateHz_, false, pAct);
        if (err != DEVICE_OK)
            return err;
    }

    err = SwitchTriggerPortToReadMode();
    if (err != DEVICE_OK)
    {
        LogMessage("Failed to switch device " + niDeviceName_ + ", port " + niTriggerPort_ + " to read mode.");
        // do not return an error to allow the user to switch the triggerport to something that works
    }

    initialized_ = true;
    return DEVICE_OK;
}


int NIDAQHub::Shutdown()
{
    if (!initialized_)
        return DEVICE_OK;

    int err = StopTask(aoTask_);

    physicalAOChannels_.clear();
    aoChannelSequences_.clear();

    if (doHub8_ != 0)
        delete doHub8_;
    else if (doHub16_ != 0)
        delete doHub16_;
    else if (doHub32_ != 0)
        delete  doHub32_;

    initialized_ = false;
    return err;
}


void NIDAQHub::GetName(char* name) const
{
    CDeviceUtils::CopyLimitedString(name, g_DeviceNameNIDAQHub);
}


int NIDAQHub::DetectInstalledDevices()
{
    std::vector<std::string> aoPorts =
        GetAnalogPortsForDevice(niDeviceName_);

    for (std::vector<std::string>::const_iterator it = aoPorts.begin(), end = aoPorts.end();
        it != end; ++it)
    {
        MM::Device* pDevice =
            ::CreateDevice((g_DeviceNameNIDAQAOPortPrefix + *it).c_str());
        if (pDevice)
        {
            AddInstalledDevice(pDevice);
        }
    }

    std::vector<std::string> doPorts = GetDigitalPortsForDevice(niDeviceName_);

    for (std::vector<std::string>::const_iterator it = doPorts.begin(), end = doPorts.end();
        it != end; ++it)
    {
        MM::Device* pDevice =
            ::CreateDevice((g_DeviceNameNIDAQDOPortPrefix + *it).c_str());
        if (pDevice)
        {
            AddInstalledDevice(pDevice);
        }
    }

    return DEVICE_OK;
}


int NIDAQHub::GetVoltageLimits(double& minVolts, double& maxVolts)
{
    minVolts = minVolts_;
    maxVolts = maxVolts_;
    return DEVICE_OK;
}


int NIDAQHub::StartAOSequenceForPort(const std::string& port,
    const std::vector<double> sequence)
{
    int err = StopTask(aoTask_);
    if (err != DEVICE_OK)
        return err;

    err = AddAOPortToSequencing(port, sequence);
    if (err != DEVICE_OK)
        return err;

    err = StartAOSequencingTask();
    if (err != DEVICE_OK)
        return err;
    // We don't restart the task without this port on failure.
    // There is little point in doing so.

    return DEVICE_OK;
}


int NIDAQHub::StopAOSequenceForPort(const std::string& port)
{
    int err = StopTask(aoTask_);
    if (err != DEVICE_OK)
        return err;
    sequenceRunning_ = false;
    RemoveAOPortFromSequencing(port);
    // We do not restart sequencing for the remaining ports,
    // since it is meaningless (we can't preserve their state).

    // Make sure that the input trigger pin has a high impedance (i.e. does not 
    // somehow become an output pin
    return SwitchTriggerPortToReadMode();
}


int NIDAQHub::SwitchTriggerPortToReadMode()
{
    int err = StopTask(aoTask_);
    if (err != DEVICE_OK)
        return err;

    int32 nierr = DAQmxCreateTask((niDeviceName_ + "TriggerPinReadTask").c_str(), &aoTask_);
    if (nierr != 0)
        return TranslateNIError(nierr);
    LogMessage("Created Trigger pin read task", true);
    nierr = DAQmxCreateDIChan(aoTask_, niTriggerPort_.c_str(), "tIn", DAQmx_Val_ChanForAllLines);
    if (nierr != 0)
        return TranslateNIError(nierr);
    nierr = DAQmxStartTask(aoTask_);
    if (nierr != 0)
        return TranslateNIError(nierr);

    return DEVICE_OK;
}


int NIDAQHub::IsSequencingEnabled(bool& flag) const
{
    flag = sequencingEnabled_;
    return DEVICE_OK;
}


int NIDAQHub::GetSequenceMaxLength(long& maxLength) const
{
    maxLength = static_cast<long>(maxSequenceLength_);
    return DEVICE_OK;
}


int NIDAQHub::AddAOPortToSequencing(const std::string& port,
    const std::vector<double> sequence)
{
    if (sequence.size() > maxSequenceLength_)
        return ERR_SEQUENCE_TOO_LONG;

    RemoveAOPortFromSequencing(port);

    physicalAOChannels_.push_back(port);
    aoChannelSequences_.push_back(sequence);
    return DEVICE_OK;
}


void NIDAQHub::RemoveAOPortFromSequencing(const std::string& port)
{
    // We assume a given port appears at most once in physicalChannels_
    size_t n = physicalAOChannels_.size();
    for (size_t i = 0; i < n; ++i)
    {
        if (physicalAOChannels_[i] == port) {
            physicalAOChannels_.erase(physicalAOChannels_.begin() + i);
            aoChannelSequences_.erase(aoChannelSequences_.begin() + i);
            break;
        }
    }
}


int NIDAQHub::GetVoltageRangeForDevice(
    const std::string& device, double& minVolts, double& maxVolts)
{
    const int MAX_RANGES = 64;
    float64 ranges[2 * MAX_RANGES];
    for (int i = 0; i < MAX_RANGES; ++i)
    {
        ranges[2 * i] = 0.0;
        ranges[2 * i + 1] = 0.0;
    }

    int32 nierr = DAQmxGetDevAOVoltageRngs(device.c_str(), ranges,
        sizeof(ranges) / sizeof(float64));
    if (nierr != 0)
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        return TranslateNIError(nierr);
    }

    minVolts = ranges[0];
    maxVolts = ranges[1];
    for (int i = 0; i < MAX_RANGES; ++i)
    {
        if (ranges[2 * i] == 0.0 && ranges[2 * i + 1] == 0.0)
            break;
        LogMessage(("Possible voltage range " +
            boost::lexical_cast<std::string>(ranges[2 * i]) + " V to " +
            boost::lexical_cast<std::string>(ranges[2 * i + 1]) + " V").c_str(),
            true);
        if (ranges[2 * i + 1] > maxVolts)
        {
            minVolts = ranges[2 * i];
            maxVolts = ranges[2 * i + 1];
        }
    }
    LogMessage(("Selected voltage range " +
        boost::lexical_cast<std::string>(minVolts) + " V to " +
        boost::lexical_cast<std::string>(maxVolts) + " V").c_str(),
        true);

    return DEVICE_OK;
}


std::vector<std::string>
NIDAQHub::GetAOTriggerTerminalsForDevice(const std::string& device)
{
    std::vector<std::string> result;

    char ports[4096];
    int32 nierr = DAQmxGetDevTerminals(device.c_str(), ports, sizeof(ports));
    if (nierr == 0)
    {
        std::vector<std::string> terminals;
        boost::split(terminals, ports, boost::is_any_of(", "),
            boost::token_compress_on);

        // Only return the PFI terminals.
        for (std::vector<std::string>::const_iterator
            it = terminals.begin(), end = terminals.end();
            it != end; ++it)
        {
            if (it->find("PFI") != std::string::npos)
            {
                result.push_back(*it);
            }
        }
    }
    else
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        LogMessage("Cannot get list of trigger ports");
    }

    return result;
}


std::vector<std::string>
NIDAQHub::GetAnalogPortsForDevice(const std::string& device)
{
    std::vector<std::string> result;

    char ports[4096];
    int32 nierr = DAQmxGetDevAOPhysicalChans(device.c_str(), ports, sizeof(ports));
    if (nierr == 0)
    {
        boost::split(result, ports, boost::is_any_of(", "),
            boost::token_compress_on);
    }
    else
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        LogMessage("Cannot get list of analog ports");
    }

    return result;
}

std::vector<std::string>
NIDAQHub::GetDigitalPortsForDevice(const std::string& device)
{
    std::vector<std::string> result;

    char ports[4096];
    int32 nierr = DAQmxGetDevDOPorts(device.c_str(), ports, sizeof(ports));
    if (nierr == 0)
    {
        boost::split(result, ports, boost::is_any_of(", "),
            boost::token_compress_on);
    }
    else
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        LogMessage("Cannot get list of digital ports");
    }

    return result;
}


std::string NIDAQHub::GetPhysicalChannelListForSequencing(std::vector<std::string> channels) const
{
    std::string ret;
    for (std::vector<std::string>::const_iterator begin = channels.begin(),
        end = channels.end(), it = begin;
        it != end; ++it)
    {
        if (it != begin)
            ret += ", ";
        ret += *it;
    }
    return ret;
}


template<typename T>
inline int NIDAQHub::GetLCMSamplesPerChannel(size_t& seqLen, std::vector<std::vector<T>> channelSequences) const
{
    // Use an arbitrary but reasonable limit to prevent
    // overflow or excessive memory consumption.
    const uint64_t factorLimit = 2 << 14;

    uint64_t len = 1;
    for (unsigned int i = 0; i < channelSequences.size(); ++i)
    {
        uint64_t channelSeqLen = channelSequences[i].size();
        if (channelSeqLen > factorLimit)
        {
            return ERR_SEQUENCE_TOO_LONG;
        }
        if (channelSeqLen == 0)
        {
            return ERR_SEQUENCE_ZERO_LENGTH;
        }
        len = boost::math::lcm(len, channelSeqLen);
        if (len > factorLimit)
        {
            return ERR_SEQUENCE_TOO_LONG;
        }
    }
    seqLen = (size_t)len;
    return DEVICE_OK;
}


template<typename T>
void NIDAQHub::GetLCMSequence(T* buffer, std::vector<std::vector<T>> sequences) const
{
    size_t seqLen;
    if (GetLCMSamplesPerChannel(seqLen, sequences) != DEVICE_OK)
        return;

    for (unsigned int i = 0; i < sequences.size(); ++i)
    {
        size_t chanOffset = seqLen * i;
        size_t chanSeqLen = sequences[i].size();
        for (unsigned int j = 0; j < seqLen; ++j)
        {
            buffer[chanOffset + j] =
                sequences[i][j % chanSeqLen];
        }
    }
}


/**
* This task will start sequencing of all analog outputs that were previously added
* using AddAOPortToSequencing
* The triggerinputport has to be supported by the device.
* Specifically, a trigger input terminal is of the form /Dev1/PFI0, where
* there is a preceding slash.  Terminals that are part of an output port
* (such as Dev1/port0/line7) do not work.
* Uses DAQmxCfgSampClkTiming to transition to the next state for each
* anlog output at each consecutive rising flank of the trigger input terminal.
*
*/
int NIDAQHub::StartAOSequencingTask()
{
    if (aoTask_)
    {
        int err = StopTask(aoTask_);
        if (err != DEVICE_OK)
            return err;
    }

    LogMessage("Starting sequencing task", true);

    boost::scoped_array<float64> samples;

    size_t numChans = physicalAOChannels_.size();
    size_t samplesPerChan;
    int err = GetLCMSamplesPerChannel(samplesPerChan, aoChannelSequences_);
    if (err != DEVICE_OK)
        return err;

    LogMessage(boost::lexical_cast<std::string>(numChans) + " channels", true);
    LogMessage("LCM sequence length = " +
        boost::lexical_cast<std::string>(samplesPerChan), true);

    int32 nierr = DAQmxCreateTask("AOSeqTask", &aoTask_);
    if (nierr != 0)
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        return nierr;
    }
    LogMessage("Created task", true);

    const std::string chanList = GetPhysicalChannelListForSequencing(physicalAOChannels_);
    nierr = DAQmxCreateAOVoltageChan(aoTask_, chanList.c_str(),
        "AOSeqChan", minVolts_, maxVolts_, DAQmx_Val_Volts,
        NULL);
    if (nierr != 0)
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        goto error;
    }
    LogMessage(("Created AO voltage channel for: " + chanList).c_str(), true);

    nierr = DAQmxCfgSampClkTiming(aoTask_, niTriggerPort_.c_str(),
        sampleRateHz_, DAQmx_Val_Rising,
        DAQmx_Val_ContSamps, samplesPerChan);
    if (nierr != 0)
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        goto error;
    }
    LogMessage("Configured sample clock timing to use " + niTriggerPort_, true);

    samples.reset(new float64[samplesPerChan * numChans]);
    GetLCMSequence(samples.get(), aoChannelSequences_);

    int32 numWritten = 0;
    nierr = DAQmxWriteAnalogF64(aoTask_, static_cast<int32>(samplesPerChan),
        false, DAQmx_Val_WaitInfinitely, DAQmx_Val_GroupByChannel,
        samples.get(), &numWritten, NULL);
    if (nierr != 0)
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        goto error;
    }
    if (numWritten != static_cast<int32>(samplesPerChan))
    {
        LogMessage("Failed to write complete sequence");
        // This is presumably unlikely; no error code here
        goto error;
    }
    LogMessage("Wrote samples", true);

    nierr = DAQmxStartTask(aoTask_);
    if (nierr != 0)
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        goto error;
    }
    LogMessage("Started task", true);

    sequenceRunning_ = true;

    return DEVICE_OK;

error:
    DAQmxClearTask(aoTask_);
    aoTask_ = 0;
    err;
    if (nierr != 0)
    {
        LogMessage("Failed; task cleared");
        err = TranslateNIError(nierr);
    }
    else
    {
        err = DEVICE_ERR;
    }

    sequenceRunning_ = false;
    return err;
}


int NIDAQHub::StartDOBlankingAndOrSequence(const std::string& port, const uInt32 portWidth, const bool blankingOn,
    const bool sequenceOn, const long& pos, const bool blankingDirection, const std::string triggerPort)
{
    if (portWidth == 8)
        return doHub8_->StartDOBlankingAndOrSequence(port, blankingOn, sequenceOn, pos, blankingDirection, triggerPort);
    else if (portWidth == 16)
        return doHub16_->StartDOBlankingAndOrSequence(port, blankingOn, sequenceOn, pos, blankingDirection, triggerPort);
    else if (portWidth == 32)
        return doHub32_->StartDOBlankingAndOrSequence(port, blankingOn, sequenceOn, pos, blankingDirection, triggerPort);

    return ERR_UNKNOWN_PINS_PER_PORT;
}

int NIDAQHub::StartDOBlankingAndOrSequenceWithoutTrigger(const std::string& port, const bool blankingOn, const bool sequenceOn,
    const long& pos, const bool blankOnLow, int32 num)
{
    return doHub32_->StartDOBlankingAndOrSequenceWithoutTrigger(port, blankingOn, sequenceOn, pos, blankOnLow, num);
}

int NIDAQHub::StopDOBlankingAndSequence(const uInt32 portWidth)
{
    if (portWidth == 8)
        return doHub8_->StopDOBlankingAndSequence();
    else if (portWidth == 16)
        return doHub16_->StopDOBlankingAndSequence();
    else if (portWidth == 32)
        return doHub32_->StopDOBlankingAndSequence();

    return ERR_UNKNOWN_PINS_PER_PORT;
}


int NIDAQHub::SetDOPortState(std::string port, uInt32 portWidth, long state)
{
    if (doTask_)
    {
        int err = StopTask(doTask_);
        if (err != DEVICE_OK)
            return err;
    }

    LogMessage("Starting on-demand task", true);

    int32 nierr = DAQmxCreateTask(NULL, &doTask_);
    if (nierr != 0)
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        return TranslateNIError(nierr);
    }
    LogMessage("Created task", true);

    nierr = DAQmxCreateDOChan(doTask_, port.c_str(), NULL, DAQmx_Val_ChanForAllLines);
    if (nierr != 0)
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        goto error;
    }
    LogMessage("Created DO channel", true);

    int32 numWritten = 0;
    if (portWidth == 8)
    {
        uInt8 samples[1];
        samples[0] = (uInt8)state;
        nierr = DAQmxWriteDigitalU8(doTask_, 1,
            true, DAQmx_Val_WaitInfinitely, DAQmx_Val_GroupByChannel,
            samples, &numWritten, NULL);

    }
    else if (portWidth == 16)
    {
        uInt16 samples[1];
        samples[0] = (uInt16)state;
        nierr = DAQmxWriteDigitalU16(doTask_, 1,
            true, DAQmx_Val_WaitInfinitely, DAQmx_Val_GroupByChannel,
            samples, &numWritten, NULL);
    }
    else if (portWidth == 32)
    {
        uInt32 samples[1];
        samples[0] = (uInt32)state;
        nierr = DAQmxWriteDigitalU32(doTask_, 1,
            true, DAQmx_Val_WaitInfinitely, DAQmx_Val_GroupByChannel,
            samples, &numWritten, NULL);
    }
    else
    {
        LogMessage(("Found invalid number of pins per port: " +
            boost::lexical_cast<std::string>(portWidth)).c_str(), true);
        goto error;
    }
    if (nierr != 0)
    {
        LogMessage(GetNIDetailedErrorForMostRecentCall().c_str());
        goto error;
    }
    if (numWritten != 1)
    {
        LogMessage("Failed to write voltage");
        // This is presumably unlikely; no error code here
        goto error;
    }
    LogMessage(("Wrote Digital out with task autostart: " +
        boost::lexical_cast<std::string>(state)).c_str(), true);

    return DEVICE_OK;

error:
    DAQmxClearTask(doTask_);
    doTask_ = 0;
    int err;
    if (nierr != 0)
    {
        LogMessage("Failed; task cleared");
        err = TranslateNIError(nierr);
    }
    else
    {
        err = DEVICE_ERR;
    }
    return err;
}


int NIDAQHub::StopTask(TaskHandle& task)
{
    if (!task)
        return DEVICE_OK;

    int32 nierr = DAQmxClearTask(task);
    if (nierr != 0)
        return TranslateNIError(nierr);
    task = 0;
    LogMessage("Stopped task", true);

    return DEVICE_OK;
}


int NIDAQHub::OnDevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(niDeviceName_.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        std::string deviceName;
        pProp->Get(deviceName);
        niDeviceName_ = deviceName;
    }
    return DEVICE_OK;
}


int NIDAQHub::OnMaxSequenceLength(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(static_cast<long>(maxSequenceLength_));
    }
    else if (eAct == MM::AfterSet)
    {
        long maxLength;
        pProp->Get(maxLength);
        if (maxLength < 0)
        {
            maxLength = 0;
            pProp->Set(maxLength);
        }
        maxSequenceLength_ = maxLength;
    }
    return DEVICE_OK;
}


int NIDAQHub::OnSequencingEnabled(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(sequencingEnabled_ ? g_On : g_Off);
    }
    else if (eAct == MM::AfterSet)
    {
        std::string sw;
        pProp->Get(sw);
        sequencingEnabled_ = (sw == g_On);
    }
    return DEVICE_OK;
}


int NIDAQHub::OnTriggerInputPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(niTriggerPort_.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        if (sequenceRunning_)
            return ERR_SEQUENCE_RUNNING;

        std::string port;
        pProp->Get(port);
        niTriggerPort_ = port;
        return SwitchTriggerPortToReadMode();
    }
    return DEVICE_OK;
}


int NIDAQHub::OnSampleRate(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(sampleRateHz_);
    }
    else if (eAct == MM::AfterSet)
    {
        if (sequenceRunning_)
            return ERR_SEQUENCE_RUNNING;

        double rateHz;
        pProp->Get(rateHz);
        if (rateHz <= 0.0)
        {
            rateHz = 1.0;
            pProp->Set(rateHz);
        }
        sampleRateHz_ = rateHz;
    }
    return DEVICE_OK;
}


//
// NIDAQDOHub
//


template<typename Tuint>
NIDAQDOHub<Tuint>::NIDAQDOHub(NIDAQHub* hub) : diTask_(0), doTask_(0), hub_(hub)
{
    if (typeid(Tuint) == typeid(uInt8))
        portWidth_ = 8;
    else if (typeid(Tuint) == typeid(uInt8))
        portWidth_ = 16;
    else if (typeid(Tuint) == typeid(uInt32))
        portWidth_ = 32;
    else
        portWidth_ = 0;
}


template<typename Tuint>
NIDAQDOHub<Tuint>::~NIDAQDOHub<Tuint>()
{
    hub_->StopTask(doTask_);

    physicalDOChannels_.clear();
    doChannelSequences_.clear();
}


template<typename Tuint>
int NIDAQDOHub<Tuint>::AddDOPortToSequencing(const std::string& port, const std::vector<Tuint> sequence)
{
    long maxSequenceLength;
    hub_->GetSequenceMaxLength(maxSequenceLength);
    if (sequence.size() > maxSequenceLength)
        return ERR_SEQUENCE_TOO_LONG;

    RemoveDOPortFromSequencing(port);

    physicalDOChannels_.push_back(port);
    doChannelSequences_.push_back(sequence);
    return DEVICE_OK;
}


template<typename Tuint>
inline void NIDAQDOHub<Tuint>::RemoveDOPortFromSequencing(const std::string& port)
{
    size_t n = physicalDOChannels_.size();
    for (size_t i = 0; i < n; ++i)
    {
        if (physicalDOChannels_[i] == port) {
            physicalDOChannels_.erase(physicalDOChannels_.begin() + i);
            doChannelSequences_.erase(doChannelSequences_.begin() + i);
        }
    }
}


template<class Tuint>
int NIDAQDOHub<Tuint>::StartDOBlankingAndOrSequenceWithoutTrigger(const std::string& port, const bool blankingOn, const bool sequenceOn,
    const long& pos, const bool blankOnLow, int32 num)
{
    if (!blankingOn && !sequenceOn)
    {
        return ERR_INVALID_REQUEST;
    }

    int err = hub_->SetDOPortState(port, portWidth_, blankOnLow ? 0 : pos);
    if (err != DEVICE_OK)
        return err;

    err = hub_->StopTask(doTask_);
    if (err != DEVICE_OK)
        return err;

    int32 nierr = DAQmxCreateTask("DOBlankTask", &doTask_);
    if (nierr != 0)
    {
        return hub_->TranslateNIError(nierr);
    }
    hub_->LogMessage("Created DO task", true);

    nierr = DAQmxCreateDOChan(doTask_, port.c_str(), "DOSeqChan", DAQmx_Val_ChanForAllLines);
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Created DO channel for: " + port, true);

    int32 number = num * 2; // 每个上升沿对应两个样本点
    boost::scoped_array<Tuint> samples;
    samples.reset(new Tuint[number]);

    // 生成信号序列
    for (int i = 0; i < num; ++i)
    {
        samples.get()[2 * i] = blankOnLow ? pos : 0;       // 第一个样本点
        samples.get()[2 * i + 1] = blankOnLow ? 0 : pos;  // 第二个样本点，产生上升沿
    }

    double sampleRateHz = hub_->sampleRateHz_;  // 这里设置内部时钟的频率
    nierr = DAQmxCfgSampClkTiming(doTask_, "", 2 * sampleRateHz, DAQmx_Val_Rising, DAQmx_Val_ContSamps, number);
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Configured sample clock timing to use internal clock with frequency: " + std::to_string(sampleRateHz) + " Hz", true);


    int32 numWritten = 0;
    nierr = DaqmxWriteDigital(doTask_, static_cast<int32>(number), samples.get(), &numWritten);


    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    if (numWritten != static_cast<int32>(number))
    {
        hub_->LogMessage("Failed to write complete sequence");
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Wrote samples", true);

    nierr = DAQmxStartTask(doTask_);
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Started DO task", true);

    return DEVICE_OK;
}


template<class Tuint>
int NIDAQDOHub<Tuint>::StartDOBlankingAndOrSequence(const std::string& port, const bool blankingOn, const bool sequenceOn,
    const long& pos, const bool blankOnLow, const std::string triggerPort)
{
    if (!blankingOn && !sequenceOn)
    {
        return ERR_INVALID_REQUEST;
    }
    // First read the state of the triggerport, since we will only get changes of triggerPort, not
    // its actual state.
    bool triggerPinState;
    int err = GetPinState(triggerPort, triggerPinState);
    if (err != DEVICE_OK)
        return err;

    //Set initial state based on blankOnLow and triggerPort state
    if (blankOnLow ^ triggerPinState)
        err = hub_->SetDOPortState(port, portWidth_, 0);
    else
        err = hub_->SetDOPortState(port, portWidth_, pos);
    if (err != DEVICE_OK)
        return err;

    err = hub_->StopTask(diTask_);
    if (err != DEVICE_OK)
        return err;

    int32 nierr = DAQmxCreateTask("DIChangeTask", &diTask_);
    if (nierr != 0)
    {
        return hub_->TranslateNIError(nierr);;
    }
    hub_->LogMessage("Created DI task", true);

    int32 number = 2;
    std::vector<Tuint> doSequence_;
    if (sequenceOn)
    {
        int i = 0;
        for (std::string pChan : physicalDOChannels_)
        {
            if (port == pChan)
            {
                doSequence_ = doChannelSequences_[i];
                number = 2 * (int32)doSequence_.size();
            }
            i++;
        }
    }

    nierr = DAQmxCreateDIChan(diTask_, triggerPort.c_str(), "DIBlankChan", DAQmx_Val_ChanForAllLines);
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Created DI channel for: " + triggerPort, true);

    // Note, if triggerPort is not part of the port, we'll likely start seeing errors here
    // This needs to be in the documentation
    nierr = DAQmxCfgChangeDetectionTiming(diTask_, triggerPort.c_str(),
        triggerPort.c_str(), DAQmx_Val_ContSamps, number);
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Configured change detection timing to use " + triggerPort, true);

    // this is only here to monitor the ChangeDetectionEvent, delete after debugging

    nierr = DAQmxExportSignal(diTask_, DAQmx_Val_ChangeDetectionEvent, hub_->niSampleClock_.c_str());
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Routed change detection timing to  " + hub_->niChangeDetection_, true);

    nierr = DAQmxStartTask(diTask_);
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Started DI task", true);

    // end routing changedetectionEvent


    // Change detection now should be running on the input port.  
    // Configure a task to use change detection as the input
    err = hub_->StopTask(doTask_);
    if (err != DEVICE_OK)
        return err;

    nierr = DAQmxCreateTask("DOBlankTask", &doTask_);
    if (nierr != 0)
    {
        return hub_->TranslateNIError(nierr);;
    }
    hub_->LogMessage("Created DO task", true);

    nierr = DAQmxCreateDOChan(doTask_, port.c_str(), "DOSeqChan", DAQmx_Val_ChanForAllLines);
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Created DO channel for: " + port, true);

    boost::scoped_array<Tuint> samples;
    samples.reset(new Tuint[number]);
    if (sequenceOn && doSequence_.size() > 0)
    {
        if (blankOnLow ^ triggerPinState)
        {
            for (uInt32 i = 0; i < doSequence_.size(); i++)
            {
                samples.get()[2 * i] = doSequence_[i];
                if (blankingOn)
                    samples.get()[2 * i + 1] = 0;
                else
                    samples.get()[2 * i + 1] = doSequence_[i];
            }
        }
        else
        {
            for (uInt32 i = 0; i < doSequence_.size(); i++)
            {
                if (blankingOn)
                    samples.get()[2 * i] = 0;
                else
                    samples.get()[2 * i] = doSequence_[i];

                samples.get()[2 * i + 1] = doSequence_[i];
            }
        }
    }
    else  // assume that blanking is on, otherwise things make no sense
    {
        if (blankOnLow ^ triggerPinState)
        {
            samples.get()[0] = (Tuint)pos;
            samples.get()[1] = 0;
        }
        else
        {
            samples.get()[0] = 0;
            samples.get()[1] = (Tuint)pos;
        }
    }

    nierr = DAQmxCfgSampClkTiming(doTask_, hub_->niChangeDetection_.c_str(),
        hub_->sampleRateHz_, DAQmx_Val_Rising, DAQmx_Val_ContSamps, number);
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Configured sample clock timing to use " + hub_->niChangeDetection_, true);

    int32 numWritten = 0;
    nierr = DaqmxWriteDigital(doTask_, static_cast<int32>(number), samples.get(), &numWritten);

    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    if (numWritten != static_cast<int32>(number))
    {
        hub_->LogMessage("Failed to write complete sequence");
        // This is presumably unlikely; no error code here
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Wrote samples", true);

    nierr = DAQmxStartTask(doTask_);
    if (nierr != 0)
    {
        return HandleTaskError(nierr);
    }
    hub_->LogMessage("Started DO task", true);

    return DEVICE_OK;
}


template<class Tuint>
int NIDAQDOHub<Tuint>::GetPinState(const std::string pinDesignation, bool& state)
{
    int err = hub_->StopTask(diTask_);
    if (err != DEVICE_OK)
        return err;

    int32 nierr = DAQmxCreateTask("DIReadTriggerPinTask", &diTask_);
    if (nierr != 0)
    {
        return hub_->TranslateNIError(nierr);;
    }
    hub_->LogMessage("Created DI task", true);
    nierr = DAQmxCreateDIChan(diTask_, pinDesignation.c_str(), "tIn", DAQmx_Val_ChanForAllLines);
    if (nierr != 0)
    {
        return hub_->TranslateNIError(nierr);;
    }
    nierr = DAQmxStartTask(diTask_);
    if (nierr != 0)
    {
        return hub_->TranslateNIError(nierr);;
    }
    uInt8 readArray[1];
    int32 read;
    int32 bytesPerSample;
    nierr = DAQmxReadDigitalLines(diTask_, 1, 0, DAQmx_Val_GroupByChannel, readArray, 1, &read, &bytesPerSample, NULL);
    if (nierr != 0)
    {
        return hub_->TranslateNIError(nierr);;
    }
    state = readArray[0] != 0;
    return DEVICE_OK;
}


template<class Tuint>
int NIDAQDOHub<Tuint>::StopDOBlankingAndSequence()
{
    hub_->StopTask(doTask_); // even if this fails, we still want to stop the diTask_
    return hub_->StopTask(diTask_);
}


template<class Tuint>
int NIDAQDOHub<Tuint>::HandleTaskError(int32 niError)
{
    std::string niErrorMsg;
    if (niError != 0)
    {
        niErrorMsg = GetNIDetailedErrorForMostRecentCall();
        hub_->LogMessage(niErrorMsg.c_str());
    }
    DAQmxClearTask(diTask_);
    diTask_ = 0;
    DAQmxClearTask(doTask_);
    doTask_ = 0;
    int err = DEVICE_OK;;
    if (niError != 0)
    {
        err = hub_->TranslateNIError(niError);
        hub_->SetErrorText(err, niErrorMsg.c_str());
    }
    return err;
}


template<class Tuint>
int NIDAQDOHub<Tuint>::DaqmxWriteDigital(TaskHandle doTask_, int32 samplesPerChan, const Tuint* samples, int32* numWritten)
{
    return ERR_UNKNOWN_PINS_PER_PORT;
}


template<>
int NIDAQDOHub<uInt8>::DaqmxWriteDigital(TaskHandle doTask, int32 samplesPerChan, const uInt8* samples, int32* numWritten)
{
    return DAQmxWriteDigitalU8(doTask, samplesPerChan,
        false, DAQmx_Val_WaitInfinitely, DAQmx_Val_GroupByChannel,
        samples, numWritten, NULL);
}


template<>
int NIDAQDOHub<uInt16>::DaqmxWriteDigital(TaskHandle doTask, int32 samplesPerChan, const uInt16* samples, int32* numWritten)
{
    return DAQmxWriteDigitalU16(doTask, samplesPerChan,
        false, DAQmx_Val_WaitInfinitely, DAQmx_Val_GroupByChannel,
        samples, numWritten, NULL);
}


template<>
int NIDAQDOHub<uInt32>::DaqmxWriteDigital(TaskHandle doTask, int32 samplesPerChan, const uInt32* samples, int32* numWritten)
{
    return DAQmxWriteDigitalU32(doTask, samplesPerChan,
        false, DAQmx_Val_WaitInfinitely, DAQmx_Val_GroupByChannel,
        samples, numWritten, NULL);
}


template class NIDAQDOHub<uInt8>;
template class NIDAQDOHub<uInt16>;
template class NIDAQDOHub<uInt32>;



///////////////////////////////////////////////////////////////////////////////////
///   DAQ 
///
kcDAQ::kcDAQ() :
    initialized_(false),
    gateOpen_(true),
    gatedVoltage_(0.0),
    sequenceRunning_(false),
    maxSequenceLength_(1024),
    minVolts_(0.0),
    maxVolts_(0.2),
    offset1(8064),
    offset2(8088),
    offset3(8072),
    offset4(8092),
    pretriglength(0.0),
    frameheader(1),
    clockmode(0),
    triggermode(1),
    triggercount(0.0),
    pulseperiod(0.0),
    pulsewidth(0.0),
    armhysteresis(70.0),
    triggerchannel(1),
    rasingcodevalue(0),
    fallingcodevalue(-0),
    segmentduration(0),
    once_trig_bytes(0),
    smaplerate(1000.0),
    channelcount(4.0),
    data1({ 0,0 }),
    repetitionfrequency(800)
{
    InitializeDefaultErrorMessages();
    pthread_mutex_init(&mutex_, NULL);  // 初始化互斥锁
}

STXDMA_CARDINFO pstCardInfo;
kcDAQ::~kcDAQ()
{
    pthread_mutex_destroy(&mutex_);  // 销毁互斥锁
}

int kcDAQ::Initialize()
{
    if (initialized_)
        return DEVICE_OK;
    initializeTheadtoDisk();
    int err = QTXdmaOpenBoard(&pstCardInfo, 0);
    QT_BoardGetCardInfo();
    QT_BoardSetADCStop();
    QT_BoardSetTransmitMode(0, 0);
    QT_BoardSetInterruptClear();
    QT_BoardSetSoftReset();
    // 调节通道偏置
    CPropertyAction* pAct = new CPropertyAction(this, &kcDAQ::OnOffset);
    err = CreateFloatProperty("Channel1 offset", offset1, false, pAct);
    if (err != DEVICE_OK)
        return err;
    err = CreateFloatProperty("Channel2 offset", offset2, false, pAct);
    if (err != DEVICE_OK)
        return err;
    err = CreateFloatProperty("Channel3 offset", offset3, false, pAct);
    if (err != DEVICE_OK)
        return err;
    err = CreateFloatProperty("Channel4 offset", offset4, false, pAct);
    if (err != DEVICE_OK)
        return err;
    // 预触发
    pAct = new CPropertyAction(this, &kcDAQ::OnPreTrigLength);
    err = CreateFloatProperty("Pre Trigger Length", pretriglength, false, pAct);
    // 帧头使能
    pAct = new CPropertyAction(this, &kcDAQ::OnFrameHeader);
    err = CreateIntegerProperty("Frame header", frameheader, false, pAct);
    SetPropertyLimits("Frame header", 0, 1);
    // 时钟模式
    pAct = new CPropertyAction(this, &kcDAQ::OnClockMode);
    err = CreateIntegerProperty("Clock Mode", clockmode, false, pAct);
    SetPropertyLimits("Clock Mode", 0, 2);
    // 触发模式
    pAct = new CPropertyAction(this, &kcDAQ::OnTriggerMode);
    err = CreateIntegerProperty("Trigger Mode", triggermode, false, pAct);
    SetPropertyLimits("Trigger Mode", 0, 7);
    // 内部脉冲触发设置
    /// 脉冲周期
    pAct = new CPropertyAction(this, &kcDAQ::OnPulsePeriod);
    err = CreateFloatProperty("Pulse Period", pulseperiod, false, pAct);
    /// 脉冲宽度
    pAct = new CPropertyAction(this, &kcDAQ::OnPulseWidth);
    err = CreateFloatProperty("Pulse Width", pulsewidth, false, pAct);
    // 通道触发设置
    /// 触发迟滞
    pAct = new CPropertyAction(this, &kcDAQ::OnArmHysteresis);
    err = CreateFloatProperty("Arm Hysteresis", armhysteresis, true, pAct);
    /// 触发通道
    pAct = new CPropertyAction(this, &kcDAQ::OnTriggerChannel);
    err = CreateFloatProperty("Trigger Channel", triggerchannel, true, pAct);
    SetPropertyLimits("Trigger Channel", 1, 4);
    /// 上升沿阈值
    pAct = new CPropertyAction(this, &kcDAQ::OnRasingCodevalue);
    err = CreateFloatProperty("Rasing Codevalue", rasingcodevalue, true, pAct);
    /// 下降沿阈值
    pAct = new CPropertyAction(this, &kcDAQ::OnFallingCodevalue);
    err = CreateFloatProperty("Falling Codevalue", fallingcodevalue, true, pAct);
    // 单次触发段时长
    pAct = new CPropertyAction(this, &kcDAQ::OnSegmentDuration);
    err = CreateFloatProperty("Segment Duration", segmentduration, false, pAct);
    SetPropertyLimits("Segment Duration", 0, 536870.904);
    // 触发频率
    pAct = new CPropertyAction(this, &kcDAQ::OnRepetitionFrequency);
    err = CreateFloatProperty("Repetition Frequency", repetitionfrequency, false, pAct);
    SetPropertyLimits("Repetition Frequency", 800, 99999999999);
    initialized_ = true;
    return DEVICE_OK;
}

int kcDAQ::Shutdown()
{
    if (!initialized_)
        return DEVICE_OK;
    int err = 0;
    err = QT_BoardSetADCStop();
    err = QT_BoardSetTransmitMode(0, 0);
    err = QTXdmaCloseBoard(&pstCardInfo);
    initialized_ = false;
    return DEVICE_OK;
}

void kcDAQ::GetName(char* name) const
{
    CDeviceUtils::CopyLimitedString(name, g_DeviceNameDAQ);
}

// 粗糙
int kcDAQ::SetGateOpen(bool open)
{
    gateOpen_ = open;
    return DEVICE_OK;
}

int kcDAQ::GetGateOpen(bool& open)
{
    open = gateOpen_;
    return DEVICE_OK;
}

int kcDAQ::GetSignal(double& volts)
{
    volts = gatedVoltage_;
    return DEVICE_OK;
}

int kcDAQ::GetLimits(double& minVolts, double& maxVolts)
{
    minVolts = minVolts_;
    maxVolts = maxVolts_;
    return DEVICE_OK;
}

int kcDAQ::IsDASequenceable(bool& isSequenceable) const
{
    isSequenceable = supportsTriggering_;
    return DEVICE_OK;
}

int kcDAQ::GetDASequenceMaxLength(long& maxLength) const
{
    maxLength = static_cast<long>(maxSequenceLength_);
    return DEVICE_OK;
}

int kcDAQ::StartDASequence()
{
    //DMA参数设置
    QT_BoardSetFifoMultiDMAParameter(once_trig_bytes, data1.DMATotolbytes);
    //DMA传输模式设置
    QT_BoardSetTransmitMode(1, 0);
    //使能PCIE中断
    QT_BoardSetInterruptSwitch();
    ////数据采集线程
    //pthread_t data_collect;
    //pthread_create(&data_collect, NULL, DatacollectEntry, &data1);



    //ADC开始采集
    QT_BoardSetADCStart();

    // 等中断线程
    pthread_t wait_intr_c2h_0;
    ThreadParams* params = new ThreadParams{ this };
    int ret = pthread_create(&wait_intr_c2h_0, NULL, PollIntrEntry, static_cast<void*>(params));
    if (ret != 0) {
        delete params;  // 如果线程创建失败，释放参数
        return DEVICE_ERR;
    }

    ////等中断线程
    //pthread_t wait_intr_c2h_0;
    //pthread_create(&wait_intr_c2h_0, NULL, PollIntrEntry, NULL);
    sequenceRunning_ = true;
    return DEVICE_OK;
}

int kcDAQ::StopDASequence()
{
    QT_BoardSetADCStop();
    printf("set adc stop......\n");
    QT_BoardSetTransmitMode(0, 0);
    printf("set DMA stop......\n");
    sequenceRunning_ = false;
    return DEVICE_OK;
}

int kcDAQ::ClearDASequence()
{
    unsentSequence_.clear();
    return DEVICE_OK;
}

int kcDAQ::AddToDASequence(double voltage)
{
    if (voltage < minVolts_ || voltage > maxVolts_)
        return DEVICE_ERR;

    unsentSequence_.push_back(voltage);
    return DEVICE_OK;
}

int kcDAQ::SendDASequence()
{
    if (sequenceRunning_)
        return DEVICE_ERR;

    sentSequence_ = unsentSequence_;
    // We don't actually "write" the sequence here, because writing
    // needs to take place once the correct task has been set up for
    // all of the AO channels.
    return DEVICE_OK;
}

int kcDAQ::OnOffset(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    std::string propName = pProp->GetName();  // 获取触发回调的属性名称
    if (eAct == MM::AfterSet)
    {
        double offset;
        pProp->Get(offset); // 从属性中获取用户设置的新偏移值

        int channelID = 0;  // 初始化通道ID
        if (propName == "Channel1 offset")
        {
            channelID = 1;
        }
        else if (propName == "Channel2 offset")
        {
            channelID = 2;
        }
        else if (propName == "Channel3 offset")
        {
            channelID = 3;
        }
        else if (propName == "Channel4 offset")
        {
            channelID = 4;
        }

        if (channelID > 0) {  // 检查channelID是否有效
            int err = QT_BoardSetOffset(channelID, offset);
            if (err != DEVICE_OK)
                return err;
        }
    }
    return DEVICE_OK;
}
int kcDAQ::OnPreTrigLength(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        pProp->Get(pretriglength);
        int err = QT_BoardSetPerTrigger(pretriglength);
        return err;
    }
    return DEVICE_OK;
}
int kcDAQ::OnFrameHeader(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        double fh;  // 用于存储获取到的属性值
        pProp->Get(fh);  // 正确传递参数
        frameheader = static_cast<int>(fh);
        int err = QT_BoardSetFrameheader(frameheader);
        return err;
    }
    return DEVICE_OK;
}
int kcDAQ::OnClockMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        double cm;  // 用于存储获取到的属性值
        pProp->Get(cm);  // 正确传递参数
        clockmode = static_cast<int>(cm);
        int err = QT_BoardSetClockMode(clockmode);
        return err;
    }
    return DEVICE_OK;
}
int kcDAQ::OnTriggerMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        double tm;  // 用于存储获取到的属性值
        pProp->Get(tm);  // 正确传递参数
        triggermode = static_cast<int>(tm);
        // 设置readonly
        if (triggermode != 1)
        {
            SetPropertyReadOnly("Pulse Period", true);
            SetPropertyReadOnly("Pulse Width", true);
            SetPropertyReadOnly("Repetition Frequency", true);
        }
        if (triggermode != 4 && triggermode != 6)
        {
            SetPropertyReadOnly("Arm Hysteresis", true);
            SetPropertyReadOnly("Trigger Channel", true);
            SetPropertyReadOnly("Rasing Codevalue", true);
        }
        if (triggermode != 5 && triggermode != 6)
        {
            SetPropertyReadOnly("Arm Hysteresis", true);
            SetPropertyReadOnly("Trigger Channel", true);
            SetPropertyReadOnly("Falling Codevalue", true);
        }
        // 调整触发模式
        if (triggermode == 0)
        {
            int err = QT_BoardSoftTrigger(); //软件触发
        }
        else if (triggermode == 1)
        {
            SetPropertyReadOnly("Pulse Period", false);
            SetPropertyReadOnly("Pulse Width", false);
            SetPropertyReadOnly("Repetition Frequency", false);
        }
        else if (triggermode == 2)
        {
            QT_BoardExternalTrigger(2, triggercount);
        }
        else if (triggermode == 3)
        {
            QT_BoardExternalTrigger(3, triggercount);
        }
        else if (triggermode == 7)
        {
            QT_BoardExternalTrigger(7, triggercount);
        }
        else if (triggermode == 4 || triggermode == 5 || triggermode == 6)
        {
            SetPropertyReadOnly("Arm Hysteresis", false);
            SetPropertyReadOnly("Trigger Channel", false);
            if (triggermode == 4)
            {
                SetPropertyReadOnly("Rasing Codevalue", false);
                ChannelTriggerConfig();
            }
            else if (triggermode == 5)
            {
                SetPropertyReadOnly("Falling Codevalue", false);
                ChannelTriggerConfig();
            }
            else if (triggermode == 6)
            {
                SetPropertyReadOnly("Rasing Codevalue", false);
                SetPropertyReadOnly("Falling Codevalue", false);
                ChannelTriggerConfig();
            }
        }

    }
    return DEVICE_OK;
}
int kcDAQ::OnPulsePeriod(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        pProp->Get(pulseperiod);  // 正确传递参数
    }
    return DEVICE_OK;
}
int kcDAQ::OnPulseWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        pProp->Get(pulsewidth);  // 正确传递参数
    }
    return DEVICE_OK;
}
int kcDAQ::OnArmHysteresis(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        pProp->Get(armhysteresis);  // 正确传递参数
        QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x3C, armhysteresis);
        ChannelTriggerConfig();
    }
    return DEVICE_OK;
}
int kcDAQ::OnTriggerChannel(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        pProp->Get(triggerchannel);  // 正确传递参数
        ChannelTriggerConfig();
    }
    return DEVICE_OK;
}
int kcDAQ::OnRasingCodevalue(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        pProp->Get(rasingcodevalue);  // 正确传递参数
        ChannelTriggerConfig();
    }
    return DEVICE_OK;
}
int kcDAQ::OnFallingCodevalue(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        pProp->Get(fallingcodevalue);  // 正确传递参数
        ChannelTriggerConfig();
    }
    return DEVICE_OK;
}
int kcDAQ::OnSegmentDuration(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        pProp->Get(segmentduration);  // 正确传递参数
        dataConfig();
    }
    return DEVICE_OK;
}
int kcDAQ::OnRepetitionFrequency(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        pProp->Get(repetitionfrequency);  // 正确传递参数
        dataConfig();
    }
    return DEVICE_OK;
}


// 其他函数
int kcDAQ::ChannelTriggerConfig()
{
    int err = QT_BoardChannelTrigger(triggermode, triggercount, triggerchannel, rasingcodevalue, fallingcodevalue);
    return err;
}
int kcDAQ::dataConfig()
{
    once_trig_bytes = segmentduration * smaplerate * channelcount * 2;
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
        return DEVICE_ERR;
    }
    //通过单次触发段时长计算的最大触发频率的理论值
    double TriggerFrequency = (1 / segmentduration) * 1000000;
    uint64_t GB = 4000000000;
    //依据流盘速度不大于4GB/S计算处推荐的最大触发频率
    if (triggermode != 0 && triggermode != 1)
    {
        if (triggermode == 6 || triggermode == 7)
        {
            TriggerFrequency = (GB / once_trig_bytes) / 2;
        }
        else
        {
            TriggerFrequency = GB / once_trig_bytes;
        }
        printf("最大触发频率(单位:Hz): %f\n", TriggerFrequency);
    }
    double triggerduration = 0;
    if (triggermode == 1)
    {
        triggerduration = pulseperiod;
        triggerduration = triggerduration / 1000;//us
        triggerduration = triggerduration / 1000;//ms
    }
    else
    {
        double dataspeed = (repetitionfrequency * once_trig_bytes) / 1024 / 1024;
        if (triggermode == 6 || triggermode == 7)
        {
            dataspeed = dataspeed * 2;
        }
        if (dataspeed > 4000)
        {
            printf("流盘速度大于4GB/S！建议降低触发段时长或者提高触发周期！\n");
            return 0;
        }

        triggerduration = (1 / repetitionfrequency) * 1000;
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
}
int kcDAQ::initializeTheadtoDisk()
{
    //队列初始化操作
    //队列初始化
    int fifo_size = 16;

    char filepath[128] = { "D:\program\Micro-Manager-2.0\data" };

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
    return DEVICE_OK;
}

void* kcDAQ::PollIntr(void* lParam)
{
    ThreadParams* params = static_cast<ThreadParams*>(lParam);
    kcDAQ* instance = params->instance;

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

        // 使用互斥锁保护对实例变量的访问
        pthread_mutex_lock(&instance->mutex_);

        if (intr_cnt % 2 == 0)
        {
            int iBufferIndex = -1;
            int64_t remain_size = instance->data1.DMATotolbytes;
            uint64_t offsetaddr_pong = 0x100000000;

            while (remain_size > 0)
            {
#ifdef WRITEFILE
                //ThreadFileToDisk::Ins().PopFreeFromListPing(iBufferIndex);

                ThreadFileToDisk::Ins().CheckFreeBuffer(iBufferIndex);

                if (iBufferIndex == -1)		//判断buffer是否可用
                {
                    printf("buffer is null! %d\n", ThreadFileToDisk::Ins().GetFreeSizePing());
                    LogMessage("pong buffer is null!");
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
                //sem_post(&c2h_pong);
                intr_pong++;
                //printf("pong is %d free list size %d\n", intr_pong, ThreadFileToDisk::Ins().GetFreeSizePing());
            }
        }
        else
        {
            int iBufferIndex = -1;
            int64_t remain_size = instance->data1.DMATotolbytes;
            uint64_t offsetaddr_ping = 0x0;


            while (remain_size > 0)
            {
#ifdef WRITEFILE
                //ThreadFileToDisk::Ins().PopFreeFromListPing(iBufferIndex);
                ThreadFileToDisk::Ins().CheckFreeBuffer(iBufferIndex);

                if (iBufferIndex == -1)		//判断buffer是否可用
                {
                    printf("buffer is null %d!\n", ThreadFileToDisk::Ins().GetFreeSizePing());
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
                        LogMessage("no data");
                        break;
                    }
                    iLoopCount++;
                } while (iWriteBytes < ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iTotalSize);
                // 数据读取和处理
                if (ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize % sizeof(int16_t) == 0) {
                    size_t numElements = ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize / sizeof(int16_t);
                    if (numElements % 4 != 0) {
                        std::cerr << "Number of elements is not a multiple of 4." << std::endl;
                    }
                    else {
                        size_t numSamples = numElements / 4;
                        std::vector<std::vector<int16_t>> channels(4);

                        int16_t* data = reinterpret_cast<int16_t*>(ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bufferAddr);
                        double scaleFactor = 2 / (2.0 * ((2) ^ 13));

                        for (size_t i = 0; i < numSamples; ++i) {
                            for (size_t j = 0; j < 4; ++j) {
                                channels[j].push_back(data[i * 4 + j] * scaleFactor);
                            }
                        }

                    }
                }
                //sem_post(&c2h_ping);
                intr_ping++;
                //printf("ping is %d free list size %d\n", intr_ping, ThreadFileToDisk::Ins().GetFreeSizePing());
            }
        }
        pthread_mutex_unlock(&instance->mutex_);
        intr_cnt++;
    }


EXIT:
    pthread_exit(NULL);
    return 0;
}
void* kcDAQ::datacollect(void* lParam)
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
                        LogMessage("no data");
                        break;
                    }
                    iLoopCount++;
                } while (iWriteBytes < ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iTotalSize);
                // 数据读取和处理
                if (ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize % sizeof(int16_t) == 0) {
                    size_t numElements = ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_iBufferSize / sizeof(int16_t);
                    if (numElements % 4 != 0) {
                        std::cerr << "Number of elements is not a multiple of 4." << std::endl;
                    }
                    else {
                        size_t numSamples = numElements / 4;
                        std::vector<std::vector<int16_t>> channels(4);

                        int16_t* data = reinterpret_cast<int16_t*>(ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bufferAddr);
                        double scaleFactor = 2 / (2.0 * ((2) ^ 13));

                        for (size_t i = 0; i < numSamples; ++i) {
                            for (size_t j = 0; j < 4; ++j) {
                                channels[j].push_back(data[i * 4 + j] * scaleFactor);
                            }
                        }

                    }
                }
//
//#ifdef WRITEFILE
//
//                //执行写文件的操作
//                ThreadFileToDisk::Ins().m_vectorBuffer[iBufferIndex]->m_bAvailable = true;
//
//                ThreadFileToDisk::Ins().PushAvailToListPing(iBufferIndex);
//#endif

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
                    LogMessage("pong buffer is null!");
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

Log_TraceLog g_Log(std::string("./logs/KunchiUpperMonitor.log"));
Log_TraceLog* pLog = &g_Log;

void printfLog(int nLevel, const char* fmt, ...)
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

///////////////////////////////////////////////////////////////////////////////////
///   ETL 
///

optotune::optotune() :
    initialized_(false)
{
    // Port
    CPropertyAction* pAct = new CPropertyAction(this, &optotune::OnPort);
    CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

optotune::~optotune()
{
    Shutdown();
}

int optotune::Initialize()
{
    initialized_ = true;
    std::string answer = sendStart();
    if (answer != "OK")
    {
        return DEVICE_ERR;
    }
    FPMin = getFPMin();
    FPMax = getFPMax();
    CPropertyAction* pAct = new CPropertyAction(this, &optotune::onSetFP);
    int nRet = CreateProperty("Focal Power(dpt)", "0.00", MM::Float, false, pAct);
    SetPropertyLimits("Focal Power(dpt)", FPMin, FPMax);

    return DEVICE_OK;
}
int optotune::Shutdown()
{
    initialized_ = false;
    return DEVICE_OK;
}

void optotune::GetName(char* name) const
{
    CDeviceUtils::CopyLimitedString(name, g_DeviceNameoptotune);
}


int optotune::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(port.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        if (initialized_)
        {
            // revert
            pProp->Set(port.c_str());
            return DEVICE_ERR;
        }

        pProp->Get(port);
    }

    return DEVICE_OK;
}

int optotune::onSetFP(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        double FP = getFP();
        pProp->Set(FP);
    }
    else if (eAct == MM::AfterSet)
    {
        double FP;
        std::ostringstream command;
        pProp->Get(FP);
        std::string fp = std::to_string(FP);
        command << "SETFP=" << fp;
        int result = SendSerialCommand(port.c_str(), command.str().c_str(), "\r\n");
        std::string answer;
        result = GetSerialAnswer(port.c_str(), "\r\n", answer);
        PurgeComPort(port.c_str());
        if (answer != "OK")
        {
            return DEVICE_ERR;
        }
    }
    return DEVICE_OK;
}

std::string optotune::sendStart()
{
    int result = SendSerialCommand(port.c_str(), "start", "\r\n");

    std::string answer;
    result = GetSerialAnswer(port.c_str(), "\r\n", answer);
    PurgeComPort(port.c_str());
    if (result == DEVICE_OK) {
        if (answer == "OK") {
            // 处理接收到的"ready"信息
            std::cout << "Device is ready." << std::endl;
        }
        else {
            // 接收到的信息不是"ready"
            std::cout << "Received unexpected response: " << answer << std::endl;
        }
    }
    else {
        // 错误处理
        std::cerr << "Failed to receive answer: " << result << std::endl;
    }

    return answer;
}

float optotune::getFPMin()
{
    int result = SendSerialCommand(port.c_str(), "getfpmin", "\r\n");

    std::string answer;
    result = GetSerialAnswer(port.c_str(), "\r\n", answer);
    PurgeComPort(port.c_str());
    if (result == DEVICE_OK) {
        return std::stof(answer);
    }
    else
    {
        return DEVICE_ERR;
    }

}

float optotune::getFPMax()
{
    int result = SendSerialCommand(port.c_str(), "getfpmax", "\r\n");

    std::string answer;
    result = GetSerialAnswer(port.c_str(), "\r\n", answer);
    PurgeComPort(port.c_str());
    if (result == DEVICE_OK) {
        return std::stof(answer);
    }
    else
    {
        return DEVICE_ERR;
    }

}

float optotune::getFP()
{
    int result = SendSerialCommand(port.c_str(), "getfp", "\r\n");

    std::string answer;
    result = GetSerialAnswer(port.c_str(), "\r\n", answer);
    PurgeComPort(port.c_str());
    return std::stof(answer);
}

///////////////////////////////////////////////////////////////////////////////////
///   TPM Camera
///

TPMCamera::TPMCamera():
m_bSequenceRunning(false),
m_bInitialized(false),
m_bBusy(false),
sthd_(0)
{
    InitializeDefaultErrorMessages();
    sthd_ = new SequenceThread(this);
}

TPMCamera::~TPMCamera()
{
    if (m_bInitialized)
    {
        Shutdown();
    }
    m_bInitialized = false;

    delete(sthd_);
}

int TPMCamera::Initialize()
{
    //MM::Device* hubDevice = GetHub();
    m_bInitialized = true;
    return DEVICE_OK;
}

int TPMCamera::Shutdown()
{
    if (m_bInitialized = true)
    {
        m_bInitialized = false;

    }
    return DEVICE_OK;
}

// 控制曝光的启停
int TPMCamera::SnapImage()
{
    // 换算电压 或者 
    return DEVICE_OK;
}

int TPMCamera::



///////////////////////////////////////////////////////////////////////////////////
///   TPM 
///

int TPM::Initialize()
{
    // 输出AOSequence的测试属性
    initialized_ = true;
    CPropertyAction* pAct = new CPropertyAction(this, &TPM::OnTriggerAOSequence);
    CreateProperty("TriggerAOSequence", "Off", MM::String, false, pAct);
    AddAllowedValue("TriggerAOSequence", "Off");
    AddAllowedValue("TriggerAOSequence", "On");
    // DO
    pAct = new CPropertyAction(this, &TPM::OnTriggerDOSequence);
    CreateProperty("TriggerDOSequence", "Off", MM::String, false, pAct);
    AddAllowedValue("TriggerDOSequence", "Off");
    AddAllowedValue("TriggerDOSequence", "On");

    // AO port
    pAct = new CPropertyAction(this, &TPM::OnPortName);
    CreateProperty("PortName", "Dev1/ao0", MM::String, false, pAct);
    AddAllowedValue("PortName", "Dev1/ao0");
    AddAllowedValue("PortName", "Dev1/ao1");
    AddAllowedValue("PortName", "Dev1/ao2");  // 根据实际端口进行添加

    // DO port
    pAct = new CPropertyAction(this, &TPM::OnDOPortName);
    CreateProperty("DOPortName", "Dev1/port0/line0", MM::String, false, pAct);
    AddAllowedValue("DOPortName", "Dev1/port0/line0");
    AddAllowedValue("DOPortName", "Dev1/port0/line1");
    AddAllowedValue("DOPortName", "Dev1/port0/line2");  // 根据实际端口进行添加

    // resonant / galvo
    pAct = new CPropertyAction(this, &TPM::OnScanMode);
    CreateProperty("ScanMode", "Resonant", MM::String, false, pAct);
    AddAllowedValue("ScanMode", "Resonant");
    AddAllowedValue("ScanMode", "Galvo");

    // frequency
    pAct = new CPropertyAction(this, &TPM::OnFrequency);
    CreateFloatProperty("Fps(Hz)", 2, false, pAct);

    // frequency
    pAct = new CPropertyAction(this, &TPM::OnDAQAcquisition);
    CreateProperty("StartAcquision", "Off", MM::String, false, pAct);
    AddAllowedValue("StartAcquision", "Off");
    AddAllowedValue("StartAcquision", "On");
    // ... 其它初始化代码 ...

    return DEVICE_OK;
}

int TPM::DetectInstalledDevices()
{
    ClearInstalledDevices();

    // make sure this method is called before we look for available devices
    InitializeModuleData();

    char hubName[MM::MaxStrLength];
    GetName(hubName); // this device name
    for (unsigned i = 0; i < GetNumberOfDevices(); i++)
    {
        char deviceName[MM::MaxStrLength];
        bool success = GetDeviceName(i, deviceName, MM::MaxStrLength);
        if (success && (strcmp(hubName, deviceName) != 0))
        {
            MM::Device* pDev = CreateDevice(deviceName);
            AddInstalledDevice(pDev);
        }
    }
    return DEVICE_OK;
}

void TPM::GetName(char* pName) const
{
    CDeviceUtils::CopyLimitedString(pName, g_HubDeviceName);
}

NIDAQHub* TPM::GetNIDAQHubSafe() {
    NIDAQHub* hub = static_cast<NIDAQHub*>(GetDevice(g_DeviceNameNIDAQHub));
    if (!hub) {
        std::cerr << "Error: NIDAQHub not found or not properly initialized." << std::endl;
        return nullptr; // 或处理错误的其他方式
    }
    return hub;
}

kcDAQ* TPM::GetkcDAQSafe() {
    kcDAQ* daq = static_cast<kcDAQ*>(GetDevice(g_DeviceNameDAQ));
    if (!daq) {
        std::cerr << "Error: NIDAQHub not found or not properly initialized." << std::endl;
        return nullptr; // 或处理错误的其他方式
    }
    return daq;
}

int TPM::OnPortName(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        std::string portName;
        GetPortName(portName);  // 获取当前设置的端口名
        pProp->Set(portName.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        std::string portName;
        pProp->Get(portName);
        SetPortName(portName);  // 保存新设置的端口名
    }
    return DEVICE_OK;
}

int TPM::OnTriggerAOSequence(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        // 可以提供一些状态信息
    }
    else if (eAct == MM::AfterSet)
    {
        std::string value;
        pProp->Get(value); // 获取当前属性的值

        if (value == "On")
        {
            return TriggerAOSequence(); // 如果是"On"，则执行TriggerAOSequence
        }
        else if (value == "Off")
        {
            return StopAOSequence(); // 如果是"Off"，则执行StopAOSequence或其他你希望的函数
        }
    }
    return DEVICE_OK;
}

int TPM::OnTriggerDOSequence(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        // 可以提供一些状态信息
    }
    else if (eAct == MM::AfterSet)
    {
        std::string value;
        pProp->Get(value); // 获取当前属性的值

        if (value == "On")
        {
            return TriggerDOSequence(); // 如果是"On"，则执行TriggerAOSequence
        }
        else if (value == "Off")
        {
            return StopDOSequence(); // 如果是"Off"，则执行StopAOSequence或其他你希望的函数
        }
    }
    return DEVICE_OK;
}

int TPM::TriggerAOSequence() {
    std::string portName;
    GetPortName(portName);  // 获取当前设置的端口名
    NIDAQHub* nidaqHub = GetNIDAQHubSafe();
    if (nidaqHub) {
        std::vector<double> sequence = { -1.0, -0.96, -0.92, -0.88, -0.84, -0.8, -0.76, -0.72, -0.68, -0.64, -0.6, -0.56, -0.52, -0.48, -0.44, -0.4,
    -0.36, -0.32, -0.28, -0.24, -0.2, -0.16, -0.12, -0.08, -0.04, 0.0, 0.04, 0.08, 0.12, 0.16, 0.2, 0.24, 0.28,
    0.32, 0.36, 0.4, 0.44, 0.48, 0.52, 0.56, 0.6, 0.64, 0.68, 0.72, 0.76, 0.8, 0.84, 0.88, 0.92, 0.96, 1.0,
    0.96, 0.92, 0.88, 0.84, 0.8, 0.76, 0.72, 0.68, 0.64, 0.6, 0.56, 0.52, 0.48, 0.44, 0.4, 0.36, 0.32, 0.28,
    0.24, 0.2, 0.16, 0.12, 0.08, 0.04, 0.0, -0.04, -0.08, -0.12, -0.16, -0.2, -0.24, -0.28, -0.32, -0.36, -0.4,
    -0.44, -0.48, -0.52, -0.56, -0.6, -0.64, -0.68, -0.72, -0.76, -0.8, -0.84, -0.88, -0.92, -0.96 };
        int result = nidaqHub->StartAOSequenceForPort(portName, sequence);
        if (result != DEVICE_OK) {
            // 处理错误
            std::cerr << "Error starting AO sequence on port " << portName << std::endl;
            return result;
        };
        return DEVICE_OK;
    }
}

int TPM::StopAOSequence() {
    std::string portName;
    GetPortName(portName);  // 获取当前设置的端口名
    NIDAQHub* nidaqHub = GetNIDAQHubSafe();
    if (nidaqHub) {
        int result = nidaqHub->StopAOSequenceForPort(portName);
        if (result != DEVICE_OK) {
            std::cerr << "Error starting AO sequence on port " << portName << std::endl;
            return result;
        }
        return DEVICE_OK;
    }
    return DEVICE_ERR;
}

int TPM::OnDOPortName(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        std::string portName;
        GetDOPortName(portName);  // 获取当前设置的端口名
        pProp->Set(portName.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        std::string portName;
        pProp->Get(portName);
        SetDOPortName(portName);  // 保存新设置的端口名
    }
    return DEVICE_OK;
}

int TPM::OnScanMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    pProp->Get(ScanMode);
    return DEVICE_OK;
}

int TPM::OnFrequency(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    pProp->Get(Frequency);
    return DEVICE_OK;
}

int TPM::OnDAQAcquisition(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    std::string start;
    pProp->Get(start);
    if (start == "On")
    {
        StartAcquisition();
    }
    else if (start == "Off")
    {
        StopAcquisition();
    }
    return DEVICE_OK;
}

int TPM::TriggerDOSequence() {
    std::string portName;
    GetDOPortName(portName);  // 获取当前设置的端口名
    NIDAQHub* nidaqHub = GetNIDAQHubSafe();
    if (nidaqHub) {
        int result = nidaqHub->StartDOBlankingAndOrSequenceWithoutTrigger(portName, true, false,
            1, false, 512 * 512);
        if (result != DEVICE_OK) {
            // 处理错误
            std::cerr << "Error starting AO sequence on port " << portName << std::endl;
            return result;
        };
        return DEVICE_OK;
    }
}

int TPM::StopDOSequence() {
    std::string portName;
    GetDOPortName(portName);  // 获取当前设置的端口名
    NIDAQHub* nidaqHub = GetNIDAQHubSafe();
    if (nidaqHub) {
        int result = nidaqHub->StopDOBlankingAndSequence(32);
        if (result != DEVICE_OK) {
            std::cerr << "Error starting AO sequence on port " << portName << std::endl;
            return result;
        }
        return DEVICE_OK;
    }
    return DEVICE_ERR;
}

int TPM::StartAcquisition()
{
    kcDAQ* kcDAQ = GetkcDAQSafe();
    if (kcDAQ)
    {
        int result = kcDAQ->StartDASequence();
        if (result != DEVICE_OK)
        {
            return result;
        }
    }
    return DEVICE_OK;
}

int TPM::StopAcquisition()
{
    kcDAQ* kcDAQ = GetkcDAQSafe();
    if (kcDAQ)
    {
        int result = kcDAQ->StopDASequence();
        if (result != DEVICE_OK)
        {
            return result;
        }
    }
    return DEVICE_OK;
}