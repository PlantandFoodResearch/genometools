/*
  Copyright (c) 2009 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2009 Center for Bioinformatics, University of Hamburg

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

#ifndef SFX_LINLCP_H
#define SFX_LINLCP_H

#include "core/encseq.h"
#include "core/compact_ulong_store.h"
#include "match/sarr-def.h"

GtCompactUlongStore *gt_lcp9_manzini(GtCompactUlongStore *spacefortab,
                                 const GtEncseq *encseq,
                                 GtReadmode readmode,
                                 GtUword partwidth,
                                 GtUword totallength,
                                 const ESASuffixptr *suftab);

unsigned int *gt_plain_lcp13_manzini(const GtUchar *sequence,
                                     GtUword totallength,
                                     const void *suftab,
                                     size_t unitsize);

int gt_lcptab_lightweightcheck(const char *esaindexname,
                               const GtEncseq *encseq,
                               GtReadmode readmode,
                               const ESASuffixptr *suftab,
                               GtLogger *logger,
                               GtError *err);

#endif
