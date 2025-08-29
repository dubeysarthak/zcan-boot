/**
 * @file flash_config.h
 * @author Sarthak Et (et-sarthak)
 * @brief Flash configuration and utility definitions
 * @version 1.0
 * @date 2024-10-05
 * 
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

/**
 * @brief Packet types for CAN communication
 */
typedef enum PacketType {
    PacketTypeHashData = 0,
    PacketTypeFlashData,
    PacketTypeFlashUpdateCmd,
    PacketTypeCheckHashCmd,
    PacketTypeJumpToApp,
} PacketType;

/**
 * @brief Status codes for function operations
 */
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
