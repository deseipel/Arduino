Arduino MIDI Master Clock/SD MIDI File Player-Looper/MIDI Mapper
-----------------------------------------------------------------------------------------
This project allows one to play and loop midi files/songs to a hardware synth or Midi 
device, while sending MIDI clock sync.  In addition, it allows for the mapping of midi events to 
other midi events.  In the current configuration, it maps certain notes to program changes, 
stop/start and specific NRPNs.  




Required Hardware:
------------------------------------------------------------------------------------------
-MIDI Device to control
-MIDI events you wish to map
-Arduino Due
-MIDI shield (I used Ruggeduino MIDI shield, which as of this writing has been discontinued )
-Ada Fruit TFT/SD shield
-Micro SD card
-USB cable 
-Power supply (if not using the USB cable)
-Jumper Cables for the Arduino (not car jumper cables).
-Solder Gun
-Solder


Required Software:
--------------------------------------------------------------------------------------------
-Arduino IDE 1.57
-MIDIFile Library


Reccommended Software (for debugging)
---------------------------------------------------------------------------------------------
-MIDIOX
-Hex Editor
-MF2T




configuration instructions:

turn MIDI shield into 3.3v compatible
Due SPI is the 6 pin header next to the cpu, use that. (HDW serial)








installation instructions:


operating instructions:



a file manifest
copyright and licensing information
contact information for the distributor or programmer
known bugs
troubleshooting
credits and acknowledgements
a changelog
