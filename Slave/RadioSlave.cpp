#include "RadioSlave.h"

RadioSlave* RadioSlave::handlerInstance = nullptr;

void RadioSlave::Init(_SPI* spiPort, uint8_t pinCE, uint8_t pinCS, uint8_t pinIRQ, int8_t powerLevel, uint8_t packetSize, uint8_t numberOfSendPackets, uint8_t numberOfReceivePackets, uint8_t frameRate)
{
  handlerInstance = this;
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
  radio.begin(spiPort, pinCE, pinCS);
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
  radio.setChannel(channelList[currentChannelIndex]);
  radio.maskIRQ(true, true, false);
  radio.powerUp();
  radio.startListening();

  //Interrupt for Radio
  attachInterrupt(digitalPinToInterrupt(pinIRQ), StaticIRQHandler, FALLING);

  //Frame Timing
  this->frameRate = (frameRate < 10) ? 10 : ((frameRate > 120) ? 120 : frameRate);  //Clamp between 10 and 120
  microsPerFrame = 1000000 / frameRate;
  halfMicrosPerFrame = microsPerFrame / 2;
  minOverflowProtection = microsPerFrame * 3;
  maxOverflowProtection = 0xffffffff - (microsPerFrame * 3);
  syncDelay = microsPerFrame / 8;
}

void RadioSlave::StaticIRQHandler()
{
  if (handlerInstance != nullptr) 
  {
    handlerInstance->IRQHandler();
  }
}
  
volatile uint32_t lastInterruptTimeStamp = 0;

void RadioSlave::IRQHandler()
{ 
    interruptTimeStamp = micros() + syncDelay;

    if(interruptTimeStamp - lastInterruptTimeStamp < halfMicrosPerFrame) //In case our interrupt acted wierd on multiple packets
    {
      return;
    }
    
    isSyncFrame = true;
    lastInterruptTimeStamp = interruptTimeStamp;
}


void RadioSlave::ClearSendPackets()
{
  for(int i = 0; i < numberOfSendPackets; i++)
  {
    memset(sendPackets[i], 0, packetSize);
    byteAddCounter[i] = 1;
  }
}

void RadioSlave::ClearReceivePackets()
{
  for(int i = 0; i < numberOfReceivePackets; i++)
  {
    receivePacketsAvailable[i] = false;
    memset(recievePackets[i], 0, packetSize);
    byteReceiveCounter[i] = 1;
  }
}

void RadioSlave::SetNextFrameEnd(uint32_t newTime) 
{
  isOverFlowFrame = (newTime < frameTimeEnd);
  frameTimeEnd = newTime;
}

void RadioSlave::AdvanceFrame()
{
    uint32_t localInterruptTimeStamp = interruptTimeStamp;
    bool localIsSyncFrame = isSyncFrame;
    isSyncFrame = false;

    if(localIsSyncFrame)
    {
      if(localInterruptTimeStamp > maxOverflowProtection) 
      {
        SetNextFrameEnd(frameTimeEnd + microsPerFrame);
        return; 
      }
      if(localInterruptTimeStamp < minOverflowProtection) 
      {
        SetNextFrameEnd(frameTimeEnd + microsPerFrame);
        return; 
      }

      uint32_t futureLocalInterruptTimeStamp = localInterruptTimeStamp + microsPerFrame;
      int32_t diffA = localInterruptTimeStamp - frameTimeEnd;
      int32_t diffB = futureLocalInterruptTimeStamp - frameTimeEnd;
      int32_t drift;

      drift = (abs(diffA) < abs(diffB)) ? diffA : diffB;

      SetNextFrameEnd(frameTimeEnd + microsPerFrame + drift);
      if(drift < 0)
      {
        totalAdjustedDrift--;
        microsPerFrame--;
      }
      else
      {
        totalAdjustedDrift++;
        microsPerFrame++;
      }
    }
    else
    {
      SetNextFrameEnd(frameTimeEnd + microsPerFrame);
    }
}

bool RadioSlave::IsFrameReady()
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

