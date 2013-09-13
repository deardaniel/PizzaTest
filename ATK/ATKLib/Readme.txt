ATK Library

This directory contains the ATK library plus a number of simple test
programs.   The ATK Library contains the following modules

ABuffer - provides a communication link between ATK components
ACode   - component to transform waveform packets to feature packets
AComponent - abstract class defining common features of all ATK components
ADict   - interface to pronunciation dictionaries
AGram   - support for word level recognition networks
AHmms   - interface to HTK format HMMs
AHTK    - interface to HTK libraries
AIO     - generic asynchronous speech-in/speech-out support
ALog    - support for data logging
AMonitor- display of component status and trace output
ANGram  - interface to statistical LMs
APacket - defines format of information passed between packets
ARec    - the ATK recogniser component
AResource - generic resource (base class for dicts, hmms, grams and lms) 
ARMan   - resource manager
ASource - component providing speech input and speech output
ASyn    - speech synthesis component
ATee    - allows a packet stream to be forked 


Functional Test Progams
-----------------------

Each of these should be executed from its own directory where 
there is a specific config file, eg TBase.cfg and any further
files needed.  Details on running the programs are supplied
in corresponding Readme files.

TBase			- displays 3 boxes holding red balls, click
			      on a box to move ball from one to another.
			      Tests functionality of core ATK object types
TSource			- test source, recording and playback
TCode			- test coder fed from audio source
TRec			- test recogniser
TSyn            - test the synthesiser
TIO             - tests the AIO asynchronous I/O interface.

