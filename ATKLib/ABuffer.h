/* ----------------------------------------------------------- */
/*                                                             */
/*                        _ ___                                */
/*                       /_\ | |_/                             */
/*                       | | | | \                             */
/*                       =========                             */
/*                                                             */
/*        Real-time API for HTK-base Speech Recognition        */
/*                                                             */
/*       Machine Intelligence Laboratory (Speech Group)        */
/*        Cambridge University Engineering Department          */
/*                  http://mi.eng.cam.ac.uk/                   */
/*                                                             */
/*               Copyright CUED 2000-2007                      */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*        File: ABuffer.h - Interface to packet buffers        */
/* ----------------------------------------------------------- */

/* !HVER!ABuffer: 1.6.0 [SJY 01/06/07] */

#ifndef _ATK_BUFFER
#define _ATK_BUFFER

#include "APacket.h"

class ABuffer;

class ABufferCallback {
  public:
    virtual void ABufferReceivedPacket(ABuffer &buffer, APacket &packet) = 0;
};

typedef list<ABufferCallback *> ABufferCallbackList;

struct ABufferEventMsg {
  HThread thread;
  unsigned char id;
};

class ABuffer {
public:
  ABuffer (const string& name, int maxPkts = 0);
  // Construct an empty buffer.  The buffer will block after maxPkts
  // inserted. If maxPkts is zero, buffer never blocks.

  void SetFilter(PacketKind kind);
  // Restrict buffer to only accept packets of given kind.

  void PutPacket(APacket p);
  // Store given packet in the buffer.

  APacket GetPacket();
  // Get next packet from buffer

  APacket PeekPacket();
  // Peek at next packet in buffer, leave it there

  void PopPacket();
  // Pop packet from buffer (Peek+Pop = Get)

  Boolean IsFull();
  // Returns TRUE if the buffer is full ie contains maxPkts

  Boolean IsEmpty();
  // Returns TRUE if the buffer is empty

  int NumPackets();
  // Returns number of packets in buffer

  PacketKind  GetFirstKind();
  // Returns kind of first packet in buffer

  void RequestBufferEvents(unsigned char id);
  // Request calling thread buffer events, with ev.c = id

  void AddCallback(ABufferCallback *callback);
  void RemoveCallback(ABufferCallback *callback);
  
  string GetName();
  
private:
  string bname;                 // name of buffer
  int bsize;                    // max packets to buffer before blocking
  list<APacket> pktList;        // queued packets
  //  typedef list<APacket>::iterator PktEntry;
  HLock lock;                   // lock for critical sections
  HSignal notFull, notEmpty;    // signals for full and empty conditions
  PacketKind filter;            // set to filter packets, default AnyPacket.
  vector<ABufferEventMsg> bevList;  // list of event requests
  int evCount;                  // num events sent so far
  void SendBufferEvents();      // send requested buffer events
  ABufferCallbackList callbackList;
};

#endif
