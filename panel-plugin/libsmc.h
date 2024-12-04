#ifndef __libsmc_h__
#define __libsmc_h__

#include <CoreFoundation/CFBase.h>

/* SMC operations */
typedef CF_ENUM(UInt8, SMCIndex) {
  /* the user client method name constants */
  kSMCUserClientOpen,
  kSMCUserClientClose,
  kSMCHandleYPCEvent,

  kSMCPlaceholder1, /* *** LEGACY SUPPORT placeholder */
  kSMCNumberOfMethods,

  /* other constants not mapped to individual methods */
  kSMCReadKey,
  kSMCWriteKey,
  kSMCGetKeyCount,
  kSMCGetKeyFromIndex,
  kSMCGetKeyInfo,

  kSMCFireInterrupt,
  kSMCGetPLimits,
  kSMCGetVers,
  kSMCPlaceholder2, /* *** LEGACY SUPPORT placeholder */

  kSMCReadStatus,
  kSMCReadResult,

  kSMCVariableCommand
};

typedef UInt32 SMCKey;
typedef UInt32 SMCDataType;
typedef UInt8 SMCDataAttributes;

/* a struct to hold the SMC version */
typedef struct SMCVersion {
  unsigned char major;
  unsigned char minor;
  unsigned char build;
  unsigned char reserved; // padding for alignment
  unsigned short release;
} SMCVersion;

typedef struct SMCPLimitData {
  UInt16 version;
  UInt16 length;
  UInt32 cpuPLimit;
  UInt32 gpuPLimit;
  UInt32 memPLimit;
} SMCPLimitData;

/* a struct to hold the key info data */
typedef struct SMCKeyInfoData {
  UInt32 dataSize;
  SMCDataType dataType;
  SMCDataAttributes dataAttributes;
} SMCKeyInfoData;

/* the struct passed back and forth between the kext and UC */
/* sizeof(SMCParamStruct) should be 168 or 80, depending on whether uses
 * bytes[32] or bytes[120] */
typedef struct SMCParamStruct {
  SMCKey key;
  struct SMCParam {
    SMCVersion vers;
    SMCPLimitData pLimitData;
    SMCKeyInfoData keyInfo;

    UInt8 result;
    UInt8 status;

    UInt8 data8;
    UInt32 data32;
    UInt8 bytes[120];
  } param;
} SMCParamStruct;

int get_fan_status(void);
const char *get_temperature(void);
int get_time_to_empty(void);
int estimate_time_to_full(void);

#endif
