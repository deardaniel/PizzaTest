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
/*      File: APacket.h -    Interface for Packet Classes      */
/* ----------------------------------------------------------- */

/* !HVER!APacket: 1.6.0 [SJY 01/06/07] */

#ifndef _ATK_Packet
#define _ATK_Packet

#include "AHTK.h"

// ------------------- Packet Data -----------------------

// Abstract type representing various kinds of packet data

enum PacketKind{
   EmptyPacket,         // contains nothing at all
   StringPacket,        // simple text strings
   CommandPacket,       // "SendMessage" commands
   WavePacket,          // array of 16bit waveform samples
   ObservationPacket,   // a HTK observation
   PhrasePacket,        // a semantic hypothesis ie a tagged phrase
   AnyPacket            // used where mixed-packet kinds are allowed
};

class APacketData {    // virtual class for various packet data types
public:               // --- concrete data types at end of file
   virtual ~APacketData(){}
   virtual void Show() = 0;
   friend class APacket;
   PacketKind kind;
};

// ----------------------- Packet Header ---------------------

// Provides a standard wrapper for packet data which includes
// a reference count and time stamp information

typedef class APacketHeader *PacketRef;   // pointer to core packet info

class APacketHeader { // common part of all packets
public:
   APacketHeader(APacketData * apd);
   ~APacketHeader();
   void Show();
   friend class APacket;
protected:
   HTime startTime,endTime;
   int count;
   APacketData *theData;
};

// ------------------------ Packet --------------------------

class APacket{  // outer wrapper for packets
public:
   APacket();                      // Empty packet (rarely used)
   APacket(APacketData *apd);      // New data packet
   APacket(const APacket& pkt);    // Share with existing packet
   APacket& operator=(const APacket& pkt);   // Assign by sharing
   ~APacket();   // Destroy embedded header+data when ref count 0
   void Show();  // Print abbreviated contents of packet

   // Get/Put Properties
   HTime GetStartTime();
   void SetStartTime(HTime t);
   HTime GetEndTime();
   void SetEndTime(HTime t);
   APacketData *GetData();
   PacketKind  GetKind();
private:
   PacketRef thePkt;
};

// --------------------- Empty Container --------------------------

class AEmptyData : public APacketData {
public:
   AEmptyData();
   void Show();
};

// ------------------- Container for Commands ----------------------

#define MAXCMDARG 10
class ACommandData : public APacketData {
public:
   ACommandData(const string& cmd);  // initialise with name of cmd
   ~ACommandData();
   Boolean AddArg(const string& s);
   Boolean AddArg(const int i);
   Boolean AddArg(const float f);
   string GetCommand();
   int NumCmdArgs();
   Boolean IsString(int n);   // true if n'th arg is a string
   Boolean IsNumber(int n);   // true if n'th arg is a number
   string GetString(int n);   // get n'th arg
   float GetFloat(int n);
   int GetInt(int n);
   void Show();
private:
   enum Kind {args,argi,argf };
   union Value{
      string *s;
      int i;
      float f;
   };
   struct Argument {
      Kind kind;
      Value value;
   };
   string cmdname;
   int numArgs;
   Argument arg[MAXCMDARG];
};

// ------------------- Container for Strings ----------------------

class AStringData : public APacketData {
public:
   AStringData();                    // create empty string
   AStringData(const string& s);     // create string s
   string GetSource();
   // unpack source from SOURCE::MARKER type strings
   string GetMarker(Boolean tagOnly=FALSE);
   // unpack marker from SOURCE::MARKER type strings.  If tagOnly,
   // then only the first tagword is returned
   void Show();
   string data;       // actual string
};


// ----------------- Container for Waveforms ----------------------

class AWaveData : public APacketData {
public:
   AWaveData();                      // create empty wave
   AWaveData(const int n, short *x); // create with x[0..n-1]
   void Show();
   int wused;                        // num samples in packet
   short data[WAVEPACKETSIZE];       // actual wave data
};

// ------------------- Container for Observations -------------------

class AObsData : public APacketData {
public:
   AObsData(BufferInfo *info, int numStreams);
   ~AObsData();
   void Show();
   Observation data;
};

// --------------------- Container for Phrases ----------------------

enum PhraseType {
   Start_PT,    // start utterance marker, tag=file name if Wave file input
   End_PT,      // end utterance marker, ignore contents
   OpenTag_PT,  // open tagged phrase
   CloseTag_PT, // close tagged phrase, always tagged
   Null_PT,     // null, might have a tag
   Word_PT,     // word or phrase, might have a tag
   Quit_PT      // notify application of termination
};

class APhraseData :  public APacketData {
public:
   APhraseData(PhraseType pt, int thisseq, int lastseq);
   void Show();
   PhraseType ptype;     // kind of this phrase data
   int seqn;            // seq no of this packet
   int pred;            // seq no of pred/succ packets
   int alt;             // seq no of alt packets (OpenTag only)
   string word;         // word/phrase
   string tag;          // tag
   float ac;            // acoustic score (log likelihood)
   float lm;            // lm score (log likelihood)
   float score;         // total score (log likelihood)
   float confidence;    // acoustic confidence (0->1, or -1 if invalid)
   float nact;          // average active model count
};

#endif
/*  ---------------------- End of APacket.h ----------------------- */



