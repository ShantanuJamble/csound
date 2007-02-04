/*
    pvinterp.c:

    Copyright (C) 1996 Richard Karpen

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

/*****************************************/
/*********** PVINTERP, PVCROSS ***********/
/******** By Richard Karpen 1996 *********/
/*****************************************/

#include "pvoc.h"
#include <math.h>

#define WLN   1         /* time window is WLN*2*ksmps long */
#define OPWLEN (2*WLN*csound->ksmps)    /* manifest used for final time wdw */

/************************************************************/
/*************PVBUFREAD**************************************/
/************************************************************/

int pvbufreadset(CSOUND *csound, PVBUFREAD *p)
{
    char     pvfilnam[MAXNAME];
    PVOCEX_MEMFILE  pp;
    int      frInc, chans, size; /* THESE SHOULD BE SAVED IN PVOC STRUCT */

    {
      PVOC_GLOBALS  *p_ = PVOC_GetGlobals(csound);
      p_->pvbufreadaddr = p;
    }

    if (p->auxch.auxp == NULL) {              /* if no buffers yet, alloc now */
      /* Assumes PVDATASIZE, PVFFTSIZE, PVWINLEN constant */
      MYFLT *fltp;
      csound->AuxAlloc(csound,
                       (PVDATASIZE + PVFFTSIZE * 3 + PVWINLEN) * sizeof(MYFLT),
                       &p->auxch);
      fltp = (MYFLT *) p->auxch.auxp;
      p->lastPhase = fltp;   fltp += PVDATASIZE;    /* and insert addresses */
      p->fftBuf = fltp;   /* fltp += PVFFTSIZE; */  /* Not needed */
    }

    csound->strarg2name(csound, pvfilnam, p->ifilno, "pvoc.", p->XSTRCODE);
    if (csound->PVOCEX_LoadFile(csound, pvfilnam, &pp) != 0)
      return csound->InitError(csound, Str("PVBUFREAD cannot load %s"),
                                       pvfilnam);

    p->frSiz = pp.fftsize;
    frInc    = pp.overlap;
    chans    = pp.chans;
    p->asr   = pp.srate;
    if (p->asr != csound->esr) {                /* & chk the data */
      csound->Warning(csound, Str("%s's srate = %8.0f, orch's srate = %8.0f"),
                              pvfilnam, p->asr, csound->esr);
    }
    if (p->frSiz > PVFRAMSIZE) {
      return csound->InitError(csound,
                               Str("PVOC frame %ld bigger than %ld in %s"),
                               (long) p->frSiz, (long) PVFRAMSIZE, pvfilnam);
    }
    if (p->frSiz < 128) {
      return csound->InitError(csound,
                               Str("PVOC frame %ld seems too small in %s"),
                               (long) p->frSiz, pvfilnam);
    }
    if (chans != 1) {
      return csound->InitError(csound, Str("%d chans (not 1) in PVOC file %s"),
                                       (int) chans, pvfilnam);
    }
    p->frPtr = (float*) pp.data;
    p->maxFr = pp.nframes - 1;
    p->frPktim = (MYFLT) csound->ksmps / (MYFLT) frInc;
    p->frPrtim = csound->esr / (MYFLT) frInc;
    size = pvfrsiz(p);          /* size used in def of OPWLEN ? */
    p->prFlg = 1;       /* true */
    /* amplitude scale for PVOC */
 /* p->scale = (MYFLT) pp.fftsize * ((MYFLT) pp.fftsize / (MYFLT) pp.winsize);
  */
    p->scale = (MYFLT) pp.fftsize * FL(0.5);
    p->scale *= csound->GetInverseRealFFTScale(csound, pp.fftsize);

    if ((OPWLEN / 2 + 1) > PVWINLEN ) {
      return csound->InitError(csound, Str("ksmps of %d needs wdw of %d, "
                                           "max is %d for pv %s"),
                                       csound->ksmps, (int) (OPWLEN / 2 + 1),
                                       (int) PVWINLEN, pvfilnam);
    }

    return OK;
}

