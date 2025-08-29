import time
import sys
from bin_parser import *
from comm import *


og_bin = "boot_data/zephyr.bin"
og_txt = "boot_data/zephyr.bin.ascii"

log_txt = "boot_data/log.txt"
file = open(log_txt, "w")

null_byte = bytes([0x00])

def send_hash_to_can(hash_val):
    for i in range(len(hash_val)):
        print(i)
        send_can_message(0x100, hash_val[i])
        time.sleep(0.1)


def send_bin_data_to_can(file_data):
    counter =0
    for i in range(len(file_data.bin_data)):
        byte_data = bytes.fromhex(file_data.bin_data[i])
        print("i=",i)
        send_can_message(0x104, byte_data)
        time.sleep(0.01)
        counter+=1
        if counter == 1024:
            
            send_can_message(0x108, null_byte)
            counter = 0
            time.sleep(8)
    if counter != 0:
        send_can_message(0x108, null_byte)
    

def main():
    can_init()
    run_xxd(og_bin, og_txt)

#writing to file
    file_data = split_bin_file(og_txt)
    file.write("Big-endian data:\n")
    for i in range(len(file_data.hex_data)):
        file.write(f"0x{file_data.hex_data[i]:04X}\n")
    file.write("\n")

#Generating hash
    hash_val = get_hash(file_data.hex_data)
    file.write("Hash values:\n")
    for i in range(len(hash_val)):
        file.write(f"0x{hash_val[i].hex()}\n")
    file.write("\n")

#sending data over CAN
    send_hash_to_can(hash_val)
    send_bin_data_to_can(file_data)
    send_can_message(0x10c, null_byte)

    can_shutdown()
    sys.exit(0)


if __name__ == "__main__":
    main()
