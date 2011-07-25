TREC

This test program implements a basic ATK grammar-based recogniser.  
The domain is simple digit dialling and the input language consists
of  (CALL|PHONE|DIAL) digit digit .....

Invoke the recogniser by

   TRec -C TRec.cfg

Press start on the audio input to start recognising and push again to stop.
The recogniser can also be run in continuous mode by changing the RUNMODE
parameter in the config file to the commented out option.

