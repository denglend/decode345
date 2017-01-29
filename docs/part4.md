---
---
# What's Next

{:toc}

### Issues
The combination of gnuradio script and the python decoder consumes about 25% of one core of a (few years old) laptop.  That's not too bad, but I don't want to keep my laptop running 24/7, and I found the usage was a bit too high to run comfortable on the computer I do leave running 24/7: a Raspberry Pi 3 Model B.  It certianly doesn't help that I'm also running other code (e.g. OpenHAB) on the pi simultaneously.  The limiting factor seemed to be the decoder script's ability to keep up with the stream of data from the radio.

### Possible solutions
1. Optimize the code enough that it can run comfortably on a Rasperry Pi.
2. Use an embedded receiver isntead of SDR.

#### Optimize the Code
Sometime in the interim between finishing 90% of this writeup and actually publishing it, I decided to try to solve the optimization problem by rewriting all the python code in C.  Somewhat surprisingly, it worked: The C version of the code consumes about 1% of a pi core.  The SDR script uses about 40% of a core.  The combination is sufficiently light that I can keep it running constantly.  Having learned a few lessons from Version 1 in python, the C code is also more flexible than the python script, supporting a number of command line options rather than requiring the code be edited to change the host, port, device IDs, etc.

The C code is available in the [GitHub repository](https://github.com/denglend/decode345).  The logic is almost identical to the python code, but much speedier, and probably less robust.  I spent almost no time testing corner cases.

#### Embedded receiver
A different option would be to move the radio tuning off the PC altogether, and use a separate microcontroller to receive the signal.  A [Texas Instruments CC1101 ](http://www.ti.com/product/CC1101) seems like a good candidate, since one of its supported bands is 300 - 348 MHz.  Most of the breakout boards advertised for sale, specifically list 433 MHz as their operating frequently, however.  I'm not sure if that's just because 433 MHz is far and away the most popular frequency, or because the boards themselves only support 433, despite the CC1101 chip having a wider range.