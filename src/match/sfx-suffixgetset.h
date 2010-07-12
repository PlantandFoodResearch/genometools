/*
  Copyright (c) 2010 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2010 Center for Bioinformatics, University of Hamburg

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

#ifndef SFX_SUFFIXGETSET_H
#define SFX_SUFFIXGETSET_H

#include "core/assert_api.h"
#include "suffixptr.h"

typedef struct
{
  Suffixptr *sortspace;
  unsigned long sortspaceoffset,
                bucketleftidx;
} Suffixsortspace;

static inline void suffixptrassert(const Suffixsortspace *sssp,
                                   const Suffixptr *subbucket,
                                   unsigned long subbucketleft,
                                   unsigned long idx)
{
  unsigned long tmp = sssp->bucketleftidx - sssp->sortspaceoffset;
  gt_assert(sssp != NULL);
  gt_assert(sssp->sortspaceoffset <= sssp->bucketleftidx + subbucketleft + idx);
  gt_assert(subbucket + idx ==
            sssp->sortspace + (sssp->bucketleftidx+subbucketleft+idx-
                               sssp->sortspaceoffset));
  gt_assert(subbucket + idx ==
            sssp->sortspace + (subbucketleft + idx + tmp));
}

static inline unsigned long suffixptrget(const Suffixsortspace *sssp,
                                         const Suffixptr *subbucket,
                                         unsigned long subbucketleft,
                                         unsigned long idx)
{
  suffixptrassert(sssp,subbucket,subbucketleft,idx);
  return SUFFIXPTRGET(subbucket,idx);
}

static inline void suffixptrset(Suffixsortspace *sssp,
                                Suffixptr *subbucket,
                                unsigned long subbucketleft,
                                unsigned long idx,
                                unsigned long value)
{
  suffixptrassert(sssp,subbucket,subbucketleft,idx);
  SUFFIXPTRSET(subbucket,idx,value);
}

#endif
