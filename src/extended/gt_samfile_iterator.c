/*
  Copyright (c) 2011 Dirk Willrodt <willrodt@zbh.uni-hamburg.de>
  Copyright (c) 2011 Center for Bioinformatics, University of Hamburg

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

#include "core/ensure.h"
#include "core/ma_api.h"
#include "extended/gt_sam_alignment.h"
#include "extended/gt_sam_alignment_rep.h"
#include "external/samtools-0.1.18/sam.h"

#include "extended/gt_samfile_iterator.h"

struct GtSamfileIter {
  samfile_t *samfile;
  GtSamAlignment *current_alignment;
  GtAlphabet *alphabet;
};

GtSamfileIter *gt_samfile_iter_new(const char *samfilename,
                                   const char *mode,
                                   void *aux,
                                   GtAlphabet *alphabet) {
  GtSamfileIter *s_iter;
  s_iter = gt_malloc(sizeof (GtSamfileIter));
  s_iter->samfile = samopen(samfilename, mode, aux);
  s_iter->current_alignment = NULL;
  s_iter->alphabet = gt_alphabet_ref(alphabet);
  return s_iter;
}

GtSamfileIter *gt_samfile_iter_new_bam(const char *bamfilename,
                                       GtAlphabet *alphabet) {
  return gt_samfile_iter_new(bamfilename,
                         "rb",
                         NULL,
                         alphabet);
}

GtSamfileIter *gt_samfile_iter_new_sam(const char *samfilename,
                                       GtAlphabet *alphabet,
                                       const char *auxfilename) {
  return gt_samfile_iter_new(samfilename,
                         "r",
                         (void *) auxfilename,
                         alphabet);
}

void gt_samfile_iter_delete(GtSamfileIter *s_iter) {
  samclose(s_iter->samfile);
  if (s_iter->current_alignment)
    gt_sam_alignment_delete(s_iter->current_alignment);
  gt_alphabet_delete(s_iter->alphabet);
  gt_free(s_iter);
}

int gt_samfile_iter_next(GtSamfileIter *s_iter,
                         GtSamAlignment **gt_s_alignment) {
  int read;
  if (s_iter->current_alignment == NULL)
    s_iter->current_alignment = gt_sam_alignment_new(s_iter->alphabet);
  read = samread(s_iter->samfile, s_iter->current_alignment->s_alignment);
  if (read > 0) {
    *gt_s_alignment = s_iter->current_alignment;
    return read;
  }
  else {
    *gt_s_alignment = NULL;
    return read;
  }
}

const char *gt_samfile_iter_reference(GtSamfileIter *s_iter,
                                      int32_t reference_num) {
  gt_assert(reference_num >= 0);
  gt_assert(reference_num < s_iter->samfile->header->n_targets);
  return s_iter->samfile->header->target_name[reference_num];
}
