// micro-manager
#include "DeviceBase.h"
#include "ImgBuffer.h"
#include "DeviceThreads.h"
#include <string>
#include <map>
#include <algorithm>
#include <stdint.h>
#include <future>

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

const char* NoHubError = "Parent Hub not defined.";

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

private:
    void GetPeripheralInventory();

    std::vector<std::string> peripherals_;
    bool initialized_;
    bool busy_;
};

class NIDAQ : public 