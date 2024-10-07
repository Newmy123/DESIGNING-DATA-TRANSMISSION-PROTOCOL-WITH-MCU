#include "DHT.h"
#include "SPI.h" // SPI library
#include "MFRC522.h" // RFID library (https://github.com/miguelbalboa/rfid)
#include <MPU6050_tockn.h>
#include <Wire.h>
#include <avr/interrupt.h>
#include <DIYables_4Digit7Segment_74HC595.h> // DIYables_4Digit7Segment_74HC595 library
MPU6050 mpu6050(Wire);
#define SCLK  30  // The Arduino pin connected to SCLK
#define RCLK  31  // The Arduino pin connected to RCLK
#define DIO   32  // The Arduino pin connected to DIO
DIYables_4Digit7Segment_74HC595 display(SCLK, RCLK, DIO);
#define DHT11PIN 2//pin sensor dht
#define DHTType DHT11//type dht
DHT HT(DHT11PIN, DHTType);//output pin2 2 , type dht11
#define BUF_SIZE 256
#define CMD_READ 0x0
#define CMD_WRITE 0x1

#define RET_FAIL -1
#define RET_NONE 0
#define RET_SUCCESS 1

#define OFFSET_HEADER_1 0
#define OFFSET_HEADER_2 1
#define OFFSET_CMD 2
#define OFFSET_DATALENGTH 3
#define OFFSET_DATA 4
#define OFFSET_TRAILER_1 5
#define OFFSET_TRAILER_2 6

#define HEADER_1 0xab
#define HEADER_2 0xcd
#define TRAILER_1 0xe1
#define TRAILER_2 0xe2

#define LIGHT 1
#define L_A   0
#define L_B   1

#define ACCELEROMETER 5
#define ACE_1 1

#define TEMPERATURE 2
#define TEMP_1 1

#define LED  6
#define LD_1  1

#define HUMIDITY 3
#define HUMI_1 1

#define RFID 4
#define RF_1 1

#define READ  1
#define OFF   0
#define ON    1
#define STATE 2
#define START 0



uint8_t status;
uint8_t rx_buf [BUF_SIZE], rx_pack[BUF_SIZE];
uint8_t tx_buf [BUF_SIZE], tx_pack[BUF_SIZE];
uint8_t rx_buf_len;
uint8_t tx_buf_len;
uint8_t cmd;
uint8_t address;
uint8_t rw = 0;
uint8_t state_receive = OFFSET_HEADER_1;
uint8_t customer_UUID[4];
const int redled = 8;
const int ylled = 7;
const int pinRST = 5;
const int pinSDA = 53;

MFRC522 mfrc522(pinSDA, pinRST);
int uart_transmit() {
  for (int i = 0; i < tx_buf_len + 6; i++) {
    Serial1.write(tx_pack[i]);
  }

  Serial.print("\nTransmited packet:\t");
  for (int j = 0; j < tx_buf_len + 6; j++) {
    Serial.print(tx_pack[j], HEX);
    Serial.print("\t");
  }
  return RET_SUCCESS;
}

