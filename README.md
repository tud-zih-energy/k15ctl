k15ctl
======
AMD Family 15h (aka Bulldozer) P-State, frequency and voltage modification utility


0. Warning
==========
USE THIS PROGRAM AT YOU OWN RISK. IT MAY DAMAGE YOUR HARDWARE.

If you are unsure about the result, always run with -dry-run first.


1. What the program can do
==========================
- It is intended for AMD Family 15h (aka Bulldozer)  CPU's for Socket AM3+ and FM2
- It runs under Linux
- It can modify the P-States, i.e. alter processor and the northbridge frequencies and voltages;
- set / change the P-State;
- read out frequency and voltage information


2. Requirements
===============
- A Linux kernel with MSR support (Processor type and features --> 
  /dev/cpu/*/msr - Model-specific register support) 
- msr module loaded
- root permissions


3. Usage
========
k15ctl -h                                              displays help
k15ctl -cpu <cpu>[-<cpun>]                             Print a lot of information about CPU(s) <cpu>(-<cpun>)
k15ctl -cpu <cpu>[-<cpun>] -p <p-state> [parameters]   Configure the P-State for CPU(s) <cpu>(-<cpun>), see below
k15ctl -cpu <cpu>[-<cpun>] -b <bp-state> [parameters]  Configure the Boosted P-State for CPU(s) <cpu>(-<cpun>), see below
k15ctl -nb <nb>[-<nbn>] -np <np-state> [parameters]    Configure the P-State for Northbridge(s) <nb>(-<nbn>), see below

Global Parameter:
  -dry-run         Shows the resulting P-State table without modifying any register.

Parameters for the P-State configuration:
  -cf <CpuFid>     CPU Frequency ID
  -cd <CpuDid>     CPU Divisor ID
  -cv <CpuVid>     CPU Voltage ID
  -cnp <NbPstate>  Associated Northbridge P-State
  -pd <mW>         Power dissipation in mW
  -en <Enable>     Enable or Disable P-State

Parameters for the Northbridge P-State configuration:
  -nf <NbFid>      Northbridge Frequency ID
  -nd <NbDid>      Northbridge Divisor ID
  -nv <NbVid>      Northbridge Voltage ID
  -nen <Enable>    Enable or Disable Northbridge P-State


4. Examples
===========
k15ctl -cpu 0-3                                         Print a lot of information about cores 0-3
k15ctl -cpu 0-3 -p 0 -nv 46 -cv 22 -cd 0 -cf 16         Set NbVid=46, CpuVid=22, CpuDiD=0, CpuFid=16 for CPUs 0-3 and P-State 0
k15ctl -cpu 0-3 -b 1 -cf 22 -nb 0 -np 0 -nf 22 -dry-run 
    Shows the resulting table for boosted P-State 0 on CPUs 0-3 with CpuFid=22 and Northbridge P-State 0 on Northbridge 0 with NbFid=22
