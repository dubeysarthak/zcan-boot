/**
 * @file can.h
 * @author Sarthak Et (et-sarthak)
 * @brief CAN interface header for system communication
 * @version 1.0
 * @date 2024-10-05
 * 
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

extern struct k_msgq can_receive_q;
int can_init();

// Not in use currently
/*
int can_write(uint32_t id, uint8_t* data);
int send_ack();
int send_nack();

uint32_t crc32_uint32(uint32_t data);
*/