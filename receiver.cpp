#include <cstdlib>
#include <iostream>
#include <RF24/RF24.h>

using namespace std;


//
// Hardware configuration
//
RF24 radio(RPI_V2_GPIO_P1_22, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ);
const uint8_t pipes[][6] = {"LNX13"};


struct sensors_data {
  unsigned long cold_counter;
  unsigned long hot_counter;
  float temp;
  unsigned long voltage;
  };

sensors_data data;
//
// Setup
//
void setup(void) {
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(1, pipes[0]);
  radio.startListening();
  //radio.printDetails();
}

int main(int argc, char** argv){
        unsigned long coldInit;
        unsigned long hotInit;
        if(argc != 3){
                fprintf(stderr, "./receiver COLD_METER_INIT_VALUE HOT_METER_INIT_VALUE\n");
                return -1;
        }
        coldInit = atol(argv[1]);
        hotInit = atol(argv[2]);

        setup();

        while(1) {
                if (radio.available()) {
                        radio.read(&data, sizeof(data));
                        printf( "-   water.cold   %lu\r\n"
                                "-   water.hot   %lu\r\n"
                                "-   water.temp   %f\r\n"
                                "-   water.voltage   %lu\r\n",
                                data.cold_counter+coldInit,
                                data.hot_counter+hotInit,
                                data.temp,
                                data.voltage);

                        fflush(stdout);
                }
                delay(925); //from example to reduce cpu load
        }
        return 0;
}
