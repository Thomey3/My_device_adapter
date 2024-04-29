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
        return new DAQAnalogInputPort();
    }
    else if (strcmp(deviceName, g_DeviceNameoptotune) == 0)
    {
        return new optotune();
    }

    // ...supplied name not recognized
    return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
    delete pDevice;
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

#define MB *1024*1024
#define INTERRUPT 0
#define POLLING 1
#define WRITEFILE 1
#define BASE_8MB 8*1024*1024
#define single_interruption_duration 1000

STXDMA_CARDINFO pstCardInfo;//存放PCIE板卡信息结构体
Log_TraceLog g_Log(std::string("./logs/KunchiUpperMonitor.log"));
Log_TraceLog* pLog = &g_Log;
static sem_t gvar_program_exit;	//声明一个信号量
sem_t c2h_ping;		//ping信号量
sem_t c2h_pong;		//pong信号量
long once_readbytes = 8 MB;
long data_size = 0;
uint32_t flag_ping = 0;
uint32_t flag_pong = 0;

//函数声明
void* PollIntr(void* lParam);
void* datacollect(void* lParam);

struct data
{
    uint64_t DMATotolbytes;
    uint64_t allbytes;
};
struct data data1;

typedef struct {
    unsigned char* bufferAddr;      // 缓冲区地址
    size_t totalSize;      // 缓冲区总大小
    size_t currentSize;    // 当前已使用的大小
} Buffer;
Buffer buffer;


DAQAnalogInputPort::DAQAnalogInputPort() :
    initialized_(false)
{
    QTXdmaOpenBoard(&pstCardInfo, 0);   //打开板卡
    QT_BoardGetCardInfo();              //获取板卡信息
    QT_BoardSetADCStop();               //停止采集  
    QT_BoardSetTransmitMode(0, 0);      //DMA置0
    QT_BoardSetInterruptClear();        //清除中断
    QT_BoardSetSoftReset();             //执行上位机软件复位
}

DAQAnalogInputPort::~DAQAnalogInputPort()
{
    Shutdown();
}