int pvbufread(CSOUND *csound, PVBUFREAD *p)
{
    MYFLT  frIndx;
    MYFLT  *buf = p->fftBuf;
    int    size = pvfrsiz(p);

    if (p->auxch.auxp == NULL) {        /* RWD fix */
      return csound->PerfError(csound, Str("pvbufread: not initialised"));
    }
    if ((frIndx = *p->ktimpnt * p->frPrtim) < 0) {
      return csound->PerfError(csound, Str("PVOC timpnt < 0"));
    }
    if (frIndx > (MYFLT) p->maxFr) {    /* not past last one */
      frIndx = (MYFLT) p->maxFr;
      if (p->prFlg) {
        p->prFlg = 0;   /* false */
        csound->Warning(csound, Str("PVOC ktimpnt truncated to last frame"));
      }
    }
    FetchIn(p->frPtr, buf, size, frIndx);
    p->buf = buf;

    return OK;
}

/************************************************************/
/*************PVINTERP**************************************/
/************************************************************/
int pvinterpset(CSOUND *csound, PVINTERP *p)
{
    int      i;
    char     pvfilnam[MAXNAME];
    PVOCEX_MEMFILE  pp;
    int      frInc, chans, size; /* THESE SHOULD BE SAVED IN PVOC STRUCT */

    p->pp = PVOC_GetGlobals(csound);
    p->pvbufread = p->pp->pvbufreadaddr;
    if (p->pvbufread == NULL)
      return csound->InitError(csound,
                               Str("pvinterp: associated pvbufread not found"));

    if (p->auxch.auxp == NULL) {              /* if no buffers yet, alloc now */
      MYFLT *fltp;
      csound->AuxAlloc(csound,
                       (PVDATASIZE + PVFFTSIZE * 3 + PVWINLEN) * sizeof(MYFLT),
                       &p->auxch);
      fltp = (MYFLT *) p->auxch.auxp;
      p->lastPhase = fltp;   fltp += PVDATASIZE;    /* and insert addresses */
      p->fftBuf = fltp;      fltp += PVFFTSIZE;
      p->dsBuf = fltp;       fltp += PVFFTSIZE;
      p->outBuf = fltp;      fltp += PVFFTSIZE;
      p->window = fltp;
    }

    csound->strarg2name(csound, pvfilnam, p->ifilno, "pvoc.", p->XSTRCODE);
    if (csound->PVOCEX_LoadFile(csound, pvfilnam, &pp) != 0)
      return csound->InitError(csound, Str("PVINTERP cannot load %s"),
                                       pvfilnam);

    p->frSiz = pp.fftsize;
    frInc    = pp.overlap;
    chans    = pp.chans;
    p->asr   = pp.srate;
    if (p->asr != csound->esr) {                /* & chk the data */
      csound->Warning(csound, Str("%s's srate = %8.0f, orch's srate = %8.0f"),
                              pvfilnam, p->asr, csound->esr);
    }
    if (p->frSiz != p->pvbufread->frSiz) {
      return csound->InitError(csound,
                               Str("pvinterp: %s: frame size %d does not "
                                   "match pvbufread frame size %d\n"), pvfilnam,
                               (int) p->frSiz, (int) p->pvbufread->frSiz);
    }
    if (chans != 1) {
      return csound->InitError(csound, Str("%d chans (not 1) in PVOC file %s"),
                                       (int) chans, pvfilnam);
    }
    /* Check that pv->frSiz is a power of two too ? */
    p->frPtr = (float*) pp.data;
    p->baseFr = 0;  /* point to first data frame */
    p->maxFr = pp.nframes - 1;
    /* highest possible frame index */
    p->frPktim = (MYFLT) csound->ksmps / (MYFLT) frInc;
    /* factor by which to mult expand phase diffs (ratio of samp spacings) */
    p->frPrtim = csound->esr / (MYFLT) frInc;
    /* factor by which to mulitply 'real' time index to get frame index */
    size = pvfrsiz(p);          /* size used in def of OPWLEN ? */
    /* amplitude scale for PVOC */
 /* p->scale = (MYFLT) pp.fftsize * ((MYFLT) pp.fftsize / (MYFLT) pp.winsize);
  */
    p->scale = (MYFLT) pp.fftsize * FL(0.5);
    p->scale *= csound->GetInverseRealFFTScale(csound, pp.fftsize);
    /* 2*incr/OPWLEN scales down for win ovlp, windo'd 1ce (but 2ce?) */
    /* 1/frSiz is the required scale down before (i)FFT */
    p->prFlg = 1;    /* true */
    p->opBpos = 0;
    p->lastPex = FL(1.0);    /* needs to know last pitchexp to update phase */
    /* Set up time window */
    for (i = 0; i < pvdasiz(p); ++i) {      /* or maybe pvdasiz(p) */
      p->lastPhase[i] = FL(0.0);
    }
    if ((OPWLEN / 2 + 1) > PVWINLEN) {
      return csound->InitError(csound, Str("ksmps of %d needs wdw of %d, "
                                           "max is %d for pv %s"),
                                       csound->ksmps, (OPWLEN / 2 + 1),
                                       PVWINLEN, pvfilnam);
    }
    for (i = 0; i < OPWLEN / 2 + 1; ++i)    /* time window is OPWLEN long */
      p->window[i] = (MYFLT) (0.5 - 0.5 * cos(TWOPI*(double)i/(double)OPWLEN));
    /* NB: HANNING */
    for (i = 0; i< pvfrsiz(p); ++i)
      p->outBuf[i] = FL(0.0);
    MakeSinc(p->pp);                    /* sinctab is same for all instances */

    return OK;
}

