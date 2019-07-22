
#include <digitalWriteFast.h>
#include <avr/power.h>
#include <LowPower.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>
#include <OneWire.h>



//defines
#define PIN_HOT_COUNTER 8
#define PIN_COLD_COUNTER 9
#define PIN_SENSOR_ENABLE 5
#define PIN_ONE_WIRE 2

//fast macros
#define SET_PIN(b) ( (b)<8 ? PORTD |=(1<<(b)) : PORTB |=(1<<(b-8)) )
#define CLR_PIN(b) ( (b)<8 ? PORTD &=~(1<<(b)) : PORTB &=~(1<<(b-8)) )

//variables
OneWire  ds(PIN_ONE_WIRE);
RF24 radio(17,10); 
byte addresses[][6] = {"1Node"};
enum session_enum {IN_SESSION, OUT_SESSION};
enum state_enum {CHECK_SENSORS, SEND_DATA};
struct sensors_data {
  unsigned long cold_counter;
  unsigned long hot_counter;
  float temp;
  unsigned long voltage;
  };

struct debounce {
  unsigned char black;
  unsigned char white;
  };

volatile sensors_data data;
volatile state_enum state = CHECK_SENSORS;

void measureTemp() {
  ds.reset();
  ds.write(0xCC); //skip rom
  ds.write(0x44);        // start measure
  }

float getTemp() {
  byte data[12];
  
  ds.reset();
  ds.write(0xCC);
  ds.write(0xBE);         // Read Scratchpad
  ds.read_bytes(data, 9);

  int16_t raw = (data[1] << 8) | data[0];
  
  byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
    
  return (float)raw / 16.0;
  }

unsigned long readVcc() { 
  power_adc_enable(); //power adc enable
  ADCSRA |= (1 << ADEN); //adc enable

  long result; 
  // Read 1.1V reference against AVcc 
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle 
  ADCSRA |= _BV(ADSC); 
  // Convert 
  while (bit_is_set(ADCSRA,ADSC)); 
  result = ADCL; 
  result |= ADCH<<8; 
  result = 1126400L / result; // Back-calculate AVcc in mV 

  ADCSRA &= ~ (1 << ADEN); //adc disable
  power_adc_disable(); //power adc disable
  return result;
  }

void checkSensors() {
  static unsigned char sessionCounter;
  static unsigned char sessionExitCounter;
  static unsigned int hourCounter;
  static session_enum session = OUT_SESSION;
  static debounce hot_debounce;
  static debounce cold_debounce;
  bool hot_tmp;
  bool cold_tmp;
  
  //reading values routine
  SET_PIN(PIN_SENSOR_ENABLE);
  SET_PIN(PIN_COLD_COUNTER); //pull-up enable
  SET_PIN(PIN_HOT_COUNTER); //pull-up enable
  delayMicroseconds(800);
  hot_tmp = digitalReadFast(PIN_HOT_COUNTER);
  cold_tmp = digitalReadFast(PIN_COLD_COUNTER);
  CLR_PIN(PIN_SENSOR_ENABLE); //turnoff led
  CLR_PIN(PIN_COLD_COUNTER);
  CLR_PIN(PIN_HOT_COUNTER);

  //logic
  //cold counter  
  if (cold_tmp) { // on white part
    if (cold_debounce.black < 2) {
      cold_debounce.black = 0;
      } else { 
        cold_debounce.white += cold_debounce.white < 2?1:0;
        if (2 == cold_debounce.white) {
          //2 times black then 2 times white triggered
          data.cold_counter++;
          sessionExitCounter=0;
          if (session == OUT_SESSION) {
            session = IN_SESSION;
            measureTemp();
            state = SEND_DATA;
            }
          cold_debounce.white = 0;
          cold_debounce.black = 0;
          }
        }
    } else { //on black part
      cold_debounce.black += cold_debounce.black < 2?1:0;
        if (cold_debounce.white < 2) {
          cold_debounce.white = 0;
          }
      }

  //hot counter      
  if (hot_tmp) { // on white part
    if (hot_debounce.black < 2) {
      hot_debounce.black = 0;
      } else { 
        hot_debounce.white += hot_debounce.white < 2?1:0;
        if (2 == hot_debounce.white) {
          //2 times black then 2 times white trigered
          data.hot_counter++;
          sessionExitCounter=0;
          if (session == OUT_SESSION) {
            session = IN_SESSION;
            measureTemp();
            state = SEND_DATA;
            }
          hot_debounce.white = 0;
          hot_debounce.black = 0;
          }
        }
    } else { //on black part
      hot_debounce.black += hot_debounce.black < 2?1:0;
        if (hot_debounce.white < 2) {
          hot_debounce.white = 0;
          }
      }

   //send data every 200*250 ms if water was used
   if (session == IN_SESSION) {
    sessionCounter++;
    sessionExitCounter++;
    if (200 == sessionCounter) {
     sessionCounter = 0;
     measureTemp();
     state = SEND_DATA;
     if (sessionExitCounter > 30){
      session = OUT_SESSION;
     }
    }
   }
   
   hourCounter++;
   if (14400 == hourCounter) { //60sec*60min*4times/sec
    hourCounter = 0;
    measureTemp();
    state = SEND_DATA;
    }
   
  }

void setup() {
  //disable adc for powersave
  ADCSRA &= ~ (1 << ADEN); //adc disable
  power_adc_disable(); //power adc disable
  
  //configure radio
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);  
  radio.openWritingPipe(addresses[0]);
  radio.stopListening();
  
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_HOT_COUNTER, INPUT);
  pinMode(PIN_COLD_COUNTER, INPUT);
  pinMode(PIN_SENSOR_ENABLE, OUTPUT);

  //init ds18b20
  ds.reset();
  ds.write(0xcc); //skip rom
  ds.write(0x4e); //write config
  ds.write(0);
  ds.write(0);
  //ds.write(0x1f); //9-bit resolution
  ds.write(0x3f); //10-bit resolution
  
}



void loop() {
  // put your main code here, to run repeatedly:
  switch(state) {
    case SEND_DATA:
      data.voltage=readVcc();
      data.temp = getTemp();
      radio.write(&data, sizeof(sensors_data));
      state = CHECK_SENSORS;
    
    case CHECK_SENSORS:
      checkSensors();
      break;
    }

  LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);  
}
