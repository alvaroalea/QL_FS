Even though I never managed it in real life, I have wanted to be a pilot all my life, ever since, at 13-14, someone gave me a copy of Microsoft Flight Simulator 3.0, and I was blown away by the Microhobby article explaining how to fly on instruments in Fighter Pilot.
There are a couple of simulators for the QL, but they either cannot be distributed or give configuration problems, ROMs and so on, and the lack of the original documentation is a problem in this kind of game.

So, with the help of AI, I have tried to solve all these problems. It is a program that is too big for an original QL: the first version printed one frame every 17 seconds, and after optimizing and optimizing it is now almost playable, although the ideal is a Super Gold Card, a QIMSI Gold, a Next or one of the fantasy QLs out there.

Anyway, the result is a flight simulator with a quite complete Iberian Peninsula, capable of VFR and IFR flight, with weather conditions, ATC, several aircraft models... and a complete manual of almost 200 pages with checklists, all the flight charts, route maps, SID, approaches... etc. to make flying on a QL as close to the real thing as possible.

Alea's Flight Simulator (AFS) in a nutshell:

- Inspired by Microsoft Flight Simulator 1.0 from 1982: instrument cockpit, 3D exterior view and top-down map.
- Written in C (c68/XTC68) with 68000 assembler in the critical parts.
- Four aircraft with their own physics: glider, 160 HP single-engine, twin 2 x 300 HP with variable-pitch propellers and retractable gear, and an executive jet.
- Iberian Peninsula and the Canary Islands at 1:2 scale: 14 airports with runways, taxiways, PAPI and lights, about 20 VOR/DME and 9 NDB, cities, rivers, motorways and the main mountain ranges. The data is approximate: it is not good for real navigation.
- Radio navigation: COM1, NAV1, NAV2, ADF, DME, transponder, ILS, VOR with OBS, NDB and marker beacons.
- Air traffic control in English: ATIS, tower, clearances and radar contact.
- Full panel in mode 4 (512x256), cockpit, chase, tower and side views, day, dusk and night with runway and city lights.
- Stall, flaps, mixture, engine failure, structural failure, crashes, save and load game.
- Sound with QSound 2+ (optional).

Requirements: QL with 640 KB, 720 KB floppy or win1_ disk (the scenery takes 450 KB), or an emulator (sQLux, QPC2, QXL...). To run smoothly you need at least the speed of a QL x8 (Super Gold Card); on a 7.5 MHz QL it works, but very slowly (the panel and the view refresh once or twice per second).

Warning: the program was written by an AI, so there may be bugs. It is not based on real life and, of course, it will not teach you to fly.

Source code, documentation and screenshots: https://github.com/alvaroalea/QL_FS
Download (floppy image, win1_ disk and PDF manual): https://github.com/alvaroalea/QL_FS/releases