int pvinterp(CSOUND *csound, PVINTERP *p)
{
    MYFLT   *ar = p->rslt;
    MYFLT   frIndx;
    MYFLT   *buf = p->fftBuf;
    MYFLT   *buf2 = p->dsBuf;
    int     asize = pvdasiz(p); /* fix */
    int     size = pvfrsiz(p);
    int     buf2Size, outlen;
    int     circBufSize = PVFFTSIZE;
    MYFLT   pex, scaleFac = p->scale;
    PVBUFREAD *q = p->pvbufread;
    long    i, j;

    if (p->auxch.auxp == NULL) {    /* RWD Fix */
      return csound->PerfError(csound, Str("pvinterp: not initialised"));
    }
    pex = *p->kfmod;
    outlen = (int) (((MYFLT) size) / pex);
    /* use outlen to check window/krate/transpose combinations */
    if (outlen>PVFFTSIZE) { /* Maximum transposition down is one octave */
                            /* ..so we won't run into buf2Size problems */
      return csound->PerfError(csound, Str("PVOC transpose too low"));
    }
    if (outlen<2*csound->ksmps) {   /* minimum post-squeeze windowlength */
      return csound->PerfError(csound, Str("PVOC transpose too high"));
    }
    buf2Size = OPWLEN;     /* always window to same length after DS */
    if ((frIndx = *p->ktimpnt * p->frPrtim) < 0) {
      return csound->PerfError(csound, Str("PVOC timpnt < 0"));
    }
    if (frIndx > (MYFLT)p->maxFr) { /* not past last one */
      frIndx = (MYFLT)p->maxFr;
      if (p->prFlg) {
        p->prFlg = 0;   /* false */
        csound->Warning(csound, Str("PVOC ktimpnt truncated to last frame"));
      }
    }
    FetchIn(p->frPtr, buf, size, frIndx);

/* Here's where the interpolation happens ***********************/
    if (pex > FL(1.0))
      scaleFac /= pex;
    for (i = 0, j = 1; i <= size; i += 2, j += 2) {
      buf[i] = buf[i] * *p->kampscale2;
      q->buf[i] = q->buf[i] * *p->kampscale1;
      buf[j] = buf[j] * *p->kfreqscale2;
      q->buf[j] = q->buf[j] * *p->kfreqscale1;
      buf[i] = (buf[i] + ((q->buf[i] - buf[i]) * *p->kampinterp)) * scaleFac;
      buf[j] = (buf[j] + ((q->buf[j] - buf[j]) * *p->kfreqinterp));
    }
/*******************************************************************/
    FrqToPhase(buf, asize, pex * (MYFLT) csound->ksmps, p->asr,
               (MYFLT) (0.5 * ((pex / p->lastPex) - 1)));
    /* accumulate phase and wrap to range -PI to PI */
    RewrapPhase(buf, asize, p->lastPhase);

    Polar2Real_PVOC(csound, buf, (int) size);

    if (pex != FL(1.0))
      UDSample(p->pp, buf,
               (FL(0.5) * ((MYFLT) size - pex * (MYFLT) buf2Size)), buf2,
               size, buf2Size, pex);
    else
      memcpy(buf2, buf + (int) ((size - buf2Size) >> 1),
             sizeof(MYFLT) * buf2Size);
    ApplyHalfWin(buf2, p->window, buf2Size);

    addToCircBuf(buf2, p->outBuf, p->opBpos, csound->ksmps, circBufSize);
    writeClrFromCircBuf(p->outBuf, ar, p->opBpos, csound->ksmps, circBufSize);
    p->opBpos += csound->ksmps;
    if (p->opBpos > circBufSize)
      p->opBpos -= circBufSize;
    addToCircBuf(buf2 + csound->ksmps, p->outBuf, p->opBpos,
                 buf2Size - csound->ksmps, circBufSize);
    p->lastPex = pex;        /* needs to know last pitchexp to update phase */

    return OK;
}

