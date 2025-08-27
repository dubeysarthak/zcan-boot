#pragma once

#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

typedef enum PacketType {
    PacketTypeHashData = 0,
    PacketTypeFlashData,
    PacketTypeFlashUpdateCmd,
    PacketTypeCheckHashCmd,
    PacketTypeJumpToApp,
} PacketType;

typedef enum FuncStatus {
    FuncStatusOk = 0,
    FuncStatusInProcess,
    FuncStatusCompleted,
    FuncStatusError = -1,
    FuncStatusInvalidData,
    FuncStatusNotReady,
    FuncStatusNotSupported,
    FuncStatusOutOfMemory,
    FuncStatusInvalidArgument,
    FuncStatusInvalidState,
} FuncStatus;

struct frame_packed {
    PacketType state;
    uint8_t data[8];
};
