#include "RadioSlave.h"
#include "Spi.h"

#define SPI_PORT SPI                //SPI PORT IN USE.  Uno or Nano is MOSI Pin 11, MISO Pin 12, SCK Pin 13
#define CE_PIN 6                    //CE Pin connected to the NRF
#define CS_PIN 5                    //CS Pin connected to the NRF
#define IRQ_PIN 3                   //Slave Requires the IRQ Pin connected to the NRF.  Arduino Uno/Nano can be Pin 2 or 3
#define POWER_LEVEL 0               //0 lowest Power, 3 highest Power (Use seperate 3.3v power supply for NRF above 0)
#define PACKET_SIZE 16              //Max 32 Bytes. Must match the Masters Packet Size. How many bytes you are maximum packing into each packet.  Useable size is 1 less than this as first byte is PacketID and Hopping information
#define NUMBER_OF_SENDPACKETS 2     //Max of 3 Packets.  How many packets per frame the slave will send.  The master needs to have the same amount of receive packets
#define NUMBER_OF_RECEIVE_PACKETS 1 //Max of 3 Packets.  How many packets per frame the slave will receive.  The Master needs to have the same amoutn of send packets
#define FRAME_RATE 50               //Locked frame rate of the microcontroller. Must match the Masters Framerate

RadioSlave radio;

void setup() 
{
  Serial.begin(115200);
  //Init must be called first with the following defined Parameters
  radio.Init(&SPI_PORT, CE_PIN, CS_PIN, IRQ_PIN, POWER_LEVEL, PACKET_SIZE, NUMBER_OF_SENDPACKETS, NUMBER_OF_RECEIVE_PACKETS, FRAME_RATE);
}

void loop() 
{
  radio.WaitAndSend();    //Must be called at the start of every frame.  Is blocking until the frame time is up
  radio.Receive();        //Call this second on every Frame
  
  ProcessReceived();      //Method below to process received data
  AddSendData();          //Method below to add Send data
}

void AddSendData()
{
  int16_t slaveRecPerSecond = radio.GetRecievedPacketsPerSecond();             //Get the number of Packets we are receiving per second
  int16_t number16Bit = 23145;      //Useless variable we will send
  uint8_t numberU8Bit = 50;         //Useless variable we will send
  float numberFloat = 302.234f;     //Useless variable we will send
  uint32_t numberU32Bit = 2342521;  //Useless variable we will send
  
  //Add data to Packet 1.  We can add 1 less byte than packet byte size
  radio.AddNextPacketValue(PACKET1, slaveRecPerSecond);
  radio.AddNextPacketValue(PACKET1, number16Bit);
  radio.AddNextPacketValue(PACKET1, numberU8Bit);

  //Add data to Packet 2.  We can add 1 less byte than packet byte size
  radio.AddNextPacketValue(PACKET2, numberFloat);
  radio.AddNextPacketValue(PACKET2, numberU32Bit);
}

void ProcessReceived()
{
  //Must call if IsNewPacket before processing
  //Packet contents must be processed in the order they were sent from the Master
  //On recieving the template also Requires a typecast to the type we are retrieving eg <int16_t>

  if(radio.IsNewPacket(PACKET1))  //Call to see if theres new values for Packet1
  {
    int16_t  masterRecPerSecond = radio.GetNextPacketValue<int16_t>(PACKET1);
    uint32_t  masterMicros = radio.GetNextPacketValue<uint32_t>(PACKET1);    
    uint16_t value2 = radio.GetNextPacketValue<uint16_t>(PACKET1);
    int8_t value3 = radio.GetNextPacketValue<int8_t>(PACKET1);
  }
}
