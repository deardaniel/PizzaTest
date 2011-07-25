TBASE

This is a simple test program which uses only the core ATK classes (ie
APacket, ABuffer and AComponent).  It is invoked as

   TBase -C TBase.cfg

When executed it displays 3 boxes called a, b and c holding red balls,
when you click on a box, one ball is moved to the next ball in the
sequence.  You can also type a command into the monitor command box to
create n new balls ("new(n)") or pass n balls ("pass(n)").  Each box
is in instance of ABalls which is a class derived from AComponent.
The tasks are connected by buffers in a round-robin fashion and each
ball is passed as a packet via the buffers. It is configured using the
file TBase.cfg
