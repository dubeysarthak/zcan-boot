/**
 * @file main.c
 * @author Sarthak Et (et-sarthak)
 * @brief Custom bootloader implementation for firmware updates over CAN bus
 * @version 1.0
 * @date 2024-10-05
 * 
 */

#include <string.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "../../include/can.h"
#include "../../include/flash_config.h"
#include "../../include/sha256.h"
#include "../../include/utils.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_MAX_LEVEL);

/*-------------------------MACROS-----------------------------------*/

#define FL_HEADER_LENGTH     0x400U
#define FL_APP_SLOT_ONE_ADDR 0x20000U
#define FL_APP_SLOT_TWO_ADDR 0x7c000U
#define FL_APP_SLOT_SIZE     0x20000U
#define FL_STORAGE_AREA      0x3c000U
#define FL_UNIT_ADDR         16U
#define FL_SECTOR_SIZE       8192U
#define FL_BASE_ADDRESS      0x08000000U
#define FLASH_DEVICE_NODE    DT_CHOSEN(zephyr_flash_controller)

const struct device* flash_dev = DEVICE_DT_GET(FLASH_DEVICE_NODE);
struct tc_sha256_state_struct sha_ctx;
struct tc_sha256_state_struct new_firm;

typedef struct fl_data {
    uint8_t hashA[2][32];
    uint8_t versionA[2][3];
    int size_in_sectors[2];
    bool is_update_req;
}fl_data_t;

typedef enum DfuState {
    DfuStateWaitForHashData = 0,
    DfuStateWaitForFlashData,
    DfuStateWaitForFlashUpdateCmd,
    DfuStateWaitForCheckHashCmd,
    DfuStatesJumpToApp,
    DfuStateWaiting,
} DfuState_e;

fl_data_t fl_data = {
    .hashA = {{0}},
    .versionA = {{0}},
    .size_in_sectors = {0},
};
int cnt = 0;
int slot_index;
int size_in_sectors;
uint8_t hash_value[32] = {0};
uint8_t output_hash[32] = {0};
uint32_t sector_data[FL_SECTOR_SIZE / 4] = {0};
uint32_t app_slot_addr[2] = {FL_APP_SLOT_ONE_ADDR, FL_APP_SLOT_TWO_ADDR};

/*-------------------------------LOCAL API----------------------------------*/

/**
 * @brief           Read data from flash memory
 * @param addr      flash address to read from
 * @param data      pointer to data buffer to store read data
 * @param size      size of data to read in bytes
 * @return          report error in case of failure
 */

 FuncStatus flash_read_data(uint32_t addr, uint32_t* data, uint32_t size) {
    if (addr % FL_UNIT_ADDR != 0) {
        LOG_ERR("Address or size not aligned to FLASH_UNIT_ADDR (%d)\n", FL_UNIT_ADDR);
        return FuncStatusInvalidArgument;
    }
    for (uint32_t i = 0; i < size; i += FL_UNIT_ADDR) {
        int rc = flash_read(flash_dev, addr + i, (uint8_t*)data + i, FL_UNIT_ADDR);
        if (rc != 0) {
            LOG_ERR("Flash read failed (rc=%d)\n", rc);
            return FuncStatusError;
        }
    }
    return FuncStatusOk;
}

/**
 * @brief           Write data to flash memory (sector erase before write)
 * @param addr      flash address to read from
 * @param data      pointer to data buffer to store read data
 * @param size      size of data to read in bytes
 * @return          report error in case of failure
 */
