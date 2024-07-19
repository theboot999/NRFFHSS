# NRFFHSS

Frequency hopping library for 2 way communication between 2 NRF24L01 radio modules.   Requires Maniacs RF24 library.  

- Switches Channel every 2 frames using 50 different channels
- Runs on a fixed preset frame rate
- Fast initial syncing time.
- Requires the interrupt pin on the slave device.
- Timed packet sending.  No missed packets from transceivers missing incoming packets while being in Send mode
- Includes packing and unpacking of sent and recieved packets. 
- Uses no Ack packets. Send and forget.
- Uses Micros for timing so should be compatible with many Microcontrollers.

## Usage
Example sketches are included for the Master and Slave.  

There are up to 3 individual packets that can be sent per frame.  The first byte in each packet is automatically used for the packet identification and the channel hop count.  The rest are useable.

The packet identifiers are defined as PACKET1, PACKET2, PACKET3.  

The following methods must be called:
1. Init - must be called in setup
2. WaitAndSend - must be called at the start of the loop.  It will block until the next frame is ready to start then send
3. Receive - should be called after send
4. AddPacketValue - adds the next value to a packet
5. IsNewPacket - call before getting unpacking a packet
6. GetPacketValue - gets the next value from a packet

As per the example, adding information to the packet is done by AddPacketValue.  Retrieving information is done by calling GetPacketValue.  GetPacketValue must be called in the same order as AddPacketValue.

## Use Case
The Typical use case would be for an RC Transmitter and Receiver.  Allowing both Master and Slave to send and receive up to 3 individual packets per frame with up to 31 useable bytes per frame.

If you are running the NRFS at the lowest transmit speed of 256kb/s and using 3 packets per frame be aware of the frame time.  Running at 120fps with a low transmit speed will cause the NRF to take too long to send each packet. Check for stability by calling GetRecievedPacketsPerSecond.

## How The Frequency Hopping Works
The Master follows a fixed channel sequence, hopping forward in the sequence once every 2 frames.  It's send time is always consistently the same at the start of every frame.

The Slave uses the NRF's interrupt to record a timestamp when a packet is in its recieve buffer.  This time stamp is then synced to its internal frame clock.  The slave will always start its next frame 1/8th of a frame after the Masters frame to avoid any packet collisions.

The Slave will adjust its overall frame time to adjust for any drift from differences in the microcontrollers clock crystal. This drift in microseconds can be read by calling GetDriftAdjustmentMicros.

To Sync, the slave will set itself in syncing mode. No packets will be sent from the slave while syncing.  It will itterate backwards through the channel sequence until it recieves a packet from the Master.

In case of the Master turning off and on again the slave will switch to scanning mode after not receiving a packet for 120 frames.  It is very reliable at re syncing quickly.  With 50 channel hops and at 100 frames per second it typically will resync in about 250 milliseconds.

## Limitations

Currently it uses a fixed 50 channel sequence of channels to hop through as well as fixed receive and send addresses.

If there is any interest I can create a binding method, where a transmitter and reciever will bind together with a unique random channel hopping sequence and random send and recieve addresses saved to Eeprom.
