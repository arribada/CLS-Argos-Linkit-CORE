# GenTracker Application

## How to build
The GenTracker application project uses CMake and the arm-none-eabi-gcc compiler.
The instructions below describe how to build and flash this application.

### Debug Version
```
cmake -DCMAKE_TOOLCHAIN_FILE=toolchain_arm_gcc_nrf52.cmake -DCMAKE_BUILD_TYPE=Debug -DBOARD=<HORIZON|GENTRACKER> -DMODEL=<SURFACEBOX|UNDERWATER> .
make 
```

### Release Version
```
cmake -DCMAKE_TOOLCHAIN_FILE=toolchain_arm_gcc_nrf52.cmake -DCMAKE_BUILD_TYPE=Release  -DBOARD=<HORIZON|GENTRACKER> -DMODEL=<SURFACEBOX|UNDERWATER> .
make
```

## How to flash the firmware
With the device connected via USB run the following command
```
nrfutil dfu usb-serial -pkg PACKAGE_dfu.zip -p DEVICE
```
Where:
* `PACKAGE` is the DFU zip file created by running `make`
* `DEVICE` is the USB device as it appears to the OS (e.g. COM7 or ttyACM0)
