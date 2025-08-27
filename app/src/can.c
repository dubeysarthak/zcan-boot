#include "../../include/can.h"
#include "../../include/flash_config.h"

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>

#define SYSTEM_CANBUS_NODE  DT_ALIAS(system_can)
#define BATTERY_CANBUS_NODE DT_ALIAS(bat_can)
#define CAN_BITRATE_500    500000

K_MSGQ_DEFINE(can_receive_q, sizeof(struct frame_packed), 20, 4);

LOG_MODULE_REGISTER(can, CONFIG_LOG_MAX_LEVEL);

int err;
int ccb_filter_id;
int batt_filter_id;

const struct device* system_can_dev = DEVICE_DT_GET(SYSTEM_CANBUS_NODE);
const struct device* batt_can_dev = DEVICE_DT_GET(BATTERY_CANBUS_NODE);

const struct can_filter rx_filter = {.flags = 0U, .id = 0x220, .mask = 0x000};

void can_ccb_callback(const struct device* dev, struct can_frame* frame, void* user_data) {
    struct frame_packed packed_frame = {0};
    packed_frame.state = ((frame->id - 0x100)/4);
    memcpy(&packed_frame.data, frame->data, sizeof(frame->data));
    k_msgq_put(&can_receive_q, &packed_frame, K_NO_WAIT);
}

void can_batt_callback(const struct device* dev, struct can_frame* frame, void* user_data) {
    LOG_INF("Received battery CAN frame with ID: 0x%03X", frame->id);
}

int can_init() {
    ccb_filter_id = can_add_rx_filter(system_can_dev, can_ccb_callback, NULL, &rx_filter);
    batt_filter_id = can_add_rx_filter(batt_can_dev, can_batt_callback, NULL, &rx_filter);
    if (!device_is_ready(system_can_dev)) {
        LOG_WRN("CAN device not ready");
    }
    if (!device_is_ready(batt_can_dev)) {
        LOG_WRN("CAN device not ready");
    }
    err = can_set_bitrate(system_can_dev, CAN_BITRATE_500);
    if (err != 0) {
        LOG_WRN("%s %d Error in setting system CAN bitrate %d", __FUNCTION__, __LINE__, err);
    }
    err = can_set_bitrate(batt_can_dev, CAN_BITRATE_500);
    if (err != 0) {
        LOG_WRN("%s %d Error in setting system CAN bitrate %d", __FUNCTION__, __LINE__, err);
    }
    err = can_start(batt_can_dev);
    if (err != 0) {
        LOG_WRN("Error starting CAN controller (err %d)", err);
    }
    err = can_start(system_can_dev);
    if (err != 0) {
        LOG_WRN("Error starting CAN controller (err %d)", err);
        return err;
    }
    return err;
}



/*
int can_write(uint32_t id, uint8_t* data) {
    struct can_frame frame = {0};
    frame.id = id;
    frame.dlc = 8;  // Data length code, max 8 bytes
    memcpy(frame.data, data, frame.dlc);
    return can_send(system_can_dev, &frame, K_FOREVER, NULL, NULL);
}

int send_ack() {
    struct can_frame frame = {0};
    frame.id = 0x020;          // Acknowledgment frame ID
    frame.dlc = 8;             // Data length code, max 8 bytes
    uint32_t ack_data = 0x01;  // Example data
    uint32_t crc = crc32_uint32(ack_data);
    memcpy(frame.data, &ack_data, sizeof(ack_data));
    memcpy(frame.data + sizeof(ack_data), &crc, sizeof(crc));
    return can_send(system_can_dev, &frame, K_FOREVER, NULL, NULL);
}

int send_nack() {
    struct can_frame frame = {0};
    frame.id = 0x020;           // acknowledgment frame ID
    frame.dlc = 8;              // Data length code, max 8 bytes
    uint32_t nack_data = 0x00;  // Example data
    uint32_t crc = crc32_uint32(nack_data);
    memcpy(frame.data, &nack_data, sizeof(nack_data));
    memcpy(frame.data + sizeof(nack_data), &crc, sizeof(crc));

    return can_send(system_can_dev, &frame, K_FOREVER, NULL, NULL);
}
*/