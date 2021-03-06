/*
  MIDIFile.cpp - An Arduino library for processing Standard MIDI Files (SMF).
  Copyright (C) 2012 Marco Colli
  All rights reserved.

  See MIDIFile.h for complete comments

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <string.h>
#include "MIDIFile.h"
#include "MIDIHelper.h"

////////////////////////////////////////
// MIDIFile Class
//

// Events may be processed in 2 different ways. One way is to give priority to all 
// events on one track before moving on to the next TRACK (TRACK_PRIORITY) or to process 
// one event from each track and cycling through all tracks roound robin fashion until 
// no events are left to be processed (EVENT_PRIORITY). The define below allows changing 
// the mode of operation implemented in getNextEvent().
//
#define	 TRACK_PRIORITY 1

void MIDIFile::initialise(void)
{
  _trackCount = 0;            // # of tracks in file
  _format = 0;
  _lastEventCheckTime = 0;
  _syncAtStart = false;
  _paused = false;
  _tick = 0;
  _Tcount = 0;
  _etime = 0;
  _delta = 0; 
  _ddelta = 0;
  _tfd = 0;
  dcount=0;
  firstdelta = true;
  setMidiHandler(NULL);
  setSysexHandler(NULL);
  total_track_deltas = 0;
  backupcount=0;

  // File handling
  setFilename("");
  _sd = NULL;

  // Set MIDI defaults
  setTicksPerQuarterNote(96);	  // 24 ticks per quarter note
  setTempo(120);				  // 120 beats per minute
  setTimeSignature(4, 4);		  // 4/4 time
}

void MIDIFile::synchTracks(void)
{
	for (uint8_t i=0; i<_trackCount; i++)
		_track[i].syncTime();
	_lastEventCheckTime = micros();
	
}

MIDIFile::MIDIFile(void) 
{ 
	initialise();
}

MIDIFile::~MIDIFile() 
{ 
  close();
}

void MIDIFile::begin(SdFat *psd)
{
	_sd = psd;
}

void MIDIFile::close()
// Close out - should be ready for the next file
{
  for (uint8_t i = 0; i<_trackCount; i++)
  {
	  _track[i].close();
  }
  _trackCount = 0;
  _syncAtStart = false;
  _paused = false;
 // _tick = 0;
 _etime = 0;
  setFilename("");
  _fd.close();
  //_Tcount = 0;
  _delta = 0;
  _ddelta =0;
}

void MIDIFile::setFilename(const char* aname) 
// sets the filename of the MIDI file.
// expects it to be in 8.3 format.
{
	if (aname != NULL)
		strcpy(_fileName, aname);
}

const char* MIDIFile::getFilename(void) 
// returns the name of the current file
{
  return _fileName;
}

uint8_t MIDIFile::getTrackCount(void) 
// return the number of tracks in the Midi File.
{ 
  return _trackCount;
}



uint8_t MIDIFile::getFormat(void) 
// return the format of the MIDI File.
{ 
  return _format;
}

uint16_t MIDIFile::getTempo(void)
{
  return _tempo;
}

void MIDIFile::setTempo(uint16_t t)
{
  _tempo = t;
  calcMicrosecondDelta();
}

uint16_t MIDIFile::getTimeSignature(void)
{
  return ((_timeSignature[0]<<8) + _timeSignature[1]);
}

void MIDIFile::setTimeSignature(uint8_t n, uint8_t d)
{
  _timeSignature[0] = n;
  _timeSignature[1] = d;
  calcMicrosecondDelta();
}

void MIDIFile::setTicksPerQuarterNote(uint16_t ticks) 
{
  _ticksPerQuarterNote = ticks;
  calcMicrosecondDelta();
}

uint16_t MIDIFile::getTicksPerQuarterNote(void) 
{ 
  return _ticksPerQuarterNote;
}

void MIDIFile::calcMicrosecondDelta(void) 
// 1 tick = microseconds per beat / ticks per Q note
// The variable "microseconds per beat" is specified by a MIDI event carrying 
// the set tempo meta message. If it is not specified then it is 500,000 microseconds 
// by default, which is equivalent to 120 beats per minute. 
// If the MIDI time division is 60 ticks per beat and if the microseconds per beat 
// is 500,000, then 1 tick = 500,000 / 60 = 8333.33 microseconds.
{
    if ((_tempo != 0) && (_ticksPerQuarterNote != 0))
	{
		_microsecondDelta = (60 * 1000000L) / _tempo;	// microseconds per beat
		_microsecondDelta = _microsecondDelta / _ticksPerQuarterNote;	// microseconds per tick
	}
}

void MIDIFile::setMicrosecondPerQuarterNote(uint32_t m)
// This is the value given in the META message setting tempo
{
	_microsecondDelta = m / _ticksPerQuarterNote;

	// work out the tempo from the delta by reversing the calcs in calcMicrosecondsDelta
	// m is alreay per quarter note
	_tempo = (60 * 1000000L) / m;
}

uint32_t MIDIFile::getMicrosecondDelta(void) 
{
	return _microsecondDelta;
}

void MIDIFile::setMidiHandler(void (*mh)(midi_event *pev))
{
	_midiHandler = mh;
}

void MIDIFile::setSysexHandler(void (*sh)(sysex_event *pev))
{
	_sysexHandler = sh;
}

bool MIDIFile::isEOF(void)
{
	for (uint8_t i=0; i<_trackCount; i++)
	 {
		 if (! _track[i].getEndOfTrack()) return false;  // breaks at first false
	 }
	 return true;
}
void 	 MIDIFile::setDcount(uint32_t dcnt)
{
 dcount = dcnt;
}

uint32_t MIDIFile::getDcount(void)
{
return dcount;
}


void MIDIFile::pause(bool bMode)
// Start pause when true and restart when false
{
	_paused = bMode;

	if (!_paused)				// restarting so ..
		_syncAtStart = false;	// .. force a time resynch when next processing events
}

void MIDIFile::restart(void)
// Reset the file to the start of all tracks
{
	for (uint8_t i=0; i<_trackCount; i++)
		_track[i].restart();

	_syncAtStart = false;		// force a time resych
}

void MIDIFile::setTick(byte tick)
{
   _tick = tick; 
}

void MIDIFile::setDelta(uint32_t delta)
{
_delta = delta;
}

void MIDIFile::resetDelta(uint32_t ddelta)
{  
  _ddelta=ddelta;
}

uint32_t MIDIFile::getDelta()
{
return _delta;
}

byte MIDIFile::getTick()
{
return _tick;
}

void MIDIFile::setTickCount(long long Tcount)
{
 _Tcount = Tcount;
}

long long 	MIDIFile::getTickCount()
{  
    return _Tcount;
}

void MIDIFile::setTrackFirstDelta(uint32_t tfd)
{
_tfd = tfd;
}

uint32_t MIDIFile::getTrackFirstDelta(void)
{
return _tfd;
}


void MIDIFile::setLastTime(unsigned long etime)
{
_etime = etime;
}

unsigned long MIDIFile::getLastTime()
{
return _etime;
}

bool MIDIFile::Play(void)
{
long unsigned  elapsedTime;
uint8_t track_count = getTrackCount();
	uint8_t		n;	
int bpm = getTempo();	
	
	// if we are paused we are paused!
	if (_paused) return false;
	
	// sync start all the tracks if we need to
	if (!_syncAtStart)
	{
		synchTracks();
		_syncAtStart = true;
	}	
	_tick = getTick();	
	
	elapsedTime = (micros() - _lastEventCheckTime);
	//elapsedTime = (getLastTime() - _lastEventCheckTime);
	//  IF THE TIMER'S FREQUENCY IS SET TO 96PPQ THEN EACH TIME PLAY IS CALLED, IT SHOULD NEVER BE TOO SOON, AND THEREFORE NO REASON TO CHECK IT? 
	// TURNING THE TICK CHECK OFF.  -- NO, this check appears to improve the loop timing somehow. (tick is on the downbeat probably).
	//  need to find another way, perhaps.  might end up using the getLastTime() after all.
	
	//if (_tick ==0) return false;  //tick is set in the midi timer, during the time when MIDI clock sync is set. 
 if (elapsedTime < (((60 * 1000000L) / bpm)/96))
		return false; 
	_lastEventCheckTime = micros();	
//	_lastEventCheckTime= getLastTime();
	
	//if (_format != 0) DUMPS("\n-- TRK "); 
	
	//begin track priority
#if TRACK_PRIORITY	
	// process all events from each track first - TRACK PRIORITY
	//for (uint8_t i = 0; i < track_count; i++)
	for (uint8_t i =0; i < track_count; i++)
	{    
	
		//if ( dcount<track_count)dcount++;
		//firstdelta=true;
		
		//if (_format != 0) DUMPX(i);
		// Limit n to be a sensible number of events in the loop counter
		// When there are no more events, just break out
		// Other than the first event, the other have an elapsed time of 0 (ie occur simultaneously)
		for (n=0; n < 100; n++)
		{
		//if ( dcount < n )dcount++;
			if (!_track[i].PlayTracks(this))
		//	if (!_track[i].PlayTracks(this, (n==0 ? elapsedTime : 0)));     //
			
				break;
			
		}
			/* setDelta(getTrackFirstDelta());
		setTickCount(backupcount);
		firstdelta =false; */
		//close();
    }   

	//setDcount(dcount);
	
