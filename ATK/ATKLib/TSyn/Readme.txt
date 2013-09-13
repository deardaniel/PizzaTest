TSYN

This test program executes a continuous prompt-recognition cycle which
is designed to stress-test the audio input/output synchronisation
controls ie muting and aborting of the prompt output, as well as the
TTS itself.  Invoke as

  TSyn -C TSyn.cfg
  
You should hear the prompt "Please provide more input if you would be
so kind" repeated endlessly.  The program includes various random
pauses with the result that the prompt is sometimes muted and
sometimes aborted. The audio widget should should the input auudio
channel being continually switched on and off.

If this program runs continuously then the test is probably ok!

