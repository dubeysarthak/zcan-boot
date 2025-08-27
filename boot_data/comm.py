import can 
import time

class CanFrame:
    def __init__(self, can_id, data_bytes):
        self.can_id = can_id
        self.data_bytes = data_bytes


can_interface = "can0"  # Change this to your CAN interface


def can_init():
    global bus
    bus = can.interface.Bus(channel=can_interface, bustype='socketcan')

def can_shutdown():
    time.sleep(1)
    bus.shutdown()


def send_can_message(can_id, data_bytes):
    try:
        msg = can.Message(arbitration_id=can_id,
                          data=data_bytes,
                          is_extended_id=False)

        bus.send(msg)
        print(f"Sent: ID=0x{can_id:X}, Data={[f'0x{b:02X}' for b in data_bytes]}")
        print("")

    except can.CanError as e:
        print(f"CAN error: {e}")


def receive_can_data(timeout):
    
    message = bus.recv(timeout)
    if message is None:
        print("No message received within timeout.")
        return 0x00
    else:
        return CanFrame(message.arbitration_id, message.data)
    