FuncStatus flash_write_sector(uint32_t addr, const uint32_t* data, uint32_t size) {
    if (addr % FL_UNIT_ADDR != 0) {
        LOG_ERR("Address or size not aligned to FLASH_UNIT_ADDR (%d)\n", FL_UNIT_ADDR);
        return FuncStatusInvalidArgument;
    }
    int rc = flash_erase(flash_dev, addr, FL_SECTOR_SIZE);
    if (rc != 0) {
        LOG_ERR("Flash erase failed (rc=%d)\n", rc);
        return FuncStatusError;
    }
    for (uint32_t i = 0; i < size; i += FL_UNIT_ADDR) {
        int rc = flash_write(flash_dev, addr + i, (const uint8_t*)data + i, FL_UNIT_ADDR);
        if (rc != 0) {
            LOG_ERR("Flash write failed at offset %u (rc=%d)\n", i, rc);
            return FuncStatusError;
        }
    }
    return FuncStatusOk;
}


/**
 * @brief            Store incoming hash data
 * @param data       Pointer to incoming data
 * @return           FuncStatus indicating the status of the operation
 */

FuncStatus store_hash(uint8_t* data) {
    if (cnt == 32) {
        slot_index = data[0];
        fl_data.size_in_sectors[slot_index] = data[1];
        memcpy(&fl_data.versionA[slot_index], &data[2], sizeof(fl_data.versionA[0]));
        memcpy(&fl_data.hashA[slot_index][0], &hash_value, sizeof(fl_data.hashA[0]));
        LOG_INF("version %x %x %x", fl_data.versionA[slot_index][0],
                fl_data.versionA[slot_index][1], fl_data.versionA[slot_index][2]);
        LOG_INF("current slot %d", slot_index);
        cnt = 0;
        return FuncStatusCompleted;
    } else {
        memcpy(&hash_value[cnt], data, sizeof(uint64_t));
        cnt = cnt + 8;
        return FuncStatusInProcess;
    }
}
/**
 * @brief             Store incoming flash data into a local buffer
 * @param data        Pointer to incoming data
 * @return            FuncStatus indicating the status of the operation
 */
FuncStatus store_flash(uint8_t* data) {
    if (cnt < (FL_SECTOR_SIZE / 4)) {
        memcpy(&sector_data[cnt], data, sizeof(uint64_t));
        cnt = cnt + 2;
        if (cnt == (FL_SECTOR_SIZE / 4)) {
            return FuncStatusCompleted;
        }
        return FuncStatusInProcess;
    } else if (cnt >= (FL_SECTOR_SIZE / 4)) {
        LOG_ERR("local buffer is full");
        return FuncStatusOutOfMemory;
    }
    return FuncStatusError;
}

/**
 * @brief               Update flash memory with data from the local buffer
 * @param slot_index    Index of the application slot to update
 * @return              FuncStatus indicating the status of the operation
 */

FuncStatus update_flash(uint8_t slot_index) {
    cnt = 0;
    static uint32_t sector_number = 0;

    LOG_INF("writing to flash_sector %d", sector_number);
    tc_sha256_update(&new_firm, (uint8_t*)sector_data, sizeof(sector_data));
    if (flash_write_sector(app_slot_addr[slot_index] + (FL_SECTOR_SIZE * sector_number),
                           sector_data, FL_SECTOR_SIZE) == FuncStatusOk) {
        sector_number++;
        memset(sector_data, 0xFFFFFFFF, sizeof(sector_data));
        if (fl_data.size_in_sectors[slot_index] == sector_number) {
            // All sectors written
            return FuncStatusCompleted;
        }
        return FuncStatusInProcess;
    }
    return FuncStatusError;
}

/**
 * @brief               Check if the computed hash matches the expected hash
 * @return              FuncStatus indicating whether the hashes match
 */
FuncStatus check_hash() {
    tc_sha256_final(output_hash, &new_firm);
    for (int i = 0; i < sizeof(output_hash); i++) {
        k_msleep(10);
        LOG_INF("%02x\t%02x ", output_hash[i], hash_value[i]);
    }
    if (memcmp(output_hash, hash_value, sizeof(output_hash)) == 0) return FuncStatusOk;
    return FuncStatusError;
}

/**
 * @brief               Jump to the application code in the specified slot
 * @param slot_index    Index of the application slot to jump to
 */
