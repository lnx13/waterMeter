# waterMeter
Get values from pawogaz water meter with arduino and raspberry pi

Compile RPI receiver
------------
g++ -Wall -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s -L/usr/local/lib -lrf24 receiver.cpp -o receiver