int DAQAnalogInputPort::Initialize()
{
    //信号量初始化
    sem_init(&gvar_program_exit, 0, 0);
    sem_init(&c2h_ping, 0, 0);
    sem_init(&c2h_pong, 0, 0);

    buffer.bufferAddr = (unsigned char*)malloc(1024 * 1024);  // 分配 1MB 大小的缓冲区
    buffer.totalSize = 1024 * 1024;
    buffer.currentSize = 0;

    // 调整偏置
    CPropertyAction* pAct = new CPropertyAction(this, &DAQAnalogInputPort::change_bias_channal);
    int err = CreateFloatProperty("Channel1 offset", offset, false, pAct);
    if (err != DEVICE_OK)
        return err;
    err = CreateFloatProperty("Channel2 offset", offset, false, pAct);
    if (err != DEVICE_OK)
        return err;
    err = CreateFloatProperty("Channel3 offset", offset, false, pAct);
    if (err != DEVICE_OK)
        return err;
    err = CreateFloatProperty("Channel4 offset", offset, false, pAct);
    if (err != DEVICE_OK)
        return err;

    // 预触发设置
    pAct = new CPropertyAction(this, &DAQAnalogInputPort::set_pre_trig_length);
    err = CreateIntegerProperty("Pre Trig Length", length, false, pAct);
    if (err != DEVICE_OK)
        return err;

    // 帧头设置
    pAct = new CPropertyAction(this, &DAQAnalogInputPort::set_Frameheader);
    err = CreateIntegerProperty("Frameheader", Frameheader, false, pAct);
    if (err != DEVICE_OK)
        return err;

    // 设置时钟模式
    pAct = new CPropertyAction(this, &DAQAnalogInputPort::set_clockmode);
    err = CreateStringProperty("Clockmode", "", false, pAct);
    if (err != DEVICE_OK)
        return err;
    AddAllowedValue("Clockmode", "内参考时钟");
    AddAllowedValue("Clockmode", "外采样时钟");
    AddAllowedValue("Clockmode", "外参考时钟");

    // 外部触发的一些变量设置
    // 只添加你需要创建属性的变量
    triggerSetupMap["Trigger Count"] = &triggercount;
    triggerSetupMap["Pulse Period"] = &pulse_period;
    triggerSetupMap["Pulse Width"] = &pulse_width;
    triggerSetupMap["Trigger lag"] = &arm_hysteresis;
    triggerSetupMap["Rasing Codevalue"] = &rasing_codevalue;
    triggerSetupMap["Falling Codevalue"] = &falling_codevalue;

    // 为每个属性创建Micro-Manager属性
    for (auto& it : triggerSetupMap) {
        // Cast uint32_t to long for conversion to string
        long valueAsLong = static_cast<long>(*(it.second));
        CreateProperty(it.first.c_str(), CDeviceUtils::ConvertToString(valueAsLong), MM::Integer, false, new CPropertyAction(this, &DAQAnalogInputPort::OnUInt32Changed));
    }

    // 设置触发通道
    pAct = new CPropertyAction(this, &DAQAnalogInputPort::set_triggerchannel);
    err = CreateStringProperty("triggerchannel", "", false, pAct);
    if (err != DEVICE_OK)
        return err;
    AddAllowedValue("triggerchannel", "1");
    AddAllowedValue("triggerchannel", "2");
    AddAllowedValue("triggerchannel", "3");
    AddAllowedValue("triggerchannel", "4");

    // 设置触发模式
    pAct = new CPropertyAction(this, &DAQAnalogInputPort::set_triggermode);
    err = CreateStringProperty("triggermode", "", false, pAct);
    if (err != DEVICE_OK)
        return err;
    AddAllowedValue("triggermode", "0 软件触发");
    AddAllowedValue("triggermode", "1 内部脉冲触发");
    AddAllowedValue("triggermode", "2 外部脉冲上升沿触发");
    AddAllowedValue("triggermode", "3 外部脉冲下降沿触发");
    AddAllowedValue("triggermode", "4 通道上升沿触发");
    AddAllowedValue("triggermode", "5 通道下降沿触发");
    AddAllowedValue("triggermode", "6 通道双沿触发");
    AddAllowedValue("triggermode", "7 外部脉冲双沿触发");

    ///////DMA 设置
    pAct = new CPropertyAction(this, &DAQAnalogInputPort::OnSegmentDurationChanged);
    err = CreateFloatProperty("Segment Duration (us)", SegmentDuration, false, pAct);
    SetPropertyLimits("Segment Duration (us)", 0, 536870.904);
    if (err != DEVICE_OK)
        return err;

    pAct = new CPropertyAction(this, &DAQAnalogInputPort::OnTriggerFrequencyChanged);
    err = CreateFloatProperty("Trigger Frequency (Hz)", TriggerFrequency, false, pAct);
    SetPropertyLimits("Trigger Frequency (Hz)", 800, 1e6); // 根据设定调整最大最小值
    if (err != DEVICE_OK)
        return err;

    // 开始采集
    pAct = new CPropertyAction(this, &DAQAnalogInputPort::onCollection);
    err = CreateStringProperty("on Collection", "off", false, pAct);
    if (err != DEVICE_OK)
        return err;
    AddAllowedValue("on Collection", "off");
    AddAllowedValue("on Collection", "on");

}

int DAQAnalogInputPort::Shutdown()
{
    if (!initialized_)
        return DEVICE_OK;

    int err = StopTask();

    initialized_ = false;
    return err;
}

int DAQAnalogInputPort::StopTask()
{
    QT_BoardSetADCStop();
    QT_BoardSetTransmitMode(0, 0);
    int err = QTXdmaCloseBoard(&pstCardInfo);
    if (err != DEVICE_OK)
    {
        return err;
    }
    else
    {
        return DEVICE_OK;
    }

}

void DAQAnalogInputPort::GetName(char* name) const
{
    CDeviceUtils::CopyLimitedString(name, g_DeviceNameDAQ);
}

