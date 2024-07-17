#include "RadioMaster.h"
#include "Spi.h"

#define SPI_PORT SPI                  //SPI PORT IN USE.  Uno or Nano is MOSI Pin 11, MISO Pin 12, SCK Pin 13
#define CE_PIN 6                      //CE Pin connected to the NRF
#define CS_PIN 5                      //CS Pin connected to the NRF
#define POWER_LEVEL 0                 //0 lowest Power, 3 highest Power (Use seperate 3.3v power supply for NRF above 0)
#define PACKET_SIZE 16                //Max 32 Bytes. Must match the slave packet size.  How many bytes you are maximum packing into each packet.  Useable size is 1 less than this as first byte is PacketID and Hopping information
#define NUMBER_OF_SENDPACKETS 1       //Max of 3 Packets.  How many packets per frame the Master will send.  The Slave needs to have the same amount of receive packets
#define NUMBER_OF_RECEIVE_PACKETS 2   //Max of 3 Packets.  How many packets per frame the Master will receive.  The Slave needs to have the same amoutn of send packets
#define FRAME_RATE 50                 //Locked frame rate of the microcontroller. Must match the Slaves Framerate

RadioMaster radio;
int16_t slaveRecPerSecond; //Useless value to store slave received per second from incoming packet

void setup() 
{
  Serial.begin(115200);

  //Init must be called first with the following defined Parameters
  radio.Init(&SPI_PORT, CE_PIN, CS_PIN, POWER_LEVEL, PACKET_SIZE, NUMBER_OF_SENDPACKETS, NUMBER_OF_RECEIVE_PACKETS, FRAME_RATE);
}

void loop() 
{
  radio.WaitAndSend();  //Must be called at the start of every frame.  Is blocking until the frame time is up
  radio.Receive();      //Call this second on every Frame
  
  ProcessReceived();    //Method below to process received data
  AddSendData();        //Method below to add Send Data

  if(radio.IsSecondTick())  //Method returns true once a second
  {
    Serial.print("Slave Received ");
    Serial.print(slaveRecPerSecond);

    Serial.print(" Master Received ");
    Serial.println(radio.GetRecievedPacketsPerSecond());    //Method to see how many packets we have received per second
  }
}


void AddSendData()
{
  int16_t masterRecPerSecond = radio.GetRecievedPacketsPerSecond();
  uint32_t masterMicros = micros();
  
  //Add data to Packet 1.  We can add 1 less byte than packet byte size
  radio.AddPacketValue(PACKET1, masterRecPerSecond);
  radio.AddPacketValue(PACKET1, masterMicros);
}



void ProcessReceived()
{
  //Must call if IsNewPacket before processing
  //Packet contents must be processed in the order they were sent from the Master
  //On recieving the template also Requires a typecast to the type we are retrieving eg <int16_t>
  
  if(radio.IsNewPacket(PACKET1))            //Call to see if theres new values for Packet1
  {
    slaveRecPerSecond = radio.GetPacketValue<int16_t>(PACKET1);
    int16_t number16Bit = radio.GetPacketValue<int16_t>(PACKET1);
    uint8_t numberU8Bit =  radio.GetPacketValue<uint8_t>(PACKET1);
  }
  if(radio.IsNewPacket(PACKET2))            //Call to see if theres new values for Packet2
  {
    float numberFloat = radio.GetPacketValue<float>(PACKET1);
    uint32_t numberU32Bit = radio.GetPacketValue<uint32_t>(PACKET1);
  }
}
