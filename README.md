# NeonTime

ESP8266 which syncronizes the NeonTime 2D3 clock using Network Time Protocol (NTP).

## Background

This clock is no longer made and the website has been taken offline although can view parts of it
via [web.archive.org](https://web.archive.org/web/20140516190414/http://neontime.co.uk/)

There was 3.5mm jack -> serial cable that was available and some windows software which would sync
the clock to NTP. Sadly no source code was supplied so reverse engineering of the serial protocol
was necessary.

Rather than having to connect it to a PC a ESP8266 with an RS-232 breakout board is used.
Since the 3.5mm jack outputs 12V we can use that to power it via a step-down voltage regulator.
