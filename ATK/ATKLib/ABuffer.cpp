/* ----------------------------------------------------------- */
/*                                                             */
/*                        _ ___                                */
/*                       /_\ | |_/                             */
/*                       | | | | \                             */
/*                       =========                             */
/*                                                             */
/*       Real-time API for HTK-based Speech Recognition        */
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
/*    File: ABuffer.cpp -   Implementation of Buffer Class     */
/* ----------------------------------------------------------- */

char * abuffer_version="!HVER!ABuffer: 1.6.0 [SJY 01/06/07]";

// 24/07/05 - sync locks added to buffer status checks though
//       its not clear if they are an unecessary overhead

#include "ABuffer.h"

// Constructor: every buffer has a name.
// If maxPkts==0 (the default), size is unlimited.
ABuffer::ABuffer(const string& name, int maxPkts)
{
  string s;

  bname = name;  bsize = maxPkts;
  // create lock and two signals for thread synchronisation
  s = name+":lock";
  lock = HCreateLock(s.c_str());
  s = name+":notFull";
  notFull = HCreateSignal(s.c_str());
  s=name+":notEmpty";
  notEmpty = HCreateSignal(s.c_str());
  filter = AnyPacket;   // default is no filtering
  evCount = 0;
}

// Set filter kind
void ABuffer::SetFilter(PacketKind kind)
{
  filter = kind;
}

// Push packet p onto the end of the pktList.  Wait for nonFull
// if the buffer is full
void ABuffer::PutPacket(APacket p)
{
  assert(filter == AnyPacket || p.GetKind() == filter);
  
  for (ABufferListenerList::iterator l = listenerList.begin(); l != listenerList.end(); l++) {
    ABufferListener *listener = *l;
    listener->ABufferReceivedPacket(*this, p);
  }
  
  HEnterSection(lock);
  while (bsize!=0 && int(pktList.size())>= bsize) {
    HWaitSignal(notFull, lock);
  }
  pktList.push_back(p);
  HLeaveSection(lock);
  HSendSignal(notEmpty);
  SendBufferEvents();
}

// Get packet p from the front of the pktList.  Wait for nonEmpty
// if the buffer is empty.
APacket ABuffer::GetPacket()
{
  HEnterSection(lock);
  while (pktList.size()==0) {
    HWaitSignal(notEmpty, lock);
  }
  APacket p = pktList.front();
  pktList.pop_front();
  HLeaveSection(lock);
  HSendSignal(notFull);
  return p;
}

// Copy packet from the front of the pktList.  Wait for nonEmpty
// if the buffer is empty.  Leave the packet where it is.
APacket ABuffer::PeekPacket()
{
  HEnterSection(lock);
  while (pktList.size()==0) {
    HWaitSignal(notEmpty, lock);
  }
  APacket p = pktList.front();
  HLeaveSection(lock);
  return p;
}

void ABuffer::PopPacket()
{
  HEnterSection(lock);
  while (pktList.size()==0) {
    HWaitSignal(notEmpty, lock);
  }
  pktList.pop_front();
  HLeaveSection(lock);
  HSendSignal(notFull);
  SendBufferEvents();
}

Boolean ABuffer::IsFull()
{
  HEnterSection(lock);
  if (bsize==0) return FALSE;
  Boolean ans = (int(pktList.size())>=bsize)?TRUE:FALSE;
  HLeaveSection(lock);
  return ans;
}

Boolean ABuffer::IsEmpty()
{
  HEnterSection(lock);
  Boolean ans = (pktList.size()==0)?TRUE:FALSE;
  HLeaveSection(lock);
  return ans;
}


int ABuffer::NumPackets()
{
  HEnterSection(lock);
  int np = pktList.size();
  HLeaveSection(lock);
  return np;
}

// Peek at kind of first item in buffer
PacketKind  ABuffer::GetFirstKind()
{
  PacketKind pk = AnyPacket;
  HEnterSection(lock);
  if (pktList.size()>0){
    APacket p = pktList.front();
    pk = p.GetKind();
  }
  HLeaveSection(lock);
  return pk;
}

// Request calling thread buffer events, with ev.c = id
void ABuffer::RequestBufferEvents(unsigned char id)
{
  ABufferEventMsg msg;
  msg.thread = HThreadSelf();  msg.id = id;
  bevList.push_back(msg);
  // send any events already missed
  for (int i = 1; i<=evCount; i++) HBufferEvent(msg.thread, msg.id);
}

// Send requested buffer events to all requesting threads
void ABuffer::SendBufferEvents()
{
  ABufferEventMsg msg;
  int size = bevList.size();
  ++evCount;
  for (int i=0; i<size; i++) {
    msg = bevList[i];
    HBufferEvent(msg.thread, msg.id);
  }
}

void ABuffer::AddListener(ABufferListener *listener)
{
    listenerList.push_back(listener);
}

void ABuffer::RemoveListener(ABufferListener *listener)
{
    listenerList.remove(listener);
}

string ABuffer::GetName()
{
  return bname;
}

// ------------------------ End ABuffer.cpp ---------------------