void RadioSlave::UpdateScanning(bool isSuccess)
  {
    if(isSuccess) 
    {
      if(radioState == STATE_SCANNING)
      {
        AdjustChannelIndex(2);
        radio.startListening();
        radioState = STATE_PARTIAL_LOCK;
        partialLockCounter = 0;
      }
      else if(radioState == STATE_PARTIAL_LOCK)
      {
        partialLockCounter++;

        if(isSuccess)
        {
          radioState = STATE_FULL_LOCK;
        }

        if(partialLockCounter > 10)
        {
          radioState = STATE_SCANNING;
        }
      }
    }
    else
    { 
      failedCounter++; 
    }  

    if(failedCounter >= failedBeforeScanning)
    {
      failedCounter = 0;
      radioState = STATE_SCANNING;
    }  
  }

void RadioSlave::UpdateSecondCounter()
{
  secondCounter++;
  isSecondTick = false;
  if(secondCounter >= frameRate)
  {
    secondCounter = 0;
    receivedPerSecond = recievedPacketCount;
    recievedPacketCount = 0;
    sentPerSecond = sentPacketCount;
    sentPacketCount = 0;
    isSecondTick = true;
  }
}

  void RadioSlave::AdjustChannelIndex(int8_t amount)
  {      

    currentChannelIndex += amount;

    if(currentChannelIndex >= channelsToHop) {currentChannelIndex = channelsToHop - currentChannelIndex; }
    if(currentChannelIndex < 0) {currentChannelIndex += channelsToHop;}

    hopOnScanCounter++;
    if(hopOnScanCounter >= channelsToHop)
    {
      hopOnScanCounter = 0;
      hopOnScanValue++;
      if(hopOnScanValue >= framesPerHop) {hopOnScanValue = 0;}
    }

    radio.stopListening();
    radio.setChannel(channelList[currentChannelIndex]);
      
  }


  bool RadioSlave::UpdateHop()
  {
    bool needsToHop = false;
    channelHopCounter++;
    if(channelHopCounter >= framesPerHop) {channelHopCounter = 0;}

    if(radioState == STATE_SCANNING)
    {
      if(channelHopCounter == hopOnScanValue)
      {
        AdjustChannelIndex(-1);
        needsToHop = true;
      }
    }
    else if(radioState == STATE_FULL_LOCK)
    {
      if(channelHopCounter == hopOnLockValue)
      {
        AdjustChannelIndex(1);
        needsToHop = true;
      }
    }
    return needsToHop;
  }



void RadioSlave::WaitAndSend()
{
  while(!IsFrameReady()) {}


  bool hasStoppedListening = UpdateHop();
  if(radioState == STATE_FULL_LOCK)
  {
    if(!hasStoppedListening)
    {
      radio.stopListening();
      hasStoppedListening = true;
    }
    for(int i = 0; i < numberOfSendPackets; i++)
    {
      sendPackets[i][0] = i;
      radio.write(sendPackets[i], packetSize);
    }
  }

  if(hasStoppedListening)
  {
    radio.startListening();
  }

  ClearSendPackets();
}

void RadioSlave::Receive()
{
  bool isSuccess = false;
  ClearReceivePackets();
    
  for(int i = 0; i < 3; i++)   //Always check 3 times to clear the input buffers otherwise interrupt wont trigger
  {
    if (radio.available())
    {  
      isSuccess = true;
      recievedPacketCount++;
      failedCounter = 0;
      uint8_t currentPacket[packetSize];
      radio.read(currentPacket, packetSize);
      uint8_t firstByte = currentPacket[0];
      uint8_t packetId = firstByte & 0x03;
      memcpy(recievePackets[packetId], currentPacket, packetSize);
      receivePacketsAvailable[packetId] = true;
      uint8_t txChannelHopCounter = (firstByte & 0xE0) >> 5;
      channelHopCounter = txChannelHopCounter; 
    }
  }

  UpdateScanning(isSuccess);
  UpdateSecondCounter();
}