int DAQAnalogInputPort::change_bias_channal(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    std::string propName = pProp->GetName();  // 获取触发回调的属性名称
    if (eAct == MM::AfterSet)
    {
        double offset;
        pProp->Get(offset); // 从属性中获取用户设置的新偏移值

        int channelID = 0;  // 初始化通道ID
        if (propName == "Channel1_offset")
        {
            channelID = 1;
        }
        else if (propName == "Channel2_offset")
        {
            channelID = 2;
        }
        else if (propName == "Channel3_offset")
        {
            channelID = 3;
        }
        else if (propName == "Channel4_offset")
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

int DAQAnalogInputPort::set_pre_trig_length(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        double length;
        pProp->Get(length); // 从属性中获取用户设置的新偏移值
        if (length > -1) {  // 检查channelID是否有效
            int err = QT_BoardSetPerTrigger(length);
            if (err != DEVICE_OK)
                return err;
        }
    }
    return DEVICE_OK;
}

int DAQAnalogInputPort::set_Frameheader(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        double Frameheader;
        pProp->Get(Frameheader); // 从属性中获取用户设置的新偏移值
        if (Frameheader > -1) {  // 检查channelID是否有效
            int err = QT_BoardSetFrameheader(Frameheader);
            if (err != DEVICE_OK)
                return err;
        }
    }
    return DEVICE_OK;
}

int DAQAnalogInputPort::set_clockmode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        std::string clockmode;
        int mode = 3;
        pProp->Get(clockmode); // 从属性中获取用户设置的新偏移值
        if (clockmode == "内参考时钟")
        {
            mode = 0;
        }
        else if (clockmode == "外采样时钟")
        {
            mode = 1;
        }
        else if (clockmode == "外参考时钟")
        {
            mode = 2;
        }
        if (mode < 3)
        {  // 检查channelID是否有效
            int err = QT_BoardSetClockMode(mode);
            if (err != DEVICE_OK)
                return err;
        }
    }
    return DEVICE_OK;
}

int DAQAnalogInputPort::OnUInt32Changed(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    std::string propName = pProp->GetName();
    auto it = triggerSetupMap.find(propName);

    if (it != triggerSetupMap.end()) {
        if (eAct == MM::BeforeGet) {
            // Convert uint32_t to string by casting to long first
            long valueAsLong = static_cast<long>(*(it->second));
            pProp->Set(CDeviceUtils::ConvertToString(valueAsLong));
        }
        else if (eAct == MM::AfterSet) {
            long temp;
            pProp->Get(temp);
            *(it->second) = static_cast<uint32_t>(temp);  // Cast back to uint32_t after getting as long
        }
    }
    return DEVICE_OK;
}

int DAQAnalogInputPort::set_triggerchannel(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet) {
        // 当要获取属性值时，将trigchannelID转换为字符串并返回
        pProp->Set(CDeviceUtils::ConvertToString(static_cast<long>(trigchannelID)));
    }
    else if (eAct == MM::AfterSet) {
        std::string valueAsString;
        pProp->Get(valueAsString); // 获取属性值（字符串形式）

        // 将字符串转换为uint32_t类型
        try {
            uint32_t value = static_cast<uint32_t>(std::stoul(valueAsString));

            // 检查是否在允许的值范围内
            if (value >= 1 && value <= 4) {
                trigchannelID = value; // 更新trigchannelID
            }
            else {
                // 如果不在允许范围内，你可以抛出错误或者将其设置为默认值
                return DEVICE_INVALID_PROPERTY_VALUE;
            }
        }
        catch (const std::exception&) {
            // 如果转换失败，返回错误
            return DEVICE_INVALID_PROPERTY_VALUE;
        }
    }
    return DEVICE_OK;
}

int DAQAnalogInputPort::set_triggermode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet) {
        // 获取当前设置的触发模式并将其转换为字符串形式返回
        pProp->Set(CDeviceUtils::ConvertToString(static_cast<long>(trigmode)));
    }
    else if (eAct == MM::AfterSet) {
        std::string modeAsString;
        pProp->Get(modeAsString); // 获取属性值（字符串形式）
        uint32_t mode = static_cast<uint32_t>(std::stoul(modeAsString.substr(0, 1))); // 解析模式编号

        // 设置trigmode变量，并根据选定的模式调用相应的硬件接口函数
        trigmode = mode;
        switch (trigmode) {
        case 0:
            QT_BoardSoftTrigger();
            break;
        case 1:
            QT_BoardInternalPulseTrigger(triggercount, pulse_period, pulse_width);
            break;
        case 2:
            QT_BoardExternalTrigger(2, triggercount);
            break;
        case 3:
            QT_BoardExternalTrigger(3, triggercount);
            break;
        case 4:
            QT_BoardChannelTrigger(4, triggercount, trigchannelID, rasing_codevalue, falling_codevalue);
            break;
        case 5:
            QT_BoardChannelTrigger(5, triggercount, trigchannelID, rasing_codevalue, falling_codevalue);
            break;
        case 6:
            QT_BoardChannelTrigger(6, triggercount, trigchannelID, rasing_codevalue, falling_codevalue);
            break;
        case 7:
            QT_BoardExternalTrigger(7, triggercount);
            break;
        default:
            // 如果触发模式无效，记录错误或设置为默认值
            return DEVICE_INVALID_PROPERTY_VALUE;
        }
    }
    return DEVICE_OK;
}

