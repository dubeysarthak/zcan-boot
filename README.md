# CAN Bootloader Using ZEPHYR

This repository contains CAN Bootloader for Zephyr RTOS environment. This application
uses Tinycrypt library for generating Hash Data, which can be added as zephyr library. 
It also contains scripts for generating CAN frames from binary file of application.

## Getting Started

Before getting started, make sure you have a proper Zephyr development
environment. Follow the official
[Zephyr Getting Started Guide](https://docs.zephyrproject.org/3.7.1/getting_started/index.html).


### Initialization

The first step is to initialize the workspace folder (``my-workspace``) where
the ``zcan-boot`` and all Zephyr modules will be cloned. Run the following
command:

```shell
# initialize my-workspace for the zcan-boot (main branch)
west init -m https://github.com/dubeysarthak/zcan-boot --mr main my-workspace
# update Zephyr modules
cd my-workspace
west update
```

### Building and running

To build the application, run the following command:

```shell
cd zcan-boot
west build -b $BOARD app
```

where `$BOARD` is the target board.

This example has overlay file explicit for stm32h5 in `app/boards`. You can
change it for your microcontroller.

Once you have built the application, run the following command to flash it:

```shell
west flash
```

### Script
This repo also contains python script to generate CAN frames in required format from the bin file. Copy your bin file under `zcan-boot/py_scripts`. You can create a virtual env to install libraries like **python-can**. after activating your venv run the following command to start sending the CAN frames.

``` shell
python3 scripts/main.py
```
**Note:** Make sure your CAN interface device is connected and initialised. 

### Application
While developing the application make sure you have ``CONFIG_BOOTLOADER_MCUBOOT=y`` in proj.conf file. It is not required for bootloader.