void jump_to_app(uint8_t slot_index) {
    LOG_INF("app_slot %x", app_slot_addr[slot_index]);
    void (*app_reset_handler)();
    uint32_t msp =
        *(volatile uint32_t*)(FL_BASE_ADDRESS + app_slot_addr[slot_index] + FL_HEADER_LENGTH);
    uint32_t reset_handler =
        *(volatile uint32_t*)(FL_BASE_ADDRESS + app_slot_addr[slot_index] + FL_HEADER_LENGTH + 4) |
        0x1;

    LOG_INF("msp: %x\n", msp);
    LOG_INF("reset_handler: %x\n", reset_handler);

    // Set the Vector Table Offset Register to the application's vector table
    SCB->VTOR = FL_BASE_ADDRESS + app_slot_addr[slot_index] + FL_HEADER_LENGTH;

    app_reset_handler = (void (*)())reset_handler;
    __disable_irq();
    HAL_DeInit();
    __set_MSP(msp);
    __DSB();
    __ISB();

    app_reset_handler();
}

/**
 * @brief               Find the latest valid firmware version
 * @param version       Pointer to the fl_data structure containing version information
 * @return              Index of the latest valid version (0 or 1), or -1 if both are invalid
 */
static inline int find_latest_version(struct fl_data* version) {
    int versionA_val =
        (version->versionA[0][0] << 16) | (version->versionA[0][1] << 8) | version->versionA[0][2];
    int versionB_val =
        (version->versionA[1][0] << 16) | (version->versionA[1][1] << 8) | version->versionA[1][2];
    int isA_valid = (versionA_val != 0xFFFFFF);
    int isB_valid = (versionB_val != 0xFFFFFF);

    if (isA_valid && !isB_valid) return 0;      // Only A is valid
    if (!isA_valid && isB_valid) return 1;      // Only B is valid
    if (!isA_valid && !isB_valid) return (-1);  // Both invalid: default to A`
    return (memcmp(&versionA_val, &versionB_val, sizeof(int)) > 0) ? 0 : 1;
}

/*-----------------------------DFU STATE_MACHINE--------------------------------*/

/**
 * @brief               Poll for incoming data and manage the DFU state machine
 * @return              FuncStatus indicating the status of the operation
 */
FuncStatus poll_data() {
    FuncStatus fs = FuncStatusNotReady;
    static DfuState_e state = DfuStateWaitForHashData;
    struct frame_packed packed_frame = {0};
    fs = k_msgq_get(&can_receive_q, &packed_frame, K_FOREVER);
    if (fs != FuncStatusOk) {
        LOG_ERR("k_msgq_get failed with error: %d", fs);
        return fs;
    }

    switch (state) {
    case DfuStateWaiting:
        state = packed_frame.state;
        break;

    case DfuStateWaitForHashData:
        if (packed_frame.state != PacketTypeHashData) {
            LOG_WRN("%s: %d: Invalid packet type: %d", __FUNCTION__, __LINE__, packed_frame.state);
            return FuncStatusInvalidState;
        }
        fs = store_hash(packed_frame.data);
        if (fs == FuncStatusCompleted) {
            state = DfuStateWaitForFlashData;
        }
        break;

    case DfuStateWaitForFlashData:
        if (packed_frame.state != PacketTypeFlashData) {
            LOG_WRN("%s: %d: Invalid packet type: %d", __FUNCTION__, __LINE__, packed_frame.state);
            return FuncStatusInvalidState;
        }
        fs = store_flash(packed_frame.data);
        if (fs == FuncStatusCompleted) {
            state = DfuStateWaitForFlashUpdateCmd;
        } else if (fs == FuncStatusInProcess) {
            return fs;
        } else {
            LOG_ERR("Error storing flash data: %d", fs);
            return fs;
        }
        break;

    case DfuStateWaitForFlashUpdateCmd:
        fs = update_flash(slot_index);
        if (fs == FuncStatusCompleted) {
            LOG_INF("All Flash sectors update successful for slot %d", slot_index);
            state = DfuStateWaitForCheckHashCmd;
        } else if (fs == FuncStatusInProcess) {
            state = DfuStateWaitForFlashData;
        } else {
            LOG_ERR("Error updating flash: %d", fs);
            return fs;
        }
        break;

    case DfuStateWaitForCheckHashCmd:
        if (packed_frame.state != PacketTypeCheckHashCmd) {
            LOG_WRN("%s: %d: Invalid packet type: %d", __FUNCTION__, __LINE__, packed_frame.state);
            return FuncStatusInvalidState;
        }
        fs = check_hash();
        if (fs == FuncStatusOk) {
            fl_data.is_update_req = false;
            fs = flash_write_sector(FL_STORAGE_AREA, (uint32_t*)&fl_data, sizeof(fl_data));
            k_msleep(1000);
            if (fs == FuncStatusOk)
                jump_to_app(slot_index);
            else
                LOG_ERR("%s: %d: Failed to write flash data to storage area: %d", __FUNCTION__,
                        __LINE__, fs);
        } else {
            LOG_ERR("Hash check failed.");
        }
        break;

    default:
        LOG_WRN("Unknown CAN ID: %x", packed_frame.state);
        return FuncStatusError;  // Unknown CAN ID
    }
    return FuncStatusOk;
}

