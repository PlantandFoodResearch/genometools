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

#ifndef CHAINDEF_H
#define CHAINDEF_H

#include "core/error_api.h"
#include "core/str_array_api.h"
#include "seqpos-def.h"
#include "verbose-def.h"

typedef Seqpos GtChainpostype;
typedef long GtChainscoretype;
typedef struct GtChain GtChain;

/*
  We use functions of the following type to report chains.
*/

typedef int (*GtChainprocessor)(void *,GtChain *,GtError *err);
typedef struct GtFragmentinfotable GtFragmentinfotable;
typedef struct GtChainmode GtChainmode;

GtFragmentinfotable *gt_chain_fragmentinfotable_new(
                           unsigned long numberoffragments);

void gt_chain_fragmentinfotable_delete(GtFragmentinfotable *fragmentinfotable);

void gt_chain_fragmentinfotable_empty(GtFragmentinfotable *fragmentinfotable);

void gt_chain_fragmentinfotable_add(GtFragmentinfotable *fragmentinfotable,
                                    GtChainpostype start1,
                                    GtChainpostype end1,
                                    GtChainpostype start2,
                                    GtChainpostype end2,
                                    GtChainscoretype weight);

void gt_chain_fillthegapvalues(GtFragmentinfotable *fragmentinfotable);

int gt_chain_fastchaining(const GtChainmode *chainmode,
                          GtChain *chain,
                          GtFragmentinfotable *fragmentinfotable,
                          bool gapsL1,
                          unsigned int presortdim,
                          bool withequivclasses,
                          GtChainprocessor chainprocessor,
                          void *cpinfo,
                          Verboseinfo *verboseinfo,
                          GtError *err);

void gt_chain_possiblysortopenformatfragments(
                             Verboseinfo *verboseinfo,
                             GtFragmentinfotable *fragmentinfotable,
                             unsigned int presortdim);

GtChainmode *gt_chain_chainmode_new(bool weightfactorset,
                                    unsigned long maxgap,
                                    bool globalset,
                                    const GtStrArray *globalargs,
                                    bool localset,
                                    const GtStrArray *localargs,
                                    GtError *err);

void gt_chain_chainmode_free(GtChainmode *gtchainmode);

GtFragmentinfotable *gt_chain_analyzeopenformatfile(double weightfactor,
                                                    const char *matchfile,
                                                    GtError *err);

#endif
