/* Copyright (c) 2024 Torrekie Gen <me@torrekie.dev>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef __libsmc_c__
#define __libsmc_c__
#endif

#include <libxfce4util/libxfce4util.h>

#ifdef __APPLE__

#include "libsmc.h"
#include <CoreFoundation/CFBase.h>
#include <IOKit/IOKitLib.h>

static io_service_t gConn = 0;

static IOReturn smc_open(void) {
  IOReturn result;
  mach_port_t masterPort;
  io_service_t service;

  if (IOMasterPort(MACH_PORT_NULL, &masterPort) != kIOReturnSuccess) {
    DBG("IOMasterPort() failed");
    return 1;
  }

  service =
      IOServiceGetMatchingService(masterPort, IOServiceMatching("AppleSMC"));
  result = IOServiceOpen(service, mach_task_self(), 0, &gConn);
  if (result != kIOReturnSuccess) {
    DBG("IOServiceOpen() failed (%d)", result);
    return result;
  }

  return kIOReturnSuccess;
}

static IOReturn smc_call(int index, SMCParamStruct *inputStruct,
                         SMCParamStruct *outputStruct) {
  size_t inputSize, outputSize;

  inputSize = sizeof(SMCParamStruct);
  outputSize = sizeof(SMCParamStruct);

  return IOConnectCallStructMethod(gConn, index, inputStruct, inputSize,
                                   outputStruct, &outputSize);
}

static IOReturn smc_get_keyinfo(UInt32 key, SMCKeyInfoData *keyInfo) {
  SMCParamStruct inputStruct;
  SMCParamStruct outputStruct;
  IOReturn result = kIOReturnSuccess;

  memset(&inputStruct, 0, sizeof(inputStruct));
  memset(&outputStruct, 0, sizeof(outputStruct));

  inputStruct.key = key;
  inputStruct.param.data8 = kSMCGetKeyInfo;

  result = smc_call(kSMCHandleYPCEvent, &inputStruct, &outputStruct);
  if (result == kIOReturnSuccess) {
    *keyInfo = outputStruct.param.keyInfo;
  }

  return result;
}

static IOReturn smc_read(UInt32 key, void *bytes) {
  IOReturn result;
  SMCParamStruct inputStruct;
  SMCParamStruct outputStruct;
  SMCKeyInfoData keyInfo;

  memset(&inputStruct, 0, sizeof(inputStruct));
  memset(&keyInfo, 0, sizeof(keyInfo));

  inputStruct.key = key;

  result = smc_get_keyinfo(inputStruct.key, &keyInfo);
  if (result != kIOReturnSuccess) {
    return result;
  }

  inputStruct.param.keyInfo.dataSize = keyInfo.dataSize;
  inputStruct.param.data8 = kSMCReadKey;

  memset(&outputStruct, 0, sizeof(outputStruct));
  result = smc_call(kSMCHandleYPCEvent, &inputStruct, &outputStruct);
  if (result != kIOReturnSuccess) {
    DBG("smc_call failed %d", result);
    return result;
  }

  memcpy(bytes, outputStruct.param.bytes, keyInfo.dataSize);

  return kIOReturnSuccess;
}

static UInt32 makeUInt32Key(char *keyString, int size, int base) {
  UInt32 total = 0;
  int i;

  for (i = 0; i < size; i++) {
    if (base == 16)
      total += keyString[i] << (size - 1 - i) * 8;
    else
      total += ((unsigned char)(keyString[i]) << (size - 1 - i) * 8);
  }
  return total;
}

__attribute__((destructor)) void smc_close(void) {
  if (gConn != 0)
    IOServiceClose(gConn);
}

int get_fan_status(void) {
  IOReturn result = kIOReturnSuccess;
  SMCKey key;
  char keyStr[5];
  uint8_t fan_num;
  int i;

  if (gConn == 0)
    result = smc_open();

  if (result != kIOReturnSuccess)
    return 0;

  key = makeUInt32Key("FNum", 4, 16);
  result = smc_read(key, &fan_num);
  /* No hardware fan support, or permission deined */
  if (result != kIOReturnSuccess)
    return 0;

  /* FNum(ui8) = 0, no fans on device */
  if (fan_num == 0)
    return 0;

  /* If have fans, check 'F*Ac', which is current speed */
  for (i = 0; i < fan_num; i++) {
    float retval;

    sprintf(keyStr, "F%dAc", i);
    key = makeUInt32Key(keyStr, 4, 16);
    result = smc_read(key, &retval);
    /* F*Ac(flt), return 1 if any fan working */
    if (retval > 0.0)
      return 1;
  }

  return 0;
}

const char *get_temperature(void) {
  IOReturn result = kIOReturnSuccess;
  float retval;
  static char buf[BUFSIZ];
  SMCKey key;

  if (gConn == 0)
    result = smc_open();

  /* In Linux/BSD impl, they returns " C" nomatter if syscall succ,
   * this is weird ig */
  if (result != kIOReturnSuccess)
    return "-";

  /* TB*T(flt), but normally they are same one */
  key = makeUInt32Key("TB0T", 4, 16);
  result = smc_read(key, &retval);
  if (result != kIOReturnSuccess)
    return "-";

  snprintf(buf, BUFSIZ, "%.1f C", retval);

  return (const char *)buf;
}

int get_time_to_empty(void) {
  IOReturn result = kIOReturnSuccess;
  SMCKey key;
  uint16_t retval;

  if (gConn == 0)
    result = smc_open();

  if (result != kIOReturnSuccess)
    return 0;

#if TARGET_OS_EMBEDDED
  /* This is weird, why B0TF means TimeToEmpty on Embedded,
   * but TimeToFullCharge on macOS? */
  key = makeUInt32Key("B0TF", 4, 16);
#else
  key = makeUInt32Key("B0TE", 4, 16);
#endif

  result = smc_read(key, &retval);
  if (result != kIOReturnSuccess)
    return 0;

  /* 0xFFFF, battery charging */
  if (retval == 65535)
    return 0;

  return retval;
}

int estimate_time_to_full() {
  IOReturn result = kIOReturnSuccess;
  SMCKey key;
  int16_t current;
  uint16_t fullcap;

  if (gConn == 0)
    result = smc_open();

  if (result != kIOReturnSuccess)
    return 0;

  /* B0FC(ui16) FullChargeCapacity (mAh) */
  key = makeUInt32Key("B0FC", 4, 16);
  result = smc_read(key, &fullcap);
  if (result != kIOReturnSuccess)
    return 0;

  /* B0AC(si16) AverageCurrent (mA) */
  key = makeUInt32Key("B0AC", 4, 16);
  result = smc_read(key, &current);
  if (result != kIOReturnSuccess)
    return 0;

  /* Not charging */
  if (current < 0)
    return 0;

  /* TimeToFullCharge = FullChargeCapacity (mAh) / AverageCurrent (mA) */
  return (fullcap / current);
}

#endif