//setTickCount(0); 

#else // EVENT_PRIORITY
	// process one event from each track round-robin style - EVENT PRIORITY
	bool doneEvents1;

	// Limit n to be a sensible number of events in the loop counter
	for (n = 0; (n < 100) && (!doneEvents1); n++)
	{
		doneEvents1 = false;

		for (uint8_t i = 0; i < _trackCount; i++)	// cycle through all
		{
			bool	bb;

			if (_format != 0) DUMPX(i);
			// Other than the first event, the other have an elapsed time of 0 (ie occur simultaneously)
			bb = _track[i].PlayTracks(this);
			//if ((bb) && (_format != 0)) DUMPS("\n-- TRK "); 
			doneEvents1 |= bb;   //won't compile.
			//doneEvents1= (doneEvents1 || bb);
		}

		// When there are no more events, just break out
		if (!doneEvents1)
			break;
	} 
#endif // EVENT/TRACK_PRIORITY
return true;

}



/* 

boolean MIDIFile::getNextEvent(void)
{
	uint32_t	elapsedTime;
	uint8_t		n;

	// if we are paused we are paused!
	if (_paused) return false;

	// sync start all the tracks if we need to
	if (!_syncAtStart)
	{
		synchTracks();
		_syncAtStart = true;
	}

	// check if enough time has passed for a MIDI tick
	elapsedTime = micros() - _lastEventCheckTime;
	if (elapsedTime < _microsecondDelta)
		return false;	
	_lastEventCheckTime = micros();			// save for next round of checks

	if (_format != 0) DUMPS("\n-- TRK "); 

#if TRACK_PRIORITY
	// process all events from each track first - TRACK PRIORITY
	for (uint8_t i = 0; i < _trackCount; i++)
	{
		if (_format != 0) DUMPX(i);
		// Limit n to be a sensible number of events in the loop counter
		// When there are no more events, just break out
		// Other than the first event, the other have an elapsed time of 0 (ie occur simultaneously)
		for (n=0; n < 100; n++)
		{
			if (!_track[i].getNextEvent(this, (n==0 ? elapsedTime : 0)))
				break;
		}

		if ((n > 0) && (_format != 0))
			DUMPS("\n-- TRK "); 
	}
#else // EVENT_PRIORITY
	// process one event from each track round-robin style - EVENT PRIORITY
	bool doneEvents;

	// Limit n to be a sensible number of events in the loop counter
	for (n = 0; (n < 100) && (!doneEvents); n++)
	{
		doneEvents = false;

		for (uint8_t i = 0; i < _trackCount; i++)	// cycle through all
		{
			bool	b ;

			if (_format != 0) DUMPX(i);
			// Other than the first event, the other have an elapsed time of 0 (ie occur simultaneously)
			b = _track[i].getNextEvent(this, (n==0 ? elapsedTime : 0));
			if (b && (_format != 0))
				DUMPS("\n-- TRK "); 
			//doneEvents ||= b;
			doneEvents = (doneEvents || b);
		}

		// When there are no more events, just break out
		if (!doneEvents)
			break;
	} 
#endif // EVENT/TRACK_PRIORITY

	return true;
} */

