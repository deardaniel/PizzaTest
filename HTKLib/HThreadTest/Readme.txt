HTHREADTEST 

HThreadTest is used to invoke some very simple tests of the ATK thread
library HThreads.

Basic usage is 

   HThreadTest n arg1 arg2 ...

where n defines the test number are arg1 ... are the test specific
args.  There are currently just 4 tests:

1.  ParallelForkAndJoin

Invoke as

    HThreadTest 1 numthreads delay

This creates n threads (n=numthreads).  Each thread is numbered
1,2,..,n.  When invoked, thread n goes round a loop n times.  Each
loop it waits for the given delay and then outputs a message.  The
main thread waits for all spawned threads to terminate and then stops.
Example: 3 threads, delay = 50secs

> HThreadTest 1 3 50
 creating 3 threads
   1 created
   2 created
   3 created
 waiting for termination
Test1 Thread 1
Test1 Thread 2
Test1 Thread 3
   1 terminated with status 1
Test1 Thread 2
Test1 Thread 3
   2 terminated with status 2
Test1 Thread 3
   3 terminated with status 3

2.  SimpleMutex

Invoke as

    HThreadTest 2 numthreads lockon

This creates n threads.  Each thread does some work (actually opens
this readme file) and increments a shared counter.  If lockon = 1, the
critical section is protected by a semaphore, if lockon = 0, it is
not.  On termination, the total count should be exactly 1.  Typically,
if lockon=0, the sum will not be one due to contention.
Example:

> HThreadTest 2 4 0
 creating 4 threads
   1 created
   2 created
HThreadTest is used to invoke some very simple tests of the ATK thread
HThreadTest is u
   3 created
   4 created
 waiting for termination
HThreadTest is used to invoke some very simple tests of the ATK thread
sed to invoke some very simple tests of the ATK thread
HThreadTest is used to invoke some very simple tests of the ATK thread
Final sum = 0.500000 (should be 1)

> HThreadTest 2 4 1
 creating 4 threads
   1 created
   2 created
HThreadTest is used to invoke some very simple tests of the ATK thread
HThreadTest i   3 created
   4 created
 waiting for termination
s used to invoke some very simple tests of the ATK thread
HThreadTest is used to invoke some very simple tests of the ATK thread
HThreadTest is used to invoke some very simple tests of the ATK thread
Final sum = 1.000000 (should be 1)

3.  SimpleSignal()

Invoke  as

    HThreadTest 3

This creates a thread which waits for a signal.  The main thread waits
for the return key to be pressed, then sends the signal.
Example:
> HThreadTest 3
 press key to send signal
Test 3 - waiting for Signal
(press return key)
Test 3 - Signal Received

4.  Bounded BufferTest

Invoke as

    HThreadTest 4 nChars bSize pDelay cDelay pcPrioStr

This is the most complex test.  It implements a simple
producer-consumer task in which a consumer puts nChars characters into
a fixed size buffer of bSize chars.  When the producer puts char A
into the buffer it prints out

 <A  immediately
  !  if buffer is full
   > operation is completed

Similarly the consumer prints out

 {   immediately on trying to receive a char
  ?  if buffer is empty
  A} when A is received

pDelay and cDelay add delays into the producer and consumer and
thereby affect the relative speed.  This changes the flow thru the
buffer.  Similarly, the priorities of the tasks can be changed by the
final pcPrioStr.  This consists of two chars, one for producer and one
for consumer.  There values should be n, l or h for normal, low, high,
eg.  nn sets both normal, lh set producer low and consumer high.

Examples
> HThreadTest 4 10 5 0 0 nn
Starting Buffer Test

 [[Producer started (n=10)]  [[Consumer started]] <A>{A}<B>{B}<C>{C}
<D>{D}<E>{E}<F>{F}<G>{G}<H>{H}<I>{I}<J>{J}< >{ }

Producer terminated[11 chars sent]
Consumer terminated[11 chars received]

> HThreadTest 4 10 5 10 0 nn
Starting Buffer Test

 [[Producer started (n=10)]  [[Consumer started]] {?<AA}>{?<BB}>
{?<CC}>{?<DD}>{?<EE}>{?<FF}>{?<GG}>{?<HH}>{?<II}>{?<JJ}>{?<  }>

Producer terminated[11 chars sent]
Consumer terminated[11 chars received]

> HThreadTest 4 10 5 0 0 hl
Starting Buffer Test

 [[Producer started (n=10)] <A><B><C><D><E><F! [[Consumer started]] 
 {><G!A}{><H!B}{><I!C}{><J!D}{><!E}{>F}{G}{H}{I}{J}{ }

Producer terminated[11 chars sent]
Consumer terminated[11 chars received]

In the first case the flow is smooth and there is no waiting.  In the
second case, the producer is slowed down and the consumer is always
waiting for a character.  In the third case, the consumer is slow and
the buffer fills.
