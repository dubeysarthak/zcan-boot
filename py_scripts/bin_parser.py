import math 
import hashlib
import subprocess
import struct
from main import file

class FileData:
    def __init__(self, NR_lines, NR_sectors, bin_data, hex_data):
        self.NR_lines = NR_lines
        self.NR_sectors = NR_sectors
        self.bin_data = bin_data
        self.hex_data = hex_data

def run_xxd(input_file, output_file):
    with open(output_file, "w") as f:
        subprocess.run(["xxd", input_file], stdout=f)
        

mylist = []
my_hex_list = []
def split_bin_file(bin_file):
    with open(bin_file, "r") as f:
        number_of_line = len(f.read().split("\n")) - 1
        number_of_sectors = math.ceil(number_of_line / 512)
        f.seek(0,0)
        for line in f:
            line = line.split(" ")
            for i in range(0,9):
                if line[i] == "":
                    line[i] = "0000"
            word1 = line[1] + line[2] + line[3] + line[4]
            word2 = line[5] + line[6] + line[7] + line[8]
            hex1 = int("0x"+line[1] + line[2],16)
            hex2 = int("0x"+line[3] + line[4],16)
            hex3 = int("0x"+line[5] + line[6],16)
            hex4 = int("0x"+line[7] + line[8],16)
            mylist.append(word1)
            mylist.append(word2)
            my_hex_list.append(hex1)
            my_hex_list.append(hex2)
            my_hex_list.append(hex3)
            my_hex_list.append(hex4)
    
    if len(mylist) % 512 != 0:
        mylist.extend(["ffffffffffffffff"] * (512 - (len(mylist) % 512)))
    if len(my_hex_list) % 1024 !=0:
        my_hex_list.extend([0xffffffff] * (1024 - (len(my_hex_list) % 1024)))
    return FileData(number_of_line, number_of_sectors, mylist, my_hex_list)

def get_hash(bin_list):
    full_data = b''.join(struct.pack('>I', word) for word in bin_list)
    hash = hashlib.sha256(full_data).digest()
    print("Expected hash:", hash.hex())
    chunks = [hash[i:i+8] for i in range(0, len(hash), 8)]
    return chunks
