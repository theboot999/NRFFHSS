#include "RadioMaster.h"
#include "Spi.h"

//In this example we are hopping between 40 different channels. We change channel once every 2 frames. At 50HZ we are changing channels every 40ms
//The Master is sending 1 packet per Frame - It should be receiving up to 50 packets per Second
//The Slave is sending 2 packets per frame - It should be receiving up to 100 packets per Second
//Each packet can take up to 31 bytes of useable data.  Adding and retrieving data is done sequentially with an example below on how to do this
//Make sure you set 115200 Baud Rate in your Serial Monitor

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

  //Print out what channel we are on
  Serial.print("c");
  Serial.print(radio.GetCurrentChannel());
  Serial.print(" ");

  //Print out once a second how many packets a second our Master and Slave are Receiving/Sending
  if(radio.IsSecondTick())  //Method returns true once a second
  {
    Serial.println();
    Serial.print("Slave Received ");
    Serial.print(slaveRecPerSecond);

    Serial.print(" Master Received ");
    Serial.println(radio.GetRecievedPacketsPerSecond());    //Method to see how many packets we have received per second
  }
}

//Functions Below to show how to add and retrieve data from each packet
void AddSendData()
{
  //Data can be sent using PACKET1, PACKET2 or PACKET3
  //The number of sent and received packets in use per frame is set as one of the definitions and passed into the Init Function
  //Data is added sequentially into each packet.  The amount of data we can add into the packet is 1 less than Define PACKET_SIZE which is passed into the Init Function
  //Eg for 4 x int16_t values we need 8 bytes + 1.  So PACKET_SIZE should be 9 bytes
  //Both Master and Slave need to have the same PACKET_SIZE.  Not all bytes need to be used in each packet

  int16_t masterRecPerSecond = radio.GetRecievedPacketsPerSecond();
  uint32_t masterMicros = micros();
  uint16_t value2 = 5343;
  int8_t value3 = 143;
  
  //Add data to Packet 1.  We can add 1 less byte than packet byte size
  radio.AddNextPacketValue(PACKET1, masterRecPerSecond);
  radio.AddNextPacketValue(PACKET1, masterMicros);
  radio.AddNextPacketValue(PACKET1, value2);
  radio.AddNextPacketValue(PACKET1, value3);
}

void ProcessReceived()
{
  //Must call if IsNewPacket before processing
  //When retrieving the data from the Packet we must do it in the same order as it was added on the slave
  //On recieving the template also Requires a typecast to the type we are retrieving eg <int16_t>
  
  if(radio.IsNewPacket(PACKET1))            //Call to see if theres new values for Packet1
  {
    slaveRecPerSecond = radio.GetNextPacketValue<int16_t>(PACKET1);
    int16_t number16Bit = radio.GetNextPacketValue<int16_t>(PACKET1);
    uint8_t numberU8Bit =  radio.GetNextPacketValue<uint8_t>(PACKET1);
  }
  if(radio.IsNewPacket(PACKET2))            //Call to see if theres new values for Packet2
  {
    float numberFloat = radio.GetNextPacketValue<float>(PACKET2);
    uint32_t numberU32Bit = radio.GetNextPacketValue<uint32_t>(PACKET2);
  }
}
