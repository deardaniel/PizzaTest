		 ATK/HTK Speech Recognition Software
		 ===================================

		       RELEASE 1.6  June 2007
				   
            == Windows/Visual Studio and Linux Version ==


LICENSE TERMS  
"""""""""""""

This software is copyright Cambridge University.  It may not be copied
or distributed to 3rd parties without the permission of the copyright
owner.  It is distributed for research and non-commercial use only.
The full license terms are in the accompanying file "License.txt".

Although not a license condition, all users of this package are
requested to report all bugs by emailing sjy@eng.cam.ac.uk.  Please
include ATK in the subject line - messages without ATK in the subject
line are likely to be ignored (see Reporting Problems below).

Contents
""""""""

ATK is distributed as a single tarball containing all of the source
files and a basic set of UK English recognition resources for testing.


a) ATKLib:   the ATK source libraries for real-time applications of 
             HTK-based recognisers

b) HTKLib:   an ATK compatible version of the HTK Libraries with various
             extensions.  This library is compatible with the functionality
             of HTK3.4 but it can only be used with ATK.
             
c) SYNLib:   an interface to Alan Black's flite synthesiser plus support for
             a US male English voice: US_English, CMU_Lexicon, CMU_US_KAL16.

d) ATKApps:  a set of example ATK applications.
                ssds - a very simple speech-in/text-out dialog
                asds - a speech in/speech out demo of the asynchronous audio
                       i/o support provided by ATK's AIO interface
                avite - an ATK equivalent to HTK's HVite which is
                       provided for off-line testing of recognition
                       resources intended for us in ATK.
e) ATKDoc:   documentation for ATK

             
f) Resources:
             HMM models, dictionaries and support files for use in ATK
             applications.  See the Readme files in each directory for
             more information.

             1) UK_SI_MFCC  - UK English, speaker independent, MFCC encoding.
             2) UK_SI_ZMFCC - UK English, speaker independent, ZMFCC encoding.

             Note that these resources are provided just to provide a
             quick start for application developers.  No claims are
             made about the performance of these model sets.  The main
             point in using ATK is that it allows developers to build
             their own resources using HTK.


Compile Instructions (Windows)
""""""""""""""""""""""""""""""

This distribution of ATK requires Visual Studio 2005.  It can be built with
earlier versions, but the project files would need to be created from scratch
since VS 2005 project file format is not compatible with earlier versions.

It is strongly recommended that you have Cygwin installed and you run ATK
console programs from a bash shell rather than cmd.exe (see www.Cygwin.com).

A. Building the basic libraries and test Programs

 Assuming that the entire ATK source tree is stored in a directory 
 called atk, open VS by double clicking on atk/atk.sln.  
 Set the configuration to Debug in the main tool bar (when everything
 has been built and tested, release versions can be built).
 The entire system can be built by selecting 'build solution' from the 
 build menu.  Alternatively, if problems are encountered or
 you wish to understand the structure of the software better, it
 is recommended that you build ATK in stages as follows:
    
 1. Using the solution explorer, right click on HTKLib and build it.

 2. In the same way, build ATKLib.

 3. In the solution explorer select TBase as the "startup project", and
    build it.  Then select 'Start (F5)' from the Debug menu.   You should see
    3 yellow windows with red balls in each.  Click on a window and
    check that the balls get transferred from window to window.  This
    tests the ATK base classes. 

 4. Now build TSource, TCode and TRec in the same way.  TSource just
    tests the audio source, TCode tests the audio plus coder and 
    TRec tests the full recognition system.   TRec expects inputs of
    the form "Dial one three nine seven" etc.  All test programs 
    Txxx have a config file of the form Txxx.cfg and a Readme.txt file
    giving more information.  Note that it is easiest to run test programs
    from the command line rather than from withing Visual Studio.
    
 5. Build SYNLib and its dependent libraries, US_English, CMU_Lexicon, 
    CMU_US_KAL16.  Then build TSyn and run it to test the synthesiser.

 6. Build TIO.  This  program tests ATK's asynchronous input/output
    facilities.  Like TRec it expects input of the form 
    "Dial one three nine seven", but it provides spoken prompts,
    and supports time-outs and barge-in. 


B. The ATK Applications

  ATK 1.6 includes three demonstration programs.  ssds and asds are
  simple spoken dialog applications which can be built and tested in
  the same way as the test programs above.   Ssds provides speech input
  but text output.  Asds is very similar except that it also supports
  speech output with barge-in.  (Note ATK does not currently support
  echo cancellation - hence to avoid feedback from speakers to microphone
  do all initial testing with a headset earphones and close talking mic.)
  
  AVite is really a tool, provided for off-line testing of HTK
  resources (note that HVite uses a different front end to AVite and
  does not support trigram LMs).  AVite can be tested by opening a
  shell in the Test directory and typing arun.  The resulting help
  message indicates the options that can then be tried.  The
  recognition accuracy for the cases which use a language model should
  should be 100%.


