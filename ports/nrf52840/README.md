# GenTracker Application

## How to build
The GenTracker application project uses CMake and the arm-none-eabi-gcc compiler.
The instructions below describe how to build and flash this application.

### Debug Version
```
cmake -DCMAKE_TOOLCHAIN_FILE=toolchain_arm_gcc_nrf52.cmake -DCMAKE_BUILD_TYPE=Debug -DDEBUG_LEVEL=4 -DBOARD=<HORIZON|LINKIT> -DMODEL=<CORE|SB|UW> .
make
```

### Release Version
```
cmake -DCMAKE_TOOLCHAIN_FILE=toolchain_arm_gcc_nrf52.cmake -DCMAKE_BUILD_TYPE=Release -DBOARD=<HORIZON|LINKIT> -DMODEL=<CORE|SB|UW> .
make
```

## How to flash the firmware in DFU mode
With the device connected via USB run the following command
```
make dfu_app
```
