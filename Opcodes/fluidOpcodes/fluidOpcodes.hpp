#ifndef FLUIDOPCODE_H
#define  FLUIDOPCODE_H

#include <fluidsynth.h>
#include "csoundCore.h"

typedef struct {
    OPDS    h;
    
    // OUTPUTS
    MYFLT   *iEngineNum;
    
} FLUIDENGINE;

typedef struct {
    OPDS    h;
    
    // OUTPUTS
    MYFLT   *iInstrumentNumber;
    
    // INPUTS
    MYFLT   *filename, *iEngineNum;
} FLUIDLOAD;

typedef struct {
	OPDS	h;
	
	// INPUTS
	MYFLT 	*iEngineNumber, *iChannelNumber, *iInstrumentNumber, *iBankNumber;
	MYFLT	*iPresetNumber;
	
} FLUID_PROGRAM_SELECT;

typedef struct {
    OPDS    h;
    
    // INPUTS
    MYFLT   *iEngineNumber, *iChannelNumber, *iMidiKeyNumber, *iVelocity;
    
    bool released;
    
} FLUIDPLAY;

typedef struct {
    OPDS    h;
    MYFLT   *aLeftOut, *aRightOut;
    
    MYFLT   *iEngineNum;
    
    int     blockSize;
} FLUIDOUT;

#endif