int uart_receive() {
  int incomingByte;
  uint8_t readbuf;
  int readbytes = 0;
  int i;
  i = 0;

  while (Serial1.available() > 0) {
    incomingByte = Serial1.read();
    readbuf = incomingByte & 0xFF;
    if (state_receive == OFFSET_HEADER_1) {
      Serial.print("\nReceived packet:\n");
    }
    Serial.print(readbuf, HEX);
    Serial.print("\t");
    switch (state_receive) {
      case OFFSET_HEADER_1:
        if (readbuf == HEADER_1) {
          state_receive = OFFSET_HEADER_2;
          Serial.println("HEADER 1");
        }
        else {
          Serial.println("[ERROR]: Wrong header 1");
          return RET_FAIL;
        }
        break;
      case OFFSET_HEADER_2:
        if (readbuf == HEADER_2) {
          state_receive = OFFSET_CMD;
          Serial.println("HEADER 2");
        }
        else {
          Serial.println("[ERROR]: Wrong header 2");
          state_receive = OFFSET_HEADER_1;
          return RET_FAIL;
        }
        break;
      case OFFSET_CMD:
        cmd = readbuf;
        state_receive = OFFSET_DATALENGTH;
        Serial.println("CMD");
        break;
      case OFFSET_DATALENGTH:
        rx_buf_len = readbuf;
        state_receive = OFFSET_DATA;
        Serial.println("DATALENGTH");
        break;
      case OFFSET_DATA:
        if (i < rx_buf_len - 1) {
          rx_buf[i++] = readbuf;
        }
        else {
          rx_buf[i++] = readbuf;
          state_receive = OFFSET_TRAILER_1;
        }
        Serial.println("DATA");
        break;
      case OFFSET_TRAILER_1:
        if (readbuf == TRAILER_1) {
          state_receive = OFFSET_TRAILER_2;
          Serial.println("TRAILER_1");
        }
        else {
          Serial.println("[ERROR]: Wrong trailer 1");
          state_receive = OFFSET_HEADER_1;
          return RET_FAIL;
        }
        break;
      case OFFSET_TRAILER_2:
        state_receive = OFFSET_HEADER_1;
        if (readbuf == TRAILER_2) {
          Serial.println("TRAILER_2");
          return RET_SUCCESS;
        }
        else {
          Serial.println("[ERROR]: Wrong trailer 2");
          return RET_FAIL;
        }
        break;
      default:
        break;
    }
  }
  return RET_FAIL;
}
void led_function(int temp)
{
  display.printInt(temp, false);
}

ISR (TIMER1_OVF_vect)
{
  TCNT1 = 61536;
  display.loop();
}

void compose_packet() {
  int i = 4;
  tx_pack[0] = HEADER_1;
  tx_pack[1] = HEADER_2;
  tx_pack[2] = cmd;
  tx_pack[3] = tx_buf_len;
  for (i; i < tx_buf_len + 4; ++i)
    tx_pack[i] = tx_buf[i - 4];
  tx_pack[i] = TRAILER_1;
  tx_pack[i + 1] = TRAILER_2;
}