Compiling instructions (Linux)
""""""""""""""""""""""""""""""

The environment variables for the compiler, compiler flags and link flags
(HTKCC, HTKCF, HTKCF) can be set, otherwise the defaults will be used.

ATK requires ALSA for the sound input and output.  Some older linux
distributions (newer than 2002/pre-2.5 kernel) do not have the ALSA
drivers installed.  If this is the
case, go to http://www.alsa-project.org/ and install.
http://alsa.opensrc.org contains more information about installing ALSA.

The linux build requires both X11 and the pthreads libraries.  If the
location of the X11 library is non-standard (ie, not in
/usr/X11R6/lib), edit the Makefiles and/or the linker flag (HTKLF) 
to reflect this.

The top-level directory contains a Makefile which will compile all of
the subcomponents in any particular combination desired.  "make All"
will make the HTK, ATK and Flite (TTS) libraries, together with the
sample applications in ATKApps and the test applications in ATKLib.  

Typing "make" in the top level ATK directory will show the various
options.  Alternatively, it is possible to make each component by
visiting each directory and making the libraries individually.

Release Notes for 1.6
"""""""""""""""""""""

This is a major release.  HAudio has been rewritten and a new AIO
module added to provide support for simultaneous speech out and audio
in.  Linux audio has been switched from OSS to ALSA.

A synthesis interface called ASyn has been added and Alan Black's
flite synthesis software has been integrated in order to provide "out
of the box" speech output.

Support for N-best output has been implemented. 

A simple data logging module has been added.

Support for building HTKTools with the ATK libaries has been dropped
since it provides no extra functionality and adds unnecessary
complexity.  Hence, the ATK directive is no longer required and one
version of the HTKLib library is needed (but note that this is NOT
compatable with the version of HTKLib in the standard HTK distribution).

Various bugs have been fixed, especially a serious bug in the network
building routines which prevented proper implementation of duplicated
subnetworks.

Release Notes for 1.5
"""""""""""""""""""""

This was an internal CUED-only release.

Release Notes for 1.4.1
"""""""""""""""""""""""

This was a minor bug fix release.

Release Notes for 1.4
"""""""""""""""""""""

1) The main new features in this release are full support for trigram
language models and Linux support.

2) The use of Visual Studio V6 has been replaced by the .NET V7 version.

3) A variety of minor bugs have been fixed.

Release Notes for 1.3
"""""""""""""""""""""

1) Each directory has a Visual Studio .dsw.  To compile a library or
application, open this file and then build using the Build menu (eg
F7) as normal.

2) HTKLib has two sets of settings labeled _ATK and _HTK.  The _ATK
version defines the compilation flag ATK which changes the behaviour
of the standard HTK lib files.  These changes are mainly concerned
with multithreading (HThreads) and the way that real time audio coding
is handled (ie HAudio and HParm).

3) HTKLib has also been enhanced in various other minor ways compared
to the current public HTK release (currently at version 3.2).  These
changes include extensions to HRec/HLM to support trigram language 
models and confidence scoring, HModel to support triphone synthesis and
HGraf to support multiple windows.  HNet also has a trace option which allows
a visual plot of the phone level recognition network to be displayed.

4) Only VS6 settings for a "debug" build have been implemented.

5) Compiled HTK tools are left in the local Debug or Release
directories.  To copy them to a 'bin' directory (specified by setting
the shell variable HBIN to the required path) run the 'install_tools'
script.

6) The ATKLib contains a number of simple test programs (named
Tlibname).  These should be built and tested before attempting
to build any ATK based applications.

7) This release contains 1 set of 4-mix word internal MFCC
triphones (MFCC_0_D_A) and 1 set of 4-mix word internal MFCC triphones
with the cepstral mean removed (MFCC_0_D_A_Z).  This latter set
requires running average cepstral mean removal to be enabled when
using ATK.  Both these acoustic models were trained using WSJCAM0.
Both model sets include the decision tree used for state tying
embedded within them.  This allows HModel to synthesise arbitrary
triphones on demand.  The _Z set also have a background model
supplied with them.

Reporting Problems
""""""""""""""""""

All users of ATK are requested to report bugs by emailing
sjy@eng.cam.ac.uk.  ATK must be included in the subject line otherwise
the message will almost certainly be treated as spam and ignored.  All
bug reports should include version information.  To obtain this in a
command line application, simply set the -V option as in HTK.  For all
other applications, set PRINTVERSIONINFO=T in the configuration file.
In both cases, the release number and individual component versions
are printed on the console.  If the ATK monitor is being used for
console output, then the display area will need to made large enough
to view the version information.


Last Updated
May 2007 by SJY