/************************************************************/
/************* PVCROSS **************************************/
/************************************************************/
int pvcrossset(CSOUND *csound, PVCROSS *p)
{
    int      i;
    char     pvfilnam[MAXNAME];
    PVOCEX_MEMFILE  pp;
    int      frInc, chans, size; /* THESE SHOULD BE SAVED IN PVOC STRUCT */

    p->pp = PVOC_GetGlobals(csound);
    p->pvbufread = p->pp->pvbufreadaddr;
    if (p->pvbufread == NULL)
      return csound->InitError(csound,
                               Str("pvcross: associated pvbufread not found"));

    if (p->auxch.auxp == NULL) {              /* if no buffers yet, alloc now */
      MYFLT *fltp;
      csound->AuxAlloc(csound,
                       (PVDATASIZE + PVFFTSIZE * 3 + PVWINLEN) * sizeof(MYFLT),
                       &p->auxch);
      fltp = (MYFLT *) p->auxch.auxp;
      p->lastPhase = fltp;   fltp += PVDATASIZE;    /* and insert addresses */
      p->fftBuf = fltp;      fltp += PVFFTSIZE;
      p->dsBuf = fltp;       fltp += PVFFTSIZE;
      p->outBuf = fltp;      fltp += PVFFTSIZE;
      p->window = fltp;
    }
    csound->strarg2name(csound, pvfilnam, p->ifilno, "pvoc.", p->XSTRCODE);
    if (csound->PVOCEX_LoadFile(csound, pvfilnam, &pp) != 0)
      return csound->InitError(csound, Str("PVCROSS cannot load %s"), pvfilnam);

    p->frSiz = pp.fftsize;
    frInc    = pp.overlap;
    chans    = pp.chans;
    p->asr   = pp.srate;
    if (p->asr != csound->esr) {                /* & chk the data */
      csound->Warning(csound, Str("%s's srate = %8.0f, orch's srate = %8.0f"),
                              pvfilnam, p->asr, csound->esr);
    }
    if (p->frSiz != p->pvbufread->frSiz) {
      return csound->InitError(csound,
                               Str("pvcross: %s: frame size %d does not "
                                   "match pvbufread frame size %d\n"), pvfilnam,
                               (int) p->frSiz, (int) p->pvbufread->frSiz);
    }
    if (chans != 1) {
      return csound->InitError(csound, Str("%d chans (not 1) in PVOC file %s"),
                                       (int) chans, pvfilnam);
    }
    /* Check that pv->frSiz is a power of two too ? */
    p->frPtr = (float*) pp.data;
    p->baseFr = 0;  /* point to first data frame */
    p->maxFr = pp.nframes - 1;
    /* highest possible frame index */
    p->frPktim = (MYFLT) csound->ksmps / (MYFLT) frInc;
    /* factor by which to mult expand phase diffs (ratio of samp spacings) */
    p->frPrtim = csound->esr / (MYFLT) frInc;
    /* factor by which to mulitply 'real' time index to get frame index */
    size = pvfrsiz(p);          /* size used in def of OPWLEN ? */
    /* amplitude scale for PVOC */
 /* p->scale = (MYFLT) pp.fftsize * ((MYFLT) pp.fftsize / (MYFLT) pp.winsize);
  */
    p->scale = (MYFLT) pp.fftsize * FL(0.5);
    p->scale *= csound->GetInverseRealFFTScale(csound, pp.fftsize);
    p->prFlg = 1;    /* true */
    p->opBpos = 0;
    p->lastPex = FL(1.0);   /* needs to know last pitchexp to update phase */
    /* Set up time window */
    for (i = 0; i < pvdasiz(p); ++i) {      /* or maybe pvdasiz(p) */
        p->lastPhase[i] = FL(0.0);
    }
    if ((OPWLEN / 2 + 1) > PVWINLEN ) {
      return csound->InitError(csound, Str("ksmps of %d needs wdw of %d, "
                                           "max is %d for pv %s"),
                                       csound->ksmps, (OPWLEN / 2 + 1),
                                       PVWINLEN, pvfilnam);
    }
    for (i = 0; i < OPWLEN / 2 + 1; ++i)    /* time window is OPWLEN long */
      p->window[i] = (MYFLT) (0.5 - 0.5 * cos(TWOPI*(double)i/(double)OPWLEN));
    /* NB: HANNING */
    for (i = 0; i < pvfrsiz(p); ++i)
      p->outBuf[i] = FL(0.0);
    MakeSinc(p->pp);                    /* sinctab is same for all instances */

    return OK;
}