void setup() {
  SPI.begin(); // open SPI connection
  mfrc522.PCD_Init(); // Initialize Proximity Coupling Device (PCD)
  Serial.begin(9600);
  Serial1.begin(9600);
  pinMode(redled, OUTPUT);
  pinMode(ylled, OUTPUT);
  //set up cho DHT
  HT.begin();
  delay(1000);
  //
  Wire.begin();
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);
  //interupt for led 7 segment
  cli();                                  // tắt ngắt toàn cục

  /* Reset Timer/Counter1 */
  TCCR1A = 0;
  TCCR1B = 0;
  TIMSK1 = 0;

  /* Setup Timer/Counter1 */
  TCCR1B |= (1 << CS11) | (1 << CS10);    // prescale = 64
  TCNT1 = 61536;
  TIMSK1 = (1 << TOIE1);                  // Overflow interrupt enable
  sei();                                  // cho phép ngắt toàn cục
  display.printInt(0, false);

}
void loop() {
  status = RET_FAIL;
  //rw = 0;

  status = uart_receive();
  switch (cmd) {
    case LIGHT:
      if (rx_buf[0] == L_A) {
        if (rx_buf[1] == OFF) {
          digitalWrite(redled, LOW);
        } else if (rx_buf[1] == ON) {
          digitalWrite(redled, HIGH);
        } else if (rx_buf[1] == STATE) {
          int i = digitalRead(redled);
          tx_buf_len = 2;
          tx_buf[0] = rx_buf[0];
          tx_buf[1] = i;
          compose_packet();
          if (status == RET_SUCCESS) {
            uart_transmit();
          }
        }
      }
      if (rx_buf[0] == L_B) {
        if (rx_buf[1] == OFF) {
          digitalWrite(ylled, LOW);
        } else if (rx_buf[1] == ON) {
          digitalWrite(ylled, HIGH);
        } else if (rx_buf[1] == STATE) {
          int b = digitalRead(ylled);
          tx_buf_len = 2;
          tx_buf[0] = rx_buf[0];
          tx_buf[1] = b;
          compose_packet();
          if (status == RET_SUCCESS) {
            uart_transmit();
          }
        }
      }
      break;
    case TEMPERATURE:
      if (rx_buf[0] == TEMP_1)
      {
        if (rx_buf[1] == READ) {
          int temp = HT.readTemperature();
          if (status == RET_SUCCESS) {
            Serial.print("\nTemperature:");
            Serial.print(temp);
            tx_buf_len = 2;
            tx_buf[0] = rx_buf[0];
            tx_buf[1] = temp;
            compose_packet();
            uart_transmit();
          }
        }
      }
      break;
    case HUMIDITY:
      if (rx_buf[0] == HUMI_1)
      {
        if (rx_buf[1] == READ) {
          int humi = HT.readHumidity();
          if (status == RET_SUCCESS) {
            Serial.print("\nHumidity:");
            Serial.print(humi);
            tx_buf_len = 2;
            tx_buf[0] = rx_buf[0];
            tx_buf[1] = humi;
            compose_packet();
            uart_transmit();
          }
        }
      }
      break;
    case RFID:
      if (rx_buf[0] == RF_1)
      {
        if (rx_buf[1] == READ) {
          if (status == RET_SUCCESS) {
            if (mfrc522.PICC_IsNewCardPresent()) { // (true, if RFID tag/card is present ) PICC = Proximity Integrated Circuit Card
              if (mfrc522.PICC_ReadCardSerial()) { // true, if RFID tag/card was read
                Serial.print("RFID TAG ID:");
                for (byte i = 0; i < mfrc522.uid.size; ++i) { // read id (in parts)
                  Serial.print(mfrc522.uid.uidByte[i], HEX); // print id as hex values
                  customer_UUID[i] = (mfrc522.uid.uidByte[i]);
                  Serial.print(" "); // add space between hex blocks to increase readability
                }
                Serial.println(); // Print out of id is complete.
              }
              for (int i = 0 ; i < 4; i++) {
                Serial.println(customer_UUID[i]) ;
              }
              tx_buf[2] = customer_UUID[0];
              tx_buf[3] = customer_UUID[1];
              tx_buf[4] = customer_UUID[2];
              tx_buf[5] = customer_UUID[3];
              tx_buf_len = 5;
              tx_buf[0] = rx_buf[0];
              tx_buf[1] = rx_buf[1];
              compose_packet();
              uart_transmit();
            }
            delay(2000);
          }
        }
      }
      break;
    case ACCELEROMETER:
      if (rx_buf[0] == ACE_1) {
        if (rx_buf[1] == READ) {
          mpu6050.update();
          if (status == RET_SUCCESS) {
            Serial.print("angleX : ");
            Serial.print(mpu6050.getAngleX());
            Serial.print("\tangleY : ");
            Serial.print(mpu6050.getAngleY());
            Serial.print("\tangleZ : ");
            Serial.println(mpu6050.getAngleZ());
            tx_buf_len = 5;
            tx_buf[0] = rx_buf[0];
            tx_buf[1] = rx_buf[1];
            tx_buf[2] = mpu6050.getAngleX();
            tx_buf[3] = mpu6050.getAngleY();
            tx_buf[4] = mpu6050.getAngleZ();
            compose_packet();
            uart_transmit();
          }
        }
      }
      break;
    case LED:
      if (rx_buf[0] == LD_1) {
        if (rx_buf[1] == ON) {
          int temp = HT.readTemperature();
          if (status == RET_SUCCESS) {
            Serial.print("\nTemperature:");
            Serial.print(temp);
            led_function(temp);
          }
        }
      }
      break;
  }
}
