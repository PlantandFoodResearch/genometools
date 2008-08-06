/*
  Copyright (c) 2007 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg

  Permission to use, copy, modify, and distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef EIS_VOIDITF_H
#define EIS_VOIDITF_H

#include "seqpos-def.h"
#include "sarr-def.h"
#include "splititv.h"

Seqpos bwtseqfirstmatch(const void *voidbwtSeq,Seqpos bound);

/** Iterator for positions defined by a lower and upper bound
 */

typedef struct Bwtseqpositioniterator Bwtseqpositioniterator;

Bwtseqpositioniterator *newBwtseqpositioniterator(const void *bwtSeq,
                                                  Seqpos lowerbound,
                                                  Seqpos upperbound);

bool nextBwtseqpositioniterator(Seqpos *pos,Bwtseqpositioniterator *bspi);

bool nextBwtseqpositionwithoutSEPiterator(Seqpos *pos,
                                          Bwtseqpositioniterator *bspi);

Uchar bwtseqgetsymbol(Seqpos bound,const void *voidbwtSeq);

void freeBwtseqpositioniterator(Bwtseqpositioniterator **bspi);

typedef struct Bwtseqcontextiterator Bwtseqcontextiterator;

Bwtseqcontextiterator *newBwtseqcontextiterator(const void *voidbwtSeq,
                                                Seqpos bound);

Uchar nextBwtseqcontextiterator(Seqpos *bound,Bwtseqcontextiterator *bsci);

void freeBwtseqcontextiterator(Bwtseqcontextiterator **bsci);

void bwtrangesplitwithoutspecial(ArrayBoundswithchar *bwci,
                                 Seqpos *rangeOccs,
                                 const void *voidBwtSeq,
                                 const Lcpinterval *parent);

void *loadvoidBWTSeqForSA(const Str *indexname,
                          const Suffixarray *suffixarray,
                          Seqpos totallength,
                          Error *err);

void deletevoidBWTSeq(void *packedindex);

unsigned long voidpackedindexuniqueforward(const void *voidbwtseq,
                                           UNUSED unsigned long offset,
                                           UNUSED Seqpos left,
                                           UNUSED Seqpos right,
                                           UNUSED Seqpos *witnessposition,
                                           const Uchar *qstart,
                                           const Uchar *qend);

unsigned long voidpackedindexmstatsforward(const void *voidbwtseq,
                                           UNUSED unsigned long offset,
                                           UNUSED Seqpos left,
                                           UNUSED Seqpos right,
                                           Seqpos *witnessposition,
                                           const Uchar *qstart,
                                           const Uchar *qend);

void pck_exactpatternmatching(const void *voidbwtseq,
                              const Uchar *pattern,
                              unsigned long patternlength,
                              Seqpos totallength,
                              void (*processmatch)(void *,bool,Seqpos,
                                                   Seqpos,Seqpos,
                                                   unsigned long),
                              void *processmatchinfo);

Seqpos voidpackedfindfirstmatchconvert(const void *voidbwtseq,
                                       Seqpos witnessbound,
                                       unsigned long matchlength);

#endif