/*-----------------------------MAIN THREAD--------------------------------*/

/**
 * @brief Main function to initialize the bootloader and manage firmware updates
 */
int main() {
    int version_idx = 0;
    static uint32_t fl_read_arr[FL_SECTOR_SIZE / 4] = {0};
    if (!tc_sha256_init(&sha_ctx)) {
        LOG_ERR("hash init failed");
    }
    if (!device_is_ready(flash_dev)) {
        printk("Flash device not ready!\n");
        return -ENODEV;
    }
    if (!tc_sha256_init(&new_firm)) {
        LOG_ERR("hash init failed");
    }
    can_init();
    LOG_INF("Welcome to the custom bootloader!");
    k_msleep(10);
    flash_read_data(FL_STORAGE_AREA, (uint32_t*)&fl_data, sizeof(fl_data));

    version_idx = find_latest_version(&fl_data);
    LOG_INF("version_idx: %d", version_idx);
    LOG_INF("update req value %d:", fl_data.is_update_req);
    if (version_idx == (-1) || fl_data.is_update_req == true) {
        LOG_INF("DFU ENTRY");
        k_msleep(10);
        goto polling_start;
    }
    size_in_sectors = fl_data.size_in_sectors[version_idx];
    for (uint8_t i = 0; i < size_in_sectors; i++) {
        flash_read_data((app_slot_addr[version_idx]) + (i * FL_SECTOR_SIZE), fl_read_arr,
                        FL_SECTOR_SIZE);
        tc_sha256_update(&sha_ctx, fl_read_arr, sizeof(fl_read_arr));
    }
    tc_sha256_final(output_hash, &sha_ctx);

    int check = (version_idx == 0) ? memcmp(fl_data.hashA[0], output_hash, sizeof(output_hash))
                                   : memcmp(fl_data.hashA[1], output_hash, sizeof(output_hash));

    if (check == 0) {
        LOG_INF("Hash matches, jumping to app slot %d", version_idx);
        flash_write_sector(FL_STORAGE_AREA, (uint32_t*)&fl_data, sizeof(fl_data));
        k_msleep(1000);
        jump_to_app(version_idx);
    } else {
        LOG_WRN("Hash mismatch, continuing bootloader operations");
    }

polling_start:
    LOG_INF("Starting polling loop...");
    FuncStatus fs = FuncStatusOk;
    while (1) {
        if (fs >= FuncStatusOk) poll_data();
        k_msleep(5);
    }
    LOG_WRN("Unexpected Exit$$$");
    return 0;
}