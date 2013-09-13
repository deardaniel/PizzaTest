HTKLIB

This directory holds the ATK versions of the HTK library.  Many of the HTK
modules have been changed to support ATK and in addition there is a new
thread library called HThreads.   The main changes are

HAudio - a completely new version with Windows and Linux i/o routines plus
         support for asynchronous recognition/synthesis and echo cancellation
HGraf  - now supports multiple windows
HLM    - extended to support ATK's trigram recognizer
HModel - triphone synthesis added so that given the decision tree created
         in training, adding new dictionary entries does not cause a 
         "missing triphone" error.
HNet   - various modifications plus a new graphical debug facility
HRec   - modified to support trigrams, plus other optimisations
HParm  - modified to support ATK's real time I/O
HThreads - new thread library presenting a uniform interface to both Windows
         threads and Linux pthreads.

This directory also contains some simple test programs for testing
network building, graphics and threads.



