TIO

This test program executes a simple demo of using AIO.  The system
asks for a number to dial, and then waits for a user response.  If the
number is too short, the system will say so and reprompt.  Otherwise,
it will announce that it is dialing the number.  If the user does not
respond within the set timeout period, then the system says "Sorry I
cant hear you" and reprompts.

A typical dialog might be

S: What number do you wish to dial?
U: 456 7892
S: Dialing four five six seven eight nine two.
S: What number do you wish to dial?
U: (Silence)
S: Sorry I cant hear you. 
S: What number do you wish to dial?

The test is invoked as

  TIO -C TIO.cfg
  
Along with the usual widgets, the AIO display appears.  The upper bar
displays the synthesiser activity and the lower bar displays the
recogniser.  The colour codes are:

               Dark Green    Light Green    Yellow       Orang      Red
SYN (Upper)    Announcing    Prompting      Muted                   Aborted
REC (Lower)    Recognising                  SpDetected   Filling    Timeout

When the user speaks the speech detector triggers and the lower bar
goes yellow.  If the recogniser then starts it may detect a filler
word (orange) or real speech (dark green).  If the synthesiser is
active when the user speaks, the synthesiser will be muted and the syn
display turns yellow.  If the recogniser believes it is real speech
(ie the user is barging in and not just making a breath noise), then
the synthesiser is aborted (upper bar turns briefly red and then goes
back to grey).

Testing that AIO is functioning properly requires the following checks
to be made:

a) let the prompts finish and speak a clear response.  The system
   should recognise the number and "call it"

b) cough during a prompt.  The prompt should be muted, the upper bar
   should turn briefly yellow and the lower bar should turn
   yellow/orange.

c) speak a telephone number before prompt finishes.  The prompt should
   mute and then stop, the upper bar should go yellow then red.  The
   spoken number should be recognised.

d) stay silent during and after a prompt, a red bar should appear on the
   lower display to indicate a timeout.

NOTE THAT SINCE THE MICROPHONE IS ALWAYS OPEN WHEN USING AIO, YOU MUST
ENSURE THAT THE FEEDBACK FROM ANY SPEAKERS TO THE MIC IS MINIMISED.
IDEALLY, RUN THIS TEST USING HEADPHONES.
