TSOURCE

This test program exercises the audio input/output module ASource.  It
is invoked as

   TSource -C TSource.cfg options ...

where the options are 

   -l file     - log to file
   -r          - replay each input
   -s          - start sampling immediately
   -t freq     - output a tone of given freq (in kHz)
   -p file     - output HTK file instead of tone
   -j time     - tone duration in seconds
   -k time     - tone burst period in seconds
   -m cycles   - stop after m cycles

When checking out the audio subsystem, the following sequence of tests
is recommended.

1.  TSource -C TSource.cfg -r

The audio control widget should appear.  Press the start button, speak
and then press the button again to stop the audio input.  You should
then hear your speech replayed.  If not check that the input and
output audio is set up correctly.

2. TSource -C TSource.cfg -r -l foo.wav

This is the same as test 1 except the audio also gets recorded to the
file foo.wav.  Record your voice, then click away the audio widget to
terminate the program.  Use an audio display program such as CoolEdit
to view foo.wav (note that by default it will be 16000Hz mono).  The
waveform should be clean with little noise.

=====================================================================

THE REMAINING TESTS REFER TO ECHO CANCELLING 
THIS IS NOT YET FULLY FUNCTIONAL IN V1.6
IGNORE THE REMAINING TESTS AND ENSURE THAT
ECHOCANCEL IS NEVER SET TRUE
=====================================================================

3. TSource -C TSource.cfg -t 1.0 -m 4 -s -l foo.wav

Before running this command ensure that

HAUDIO: ECHOCANCEL = F

is set in the config file TSource.cfg.  This command will play a 1kHz
tone burst 4 times, whilst simultaneously recording.  Check the
recorded waveform.  If you are using loud speakers, you will probably
see the tone echoed back on the input.  Now set

HAUDIO: ECHOCANCEL = T

and repeat the process.  The echoed waveform should be substantially
reduced, except perhaps at the edges where the tone is not suppressed.

4. TSource -C TSource.cfg -p prompt.wav -m 4 -s -l foo.wav

Repeat test 3, however this time, the tone is replaced by a spoken
prompt.  The cancellation in this case should be substantial with
minimal edge effects.