int DAQAnalogInputPort::OnSegmentDurationChanged(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(SegmentDuration);
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(SegmentDuration);
        int err = CalculateOnceTrigBytes();
        if (err != DEVICE_OK)
        {
            return DEVICE_ERR;
        }
        CalculateTriggerFrequency();
        CheckTriggerDuration();
    }
    return DEVICE_OK;
}

int DAQAnalogInputPort::OnTriggerFrequencyChanged(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(TriggerFrequency);
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(TriggerFrequency);
        CalculateTriggerFrequency();
        CheckTriggerDuration();
        CheckDataSpeed(); // 检查数据速度是否超标
        set_data1();
    }
    return DEVICE_OK;
}

int DAQAnalogInputPort::onCollection(MM::PropertyBase* pProp, MM::ActionType eAct)
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
            //DMA参数设置
            QT_BoardSetFifoMultiDMAParameter(OnceTrigBytes, data1.DMATotolbytes);
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
        }
        else if (value == "Off")
        {
            QT_BoardSetADCStop();
            QT_BoardSetTransmitMode(0, 0);
            QTXdmaCloseBoard(&pstCardInfo);
        }
        sem_wait(&gvar_program_exit);
        sem_destroy(&gvar_program_exit);
    }
    return DEVICE_OK;

}

int DAQAnalogInputPort::CalculateOnceTrigBytes()
{
    OnceTrigBytes = static_cast<uint64_t>(SegmentDuration * Samplerate * ChannelCount * 2);
    if (OnceTrigBytes % 512 != 0)
    {
        OnceTrigBytes = (OnceTrigBytes / 512) * 512;
    }
    if (OnceTrigBytes > 4294967232)
    {
        LogMessage("单次触发数据量超过DDR大小！请减小触发段时长或者降低触发频率！");
        return DEVICE_ERR;
    }
    return DEVICE_OK;
}

void DAQAnalogInputPort::CalculateTriggerFrequency()
{
    if (trigmode != 0 && trigmode != 1)
    {
        uint64_t GB = 4000000000;
        if (trigmode == 6 || trigmode == 7)
        {
            TriggerFrequency = (GB / OnceTrigBytes) / 2;
        }
        else
        {
            TriggerFrequency = GB / OnceTrigBytes;
        }
    }
}

void DAQAnalogInputPort::CheckTriggerDuration()
{
    if (trigmode == 1)
    {
        TriggerDuration = pulse_period / 1000.0 / 1000.0;
    }
    else
    {
        TriggerDuration = 1000.0 / TriggerFrequency;
    }
}

int DAQAnalogInputPort::CheckDataSpeed()
{
    double dataspeed = (TriggerFrequency * OnceTrigBytes) / 1024.0 / 1024.0; // 数据速度计算，单位为MB/s
    if (trigmode == 6 || trigmode == 7) // 如果是双边沿触发
    {
        dataspeed *= 2; // 数据速度翻倍
    }
    if (dataspeed > 4000) // 超过4GB/s
    {
        LogMessage("流盘速度大于4GB/S！建议降低触发段时长或者提高触发周期！", true); // 记录错误消息
        // 这里可以进行一些恢复或安全设置的操作
        // 可以选择停止操作，设置触发频率到一个安全值，或者提示用户进行调整
        TriggerFrequency = 4000 / (OnceTrigBytes / 1024.0 / 1024.0); // 调整到最大安全频率
        return DEVICE_ERR;
    }
    return DEVICE_OK;
}

