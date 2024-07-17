#include "RadioMaster.h"

static RadioMaster* instance; 

void RadioMaster::Init(_SPI* spiPort, uint8_t pinCE, uint8_t PinCS, int8_t powerLevel, uint8_t packetSize, uint8_t numberOfSendPackets, uint8_t numberOfReceivePackets, uint8_t frameRate)
{
  //Packets
  instance = this;
  this->numberOfSendPackets = (numberOfSendPackets < 0) ? 0 : ((numberOfSendPackets > 3) ? 3 : numberOfSendPackets);
  this->numberOfReceivePackets = (numberOfReceivePackets < 0) ? 0 : ((numberOfReceivePackets > 3) ? 3 : numberOfReceivePackets);
  this->packetSize = (packetSize < 1) ? 1 : ((packetSize > 32) ? 32 : packetSize);
  powerLevel = (powerLevel < 0) ? 0 : ((powerLevel > 3) ? 3: powerLevel);

  for (int i = 0; i < numberOfSendPackets; ++i) 
  {
    sendPackets[i] = new uint8_t[packetSize]();
  }

  for (int i = 0; i < numberOfReceivePackets; ++i) 
  {
    recievePackets[i] = new uint8_t[packetSize]();
  }

  ClearSendPackets();
  ClearReceivePackets();

  //Radio
  spiPort->begin();
  radio.begin(spiPort, pinCE, PinCS);
  radio.stopListening();
  radio.powerDown();
  radio.setPALevel(powerLevel);
  radio.setAddressWidth(3);
  radio.openReadingPipe(1,addressRec);
  radio.openWritingPipe(addressTransmit);
  radio.setDataRate(RF24_1MBPS);
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPayloadSize(packetSize);
  //radio.setChannel(channelList[currentChannelIndex]);
  radio.setChannel(108);
  radio.maskIRQ(true, true, false);
  radio.powerUp();
  radio.startListening();

  //Frame Timing
  this->frameRate = (frameRate < 10) ? 10 : ((frameRate > 120) ? 120 : frameRate);  //Clamp between 10 and 120
  microsPerFrame = 1000000 / frameRate;
}

void RadioMaster::ClearSendPackets()
{
  for(int i = 0; i < numberOfSendPackets; i++)
  {
    memset(sendPackets[i], 0, packetSize);
    byteAddCounter[i] = 1;
  }
}

void RadioMaster::ClearReceivePackets()
{
  for(int i = 0; i < numberOfReceivePackets; i++)
  {
    receivePacketsAvailable[i] = false;
    memset(recievePackets[i], 0, packetSize);
    byteReceiveCounter[i] = 1;
  }
}

void RadioMaster::AdvanceFrame()
{
  uint32_t newTime = frameTimeEnd + microsPerFrame;
  isOverFlowFrame = (newTime < frameTimeEnd);
  frameTimeEnd = newTime;
}

bool RadioMaster::IsFrameReady()
{
	uint32_t currentTimeStamp = micros();
  uint32_t localFrameTimeEnd = frameTimeEnd;
   
  if(isOverFlowFrame)
  {
    currentTimeStamp -= 0x80000000;
    localFrameTimeEnd -= 0x80000000;
  }

  if (currentTimeStamp >= localFrameTimeEnd)
	{
    AdvanceFrame();
    return true;
  }
    return false;
}

void RadioMaster::UpdateRecording()
{
  secondCounter++;
  isSecondTick = false;
  if(secondCounter >= frameRate)
  {
    secondCounter = 0;
    receivedPerSecond = recievedPacketCount;
    recievedPacketCount = 0;
    isSecondTick = true;
  }
}

void RadioMaster::WaitAndSend()
{
  while(!IsFrameReady()) {}

  radio.stopListening();
  
  for(int i = 0; i < numberOfSendPackets; i++)
  {
    sendPackets[i][0] = i;
    sendPackets[i][0] |= ((channelHopCounter << 5) & 0xE0);
    radio.write(sendPackets[i], packetSize);
  }  

  channelHopCounter++;
  if(channelHopCounter >= framesPerHop)
  { 
    channelHopCounter = 0;
    currentChannelIndex++;
    if(currentChannelIndex >= channelsToHop) { currentChannelIndex = 0; }
    radio.setChannel(channelList[currentChannelIndex]);
  }

  radio.startListening();
  ClearSendPackets();
}

void RadioMaster::Receive()
{
  ClearReceivePackets();

  for(int i = 0; i < 3; i++)  //Always check 3 times to clear the input buffers
  {
    if (radio.available())
    {       
      recievedPacketCount++;
      uint8_t currentPacket[packetSize];
      radio.read(currentPacket, packetSize);
      uint8_t firstByte = currentPacket[0];
      uint8_t packetId = firstByte & 0x03;
      memcpy(recievePackets[packetId], currentPacket, packetSize);
      receivePacketsAvailable[packetId] = true;
    }
  }

  UpdateRecording();
}


