# About

** This is from Thu 10 Jun,1999 **


This page is about a device that plays mp3 files from a CDROM. It's a transformed Intel-based PC running Linux and rxcontrol, with an LCD and keypad. I made this device in 1998 for my own pleasure. Please consider that since then many commercial devices have hit the market, and this may render now the construction of such a device less interesting, but back then this wasn't the case. Nevertheless you may profit from my efforts into building something that really satisfies your needs.
The name is pronounced MP3-O-PHONO. PHONI is the greek for VOICE

I'm giving infomation here for people wishing to build their own devices. I'm not willing to build mp3 players for anyone cause I have no time :-) but I would be happy to answer questions about my construction. You can contact me by email at sivann at softlab.ece.ntua dot gr
Please don't ask me questions about learning/installing linux, hardware prices, cpu performance, paint quality e.t.c. Please read these pages and the rxcontrol's documentation careful before asking questions.
Please consider that you must already be good at linux/unix before attempting to construct this!
Thank you, and good luck.

Spiros Ioannou

## Features
* Menu driven interface leads to only 9 front-panel keys
* LCD green backlit, 20 columns x 4 lines
* Supports standard functions such as play,stop,pause,FF,FREV,next,prev, go to first
* FF (Fast forward) and FREV have adjustable length and time intervals
* File selection modes: select current, select all, select randomly
* Playlist support: saves playlists to HD.
* Playlists are editable and can be re-ordered
* File selection from HD and CDROM (automounted)
* While browsing the filesystem to add files, the display is fully scrollable. Only mp3 files and directories are shown.
* Directories can be added to a playlist recursively
* Hot key to upgrade software from diskette
* Infra Red (IR) support implemented to both play functions & menu navigation
* Repeat toggle button
* Mute toggle button
* Optional volume control (+/-) buttons
* LCD Displays time left, time elapsed & time progress bar
* Song title & artist are guessed from filename or read from the ID3 tag if the latter exists.
* Title and artist autoscroll left<->right if they don't fit on the display
* Scroll interval and step are configurable
* Welcome message at startup
* All keypad and IR keys are configurable
* Written totally in C so it's fast, robust, and memory efficient
* LCD routines written in C for fast and flicker-free lcd updates
* Not intended for the linux newbie!
* total cost 50$ (for the lcd) if you have the spare parts


## News
* xaudio version 1.0.5 seems to handle Xing's VBR correctly :-)
* Scanned 2 articles from a greek and an english mag about mp3ophono!
* Version 1.7 and 1.8b are out. Check the changelog at the DOWNLOAD section
* This site has been slashdotted (https://hardware.slashdot.org/story/99/04/22/0150217/do-it-yourself-mp3-stereo)!   Have a look at the hits/min after the publish
* X-axis is time. (made with gnuplot)

## Press
* You may find press coverage here


# HARDWARE
The hardware is actually an intel-based PC. It uses a P100 with 16MB ram. Don't ask me if a 486 would do the job or if the HD is too large, I just had these parts so I used them ;-) I believe that a 486 would do the job, and the HD isn't needed, a floppy would be just fine.
## Output
Instead of a monitor it has a 4line/20chars LCD (LCD2041) Display from Matrix Orbital. I bought it from linuxcentral. It arrived here in greece in 10 days, no problem with customs. It's expensive, but it has an excelent viewing angle and you don't have to program a PIC. You can find several C routines for handling this LCD in the software section. I mounted the LCD on the 3.5inch bay using a drill and imagination. It took 2 bays.
## Input
For the keyboard, instead of using some parallel/serial keypad I chose to dismantle an old keyboard I had and use its key decoder chip, so as you can see in the photos, I just connected some switches in the places where the keyboard keys would normally be and mounted them to the front. I didn't solder the cables directly to the motherboard so that I can easily disconnect the front keys and connect a normal keyboard (for upgrading linux). More info can be found here. I've also installed a floppy to automatically update the software from the rxcontrol with a touch of a button (from version 1.1).
Infra red support is here thanks to Bryan Mattern.
## Media
Linux boots from a HD though it could boot from a flash disk. The mp3 files are on cdrom which plays for 10 hours constantly @128Kbps. When DVD is out it will have 18.5 days of music/disk.
## Case
The case comes from an old PC but I painted it with black-satin spray. The height is 13cm,which is the shortest case I could find. It fits perfectly among my other stereo equipement. The front panel with the keys and the display needs a heavy redesign though.
## Noise
I extinguished the noise from the Power Supply fan with a simple variable resistor in series with the fan. I can adjust the fan speed that way to something not audible. The only thing that can be heard right now is the cpu fan which is not a problem. The whole thing does not overheat even here in Greece :-)
## Summary
* pc case, power supply (plus fan trim resistor)
* motherboard with onboard IDE and serial/parallel ports
* Pentium 100 MHz, 16 MB RAM, CPU fan
* graphics card (not required,but why not?)
* sound card PCI 128
* CD-ROM drive 4x, completely silent
* hard disk unit 300MB SCSI from an old Apple (!)
* Future Domain SCSI controller (1992)
* floppy disk drive for software updates

Special parts:
* serial LCD panel from MatrixOrbital working at 19200bps
* customized keyboard

# SOFTWARE

