k15ctl
======
AMD Family 15h (aka Bulldozer) P-State, frequency and voltage modification utility


0. Warning
----------
USE THIS PROGRAM AT YOU OWN RISK. IT MAY DAMAGE YOUR HARDWARE.

If you are unsure about the result, always run with -dry-run first.


1. What the program can do
--------------------------
- It is intended for AMD Family 15h (aka Bulldozer)  CPU's for Socket AM3+ and FM2
- It runs under Linux
- It can modify the P-States, i.e. alter processor and the northbridge frequencies and voltages;
- set / change the P-State;
- read out frequency and voltage information


2. Requirements
---------------
- A Linux kernel with MSR support (Processor type and features --> 
  /dev/cpu/*/msr - Model-specific register support) 
- msr module loaded
- root permissions
