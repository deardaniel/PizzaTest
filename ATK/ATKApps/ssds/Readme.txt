SSDS
-----

This program demonstrates basic use of ATK in a simple spoken dialog system.  
To run it, cd into the Test directory and (assuming SSDS.exe is in your path)
type

SSDS -C ssds.cfg

Some things to note

a) you can run this in 'hands-free' mode or 'click-to-talk' mode.
   Set 
	AREC: RUNMODE = 01441   -- hands free
	AREC: RUNMODE = 01221   -- click to talk

   in the 'config file'
   Note that even in handsfree mode, you have to click once to start
   the audio sampling.

b) the dialog is a simple pizza ordering application.  A sample dialogue
   might be

     What topping would you like?
     Ham and Cheese
     Are sure you want topping(PEPPERONI) ?
     No, I want Ham and Cheese
     Are sure you want topping(HAM AND CHEESE) ?
     Yes
     How many pizzas would you like?
     Three please.
     Are sure you want quantity(THREE) ?
     Yes
     Your order is topping(HAM AND CHEESE), quantity(THREE)

  in the above the correction grammars are created automatically
  from the ask-question grammars by prepending a "No" and placing
  a "Yes" in parallel.

c) the file global.net holds dialog subnets used at every step.  Topping.net
   and howmany.net are used for the specific questions.   The global.net main
   network is placed in parallel with topping.net and howmany.net.  It contains
   several subnets which act as a library for the other two networks (An ATK 
   grammar can share its subnetworks with other grammars)