int DAQAnalogInputPort::set_data1()
{
    if (TriggerDuration > single_interruption_duration) {
        data1.DMATotolbytes = OnceTrigBytes;
    }
    else {
        double x = single_interruption_duration / TriggerDuration;
        if (x - static_cast<int>(x) > 0.5) x += 1;
        int frameCount = static_cast<int>(x); // 帧头个数
        LogMessage("帧头个数 = " + std::to_string(frameCount));
        data1.DMATotolbytes = OnceTrigBytes * frameCount;
    }

    // 保证数据量为512字节的倍数
    if (data1.DMATotolbytes % 512 != 0) {
        data1.DMATotolbytes = (data1.DMATotolbytes / 512) * 512;
    }

    // 检查数据量是否超过DDR大小
    if (data1.DMATotolbytes > 4294967232) {
        LogMessage("单次中断数据量超过DDR大小！请减小触发段时长或者降低触发频率！", true);
        return DEVICE_ERR;
    }

    LogMessage("单次中断数据量(单位:字节): " + std::to_string(data1.DMATotolbytes));
    data1.allbytes = data1.DMATotolbytes;

    return DEVICE_OK;
}

int DAQAnalogInputPort::Start_Collection()
{
    //DMA参数设置
    int err = QT_BoardSetFifoMultiDMAParameter(OnceTrigBytes, data1.DMATotolbytes);
    if (err != DEVICE_OK)
    {
        LogMessage("dma setup failed!");
        return err;
    }
    //DMA传输模式设置
    QT_BoardSetTransmitMode(1, 0);
    if (err != DEVICE_OK)
    {
        LogMessage("dma setup failed!");
        return err;
    }
    //使能PCIE中断
    QT_BoardSetInterruptSwitch();
    if (err != DEVICE_OK)
    {
        LogMessage("PCIE setup failed!");
        return err;
    }
    //数据采集线程
    pthread_t data_collect;
    pthread_create(&data_collect, NULL, datacollect, &data1);
    if (err != DEVICE_OK)
    {
        LogMessage("Thread_data_collect setup failed!");
        return err;
    }
    //ADC开始采集
    QT_BoardSetADCStart();
    if (err != DEVICE_OK)
    {
        LogMessage("ADC setup failed!");
        return err;
    }
    //等中断线程
    pthread_t wait_intr_c2h_0;
    pthread_create(&wait_intr_c2h_0, NULL, PollIntr, NULL);
    if (err != DEVICE_OK)
    {
        LogMessage("Thread_data_waitintr setup failed!");
        return err;
    }
}

void* PollIntr(void* lParam)
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

void* datacollect(void* lParam)
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
                            buffer.bufferAddr + buffer.currentSize, once_readbytes, 0);

                        buffer.currentSize += once_readbytes;

                        iWriteBytes += once_readbytes;
                    }
                    else if (remain_size > 0)
                    {
                        QTXdmaGetDataBuffer(offsetaddr_ping, &pstCardInfo,
                            buffer.bufferAddr + buffer.currentSize, remain_size, 0);

                        buffer.currentSize += once_readbytes;
                        iWriteBytes += remain_size;
                    }

                    offsetaddr_ping += once_readbytes;
                    remain_size -= once_readbytes;

                    if (remain_size <= 0)
                    {

                        break;
                    }
                    iLoopCount++;
                } while (iWriteBytes < buffer.totalSize);

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

                int iLoopCount = 0;
                int iWriteBytes = 0;

                do
                {
                    if (remain_size >= once_readbytes)
                    {
                        QTXdmaGetDataBuffer(offsetaddr_pong, &pstCardInfo,
                            buffer.bufferAddr + buffer.currentSize, once_readbytes, 0);

                        buffer.currentSize += once_readbytes;

                        iWriteBytes += once_readbytes;
                    }
                    else if (remain_size > 0)
                    {
                        QTXdmaGetDataBuffer(offsetaddr_pong, &pstCardInfo,
                            buffer.bufferAddr + buffer.currentSize, remain_size, 0);

                        buffer.currentSize += once_readbytes;

                        iWriteBytes += remain_size;
                    }

                    iLoopCount++;

                    offsetaddr_pong += once_readbytes;
                    remain_size -= once_readbytes;

                    if (remain_size <= 0)
                    {
                        break;
                    }

                } while (iWriteBytes < buffer.totalSize);
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

    // 添加 portName 属性
    CPropertyAction* pActPort = new CPropertyAction(this, &TPM::OnPortName);
    CreateProperty("PortName", "Dev2/ao0", MM::String, false, pActPort);
    AddAllowedValue("PortName", "Dev2/ao0");
    AddAllowedValue("PortName", "Dev2/ao1");
    AddAllowedValue("PortName", "Dev2/ao2");  // 根据实际端口进行添加

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