int pvcross(CSOUND *csound, PVCROSS *p)
{
    int     n;
    MYFLT   *ar = p->rslt;
    MYFLT   frIndx;
    MYFLT   *buf = p->fftBuf;
    MYFLT   *buf2 = p->dsBuf;
    int     asize = pvdasiz(p);         /* fix */
    int     size = pvfrsiz(p);
    int     buf2Size, outlen;
    int     circBufSize = PVFFTSIZE;
    int     specwp = (int) *p->ispecwp; /* spectral warping flag */
    MYFLT   pex, scaleFac = p->scale;
    PVBUFREAD *q = p->pvbufread;
    long    i, j;
    MYFLT   ampscale1 = *p->kampscale1;
    MYFLT   ampscale2 = *p->kampscale2;

    if (p->auxch.auxp == NULL) {        /* RWD Fix */
      return csound->PerfError(csound, Str("pvcross: not initialised"));
    }
    pex = *p->kfmod;
    outlen = (int) (((MYFLT) size) / pex);
    /* use outlen to check window/krate/transpose combinations */
    if (outlen>PVFFTSIZE) { /* Maximum transposition down is one octave */
                            /* ..so we won't run into buf2Size problems */
      return csound->PerfError(csound, Str("PVOC transpose too low"));
    }
    if (outlen<2*csound->ksmps) {  /* minimum post-squeeze windowlength */
      return csound->PerfError(csound, Str("PVOC transpose too high"));
    }
    buf2Size = OPWLEN;     /* always window to same length after DS */
    if ((frIndx = *p->ktimpnt * p->frPrtim) < 0) {
      return csound->PerfError(csound, Str("PVOC timpnt < 0"));
    }
    if (frIndx > (MYFLT) p->maxFr) {    /* not past last one */
      frIndx = (MYFLT) p->maxFr;
      if (p->prFlg) {
        p->prFlg = 0;   /* false */
        csound->Warning(csound, Str("PVOC ktimpnt truncated to last frame"));
      }
    }

    FetchIn(p->frPtr, buf, size, frIndx);

/**** Apply amplitudes from pvbufread ********/
    if (pex > FL(1.0))
      scaleFac /= pex;
    for (i = 0, j = 0; i <= size; i += 2, j++)
      buf[i] = ((buf[i] * ampscale2) + (q->buf[i] * ampscale1)) * scaleFac;
/***************************************************/

    FrqToPhase(buf, asize, pex * (MYFLT) csound->ksmps, p->asr,
               (MYFLT) (0.5 * ((pex / p->lastPex) - 1)));
    /* accumulate phase and wrap to range -PI to PI */
    RewrapPhase(buf, asize, p->lastPhase);

    if (specwp == 0 || (p->prFlg)++ == -(int) specwp) {
      /* ?screws up when prFlg used */
      /* specwp=0 => normal; specwp = -n => just nth frame */
      if (specwp < 0)
        csound->Message(csound, Str("PVOC debug: one frame gets through\n"));
      if (specwp > 0)
        PreWarpSpec(p->pp, buf, asize, pex);

      Polar2Real_PVOC(csound, buf, (int) size);

      if (pex != FL(1.0))
        UDSample(p->pp, buf,
                 (FL(0.5) * ((MYFLT) size - pex * (MYFLT) buf2Size)), buf2,
                 size, buf2Size, pex);
      else
        memcpy(buf2, buf + (int) ((size - buf2Size) >> 1),
               sizeof(MYFLT) * buf2Size);
      if (specwp >= 0)
        ApplyHalfWin(buf2, p->window, buf2Size);
    }
    else {
      for (n = 0; n < buf2Size; ++n)
        buf2[n] = FL(0.0);
    }

    addToCircBuf(buf2, p->outBuf, p->opBpos, csound->ksmps, circBufSize);
    writeClrFromCircBuf(p->outBuf, ar, p->opBpos, csound->ksmps, circBufSize);
    p->opBpos += csound->ksmps;
    if (p->opBpos > circBufSize)
      p->opBpos -= circBufSize;
    addToCircBuf(buf2 + csound->ksmps, p->outBuf, p->opBpos,
                 buf2Size - csound->ksmps, circBufSize);
    p->lastPex = pex;       /* needs to know last pitchexp to update phase */

    return OK;
}