The software used by Mp3-O-Phono is called rxcontrol. I wrote this software, and what it does is that it handles input from keyboard, output to LCD, input + output from the rxaudio mp3 decoder and some basic cd tray functions. It also supports input from an Infra Red (IR) device such as a tv remote.
Operating System is Linux with 2.0.35 kernel. It currently needs a shutdown because the HD gets mounted read-write in order to be able to save the playlists. If the HD were to be mounted read-only, then no shutdown would be required.

Some info about keyboard input
To bypass the login prompt I used the following solution: I'm talking to a virtual console. Rxcontrol, changes the current virtual console with the chvt() function (there is a chvt program that does the same). I remind you that virtual consoles are the ones changed with Alt-F1 Alt-F2 e.t.c. Rxcontrol uses virtual console 5 by default so a getty shouldn't be running there. You must comment it out in /etc/inittab. That way when your software starts, it changes the virtual console let's say to 5 so it gets the keyboard input. That simple.

```
rxcontrol version 1.6
----------------------

(C) Spiros Ioannou 1998

Send bugs(please)/comments to: sivann@cs.ntua.gr
See 'CHANGELOG' for changes between versions

Visit: http://www.softlab.ece.ntua.gr/~sivann/rxcontrol for more info.

I'm proud to declare that the above url has been on slashdot! (slashdot.org)

About this software
-------------------
This software is used with my "MP3-O-Phono" (C) hardware, a linux based
player, without monitor or keyboard. It has a Matrix Orbital 20x4 LCD
and 9 buttons at the front. It boots from a small HD, and has a CDROM
for reading MP3 songs. The buttons are connected to the keyboard socket
of the motherboard through a chip taken by a dissasembled keyboard, so 
the input comes from stdin! This is much cheaper in money and time than
connecting a serial or parallel keypad,and it can support up to 108 keys.

COMPILING
---------
See 'COMPILE' for compiling instructions

INSTALLING
----------
See 'INSTALL' for installation procedures.

OPTIONS
-------
Running the program with '-h' gives a brief description of command-line options.

Song name format
----------------
Song must be in the following format:
"artist - song name.mp3"
or
"artist - song name - other info.mp3" 

`other info' is not displayed on the LCD.

The spaces before and after the '-' are optional.


General Operation
-----------------
There are 3 states:

	-normal
	-file selection
	-playlist editing

when the program starts and when it plays a song it is in the normal state 
For the hardware version, keyboard keys will be connected to buttons.

All keys are the default keys defined in rxkeys.h

*
* For the hardware implementation I have done, only keys marked with '*' are
* needed and used. Capital letters are implemented through a button <shift>
* so you press a combination of 2 buttons. It has the label 'All' which means
* for example that if 'v' (the stop button) erases the current entry, 'V' 
* erases all the entries, e.t.c.
*

Commands  in normal mode
------------------------
Just type in the corresponding letter



    f: enter file selection mode
    p: enter playlist editing mode
    l: browse through pre-compiled playlists
   *u: load a menu capable of loading the previous 3 functions
       plus a linux shutdown option
    U: Upgrades the binary from floppy. File locations defined in
       rxcontrol.h

    Playing commands

    r: toggle repeat flag
    c: pause
   *x: play/pause
   *v: stop
   *[: seek left / Scroll LCD left
   *]: seek right / Scroll LCD right
   *z: previous track / Go up in list browsing
   *b: next track / Go down in list browsing
    right arrow: seek right
    left arrow:  seek left

    Other commands

    m: mute (don't press it, dont' know how to de-mute !)
    ,: volume down (or arrow up)
    .: volume up (or arrow down)
    q: quit program



Commands  in file selection mode
--------------------------------
Just type in the corresponding letter

   *]: scroll display right
   *[: scroll display left
   *z: go up
   *b: go down
   *x: select current file
   *r: make a random selection from all files in this directory
    X: select all files in this directory
    R: select all files from this directory and below 
    V: clear playlist
   *u: go back to normal mode



Commands  in playlist editing mode
----------------------------------
Just type in the corresponding letter


   *]: scroll display right
   *[: scroll display left
   *z: go up
   *b: go down
   *v: remove current entry from playlist
   *V: clear playlist
   *X: save playlist (to #PLDIR)
   *r: randomly reorder the playlist
   *u: go back to normal mode



Commands  in playlist editing mode
----------------------------------
Just type in the corresponding letter

   v: delete playlist from disk
   <enter>: load current playlist



Also NOTE:
----------
1.after pressing one of : stop/edit-playlist/file-browser , the playlist doesn't
  restart from file #1, but only if stop pressed twice (i.e. pressed when 
  already stopped) so you can continue listening from the current song by
  pressing "play" and not from the beggining.

2.playlists are saved as playlist-## where ## is a 2-digit number, in directory
  pldir defined in rxcontrol.conf


BUGS
----

None found before each release, but there are bugs. If you find bugs any please 
mail them to sivann@cs.ntua.gr. 
The 'none found' goes to rxcontrol, NOT to rxaudio which has many bugs causing 
rxcontrol to fail sometimes. I've never seen any fatal ones of them to my PC 
but the've been reported to me. Mail those to xaudio@mpegtv.com please. 


CREDITS TO
----------
-All the people who expressed themselves positively about MP3-O-Phono
-Bryan Mattern for the donnated IR hardwre. Thanks Bryan, I'd probably delay
 IR support too long without your help.
-Aaron Newsome for the bug reports.


```