int MIDIFile::load() 
// load the MIDI file into memory ready for processing
{
  uint32_t  dat32;
  uint16_t  dat16;
  
  if (_fileName[0] == '\0')  
    return(0);

  // open the file for reading:
  if (!_fd.open(_fileName, O_READ)) 
    return(2);

  // Read the MIDI header
  // header chunk = "MThd" + <header_length:4> + <format:2> + <num_tracks:2> + <time_division:2>
  {
    #define HDR_SIZE  4
    char    h[HDR_SIZE+1]; // Header characters + nul
  
    _fd.fgets(h, HDR_SIZE+1);
    h[HDR_SIZE] = '\0';
    
    if (strcmp(h, "MThd") != 0)
	{
	  _fd.close();
      return(3);
	}
  }
  
  // read header size
  dat32 = readMultiByte(&_fd, MB_LONG);
  if (dat32 != 6)   // must be 6 for this header
  {
	_fd.close();
    return(4);
  }
  
  // read file type
  dat16 = readMultiByte(&_fd, MB_WORD);
  if ((dat16 != 0) && (dat16 != 1))
  {
	_fd.close();
    return(5);
  }
  _format = dat16;
 
   // read number of tracks
  dat16 = readMultiByte(&_fd, MB_WORD);
  if ((_format == 0) && (dat16 != 1)) 
  {
	_fd.close();
    return(6);
  }
  if (dat16 > MIDI_MAX_TRACKS)
  {
	_fd.close();
	return(7);
  }
  _trackCount = dat16;

   // read ticks per quarter note
  dat16 = readMultiByte(&_fd, MB_WORD);
  if (dat16 & 0x8000) // top bit set is SMTE format
  {
      int framespersecond = (dat16 >> 8) & 0x00ff;
      int resolution      = dat16 & 0x00ff;

      switch (framespersecond) {
        case 232:  framespersecond = 24; break;
        case 231:  framespersecond = 25; break;
        case 227:  framespersecond = 29; break;
        case 226:  framespersecond = 30; break;
        default:   _fd.close();	return(7);
      }
      dat16 = framespersecond * resolution;
   } 
   _ticksPerQuarterNote = dat16;
   calcMicrosecondDelta();	// we may have changed from default, so recalculate

   // load all tracks
   for (uint8_t i = 0; i<_trackCount; i++)
   {
	   int err;
	   
	   if ((err = _track[i].load(i, this)) != -1)
	   {
		   _fd.close();
		   return((10*(i+1))+err);
	   }
   }

  return(-1);
}

void MIDIFile::dump(void)
{
  DUMPS("\nFile Name:\t");
  DUMP(getFilename());
  DUMPS("\nFile format:\t");
  DUMP(getFormat());
  DUMPS("\nTracks:\t\t");
  DUMP(getTrackCount());
  DUMPS("\nTime division:\t");
  DUMP(getTicksPerQuarterNote());
  DUMPS("\nTempo:\t\t");
  DUMP(getTempo());
  DUMPS("\nMicrosec/tick:\t");
  DUMP(getMicrosecondDelta());
  DUMPS("\nTime Signature:\t");
  DUMP(getTimeSignature()>>8);
  DUMP('/');
  DUMP(getTimeSignature()&0xf);
  DUMP('\n');
 
#if DUMP_DATA
  for (uint8_t i=0; i<_trackCount; i++)
  {
	  _track[i].dump();
	  DUMP('\n');
  } 
#endif // DUMP_DATA
}

