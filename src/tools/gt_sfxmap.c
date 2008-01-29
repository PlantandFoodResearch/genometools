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

#include "libgtcore/error.h"
#include "libgtcore/option.h"
#include "libgtcore/versionfunc.h"
#include "libgtmatch/sarr-def.h"
#include "libgtmatch/verbose-def.h"
#include "libgtmatch/stamp.h"
#include "libgtmatch/esa-seqread.h"
#include "libgtmatch/esa-map.pr"
#include "libgtmatch/test-encseq.pr"
#include "libgtmatch/pos2seqnum.pr"
#include "libgtmatch/test-mappedstr.pr"
#include "libgtmatch/sfx-suftaborder.pr"
#include "libgtmatch/echoseq.pr"
#include "tools/gt_sfxmap.h"

typedef struct
{
  bool usestream,
       verbose,
       inputtis,
       inputsuf,
       inputdes,
       inputbwt,
       inputlcp;
  unsigned long trials;
} Sfxmapoptions;

static OPrval parse_options(Sfxmapoptions *sfxmapoptions,
                            int *parsed_args,
                            int argc,
                            const char **argv,
                            Error *err)
{
  OptionParser *op;
  Option *optionstream, *optionverbose, *optiontrials,
         *optiontis, *optionsuf, *optiondes, *optionbwt, *optionlcp;
  OPrval oprval;

  error_check(err);
  op = option_parser_new("[options] indexname",
                         "Map or Stream <indexname> and check consistency.");
  option_parser_set_mailaddress(op,"<kurtz@zbh.uni-hamburg.de>");
  optionstream = option_new_bool("stream","stream the index",
                                 &sfxmapoptions->usestream,false);
  option_parser_add_option(op, optionstream);

  optiontrials = option_new_ulong("trials",
                                  "specify number of sequential trials",
                                  &sfxmapoptions->trials,0);
  option_parser_add_option(op, optiontrials);

  optiontis = option_new_bool("tis","input the encoded sequence",
                              &sfxmapoptions->inputtis,
                              false);
  option_parser_add_option(op, optiontis);

  optiondes = option_new_bool("des","input the descriptions",
                              &sfxmapoptions->inputdes,
                              false);
  option_parser_add_option(op, optiondes);

  optionsuf = option_new_bool("suf","input the suffix array",
                              &sfxmapoptions->inputsuf,
                              false);
  option_parser_add_option(op, optionsuf);

  optionlcp = option_new_bool("lcp","input the lcp-table",
                              &sfxmapoptions->inputlcp,
                              false);
  option_parser_add_option(op, optionlcp);

  optionbwt = option_new_bool("bwt","input the Burrows-Wheeler Transformation",
                              &sfxmapoptions->inputbwt,
                              false);
  option_parser_add_option(op, optionbwt);

  optionverbose = option_new_bool("v","be verbose",&sfxmapoptions->verbose,
                                  false);
  option_parser_add_option(op, optionverbose);

  option_parser_set_min_max_args(op, 1, 2);
  oprval = option_parser_parse(op, parsed_args, argc, argv, versionfunc, err);
  option_parser_delete(op);
  return oprval;
}

int gt_sfxmap(int argc, const char **argv, Error *err)
{
  Str *indexname;
  bool haserr = false;
  Suffixarray suffixarray;
  Seqpos totallength;
  int parsed_args;
  Verboseinfo *verboseinfo;
  Sfxmapoptions sfxmapoptions;
  unsigned int demand = 0;

  error_check(err);

  switch (parse_options(&sfxmapoptions,&parsed_args, argc, argv,
                        err))
  {
    case OPTIONPARSER_OK: break;
    case OPTIONPARSER_ERROR: return -1;
    case OPTIONPARSER_REQUESTS_EXIT: return 0;
  }
  assert(argc >= 2 && parsed_args == argc - 1);

  indexname = str_new_cstr(argv[parsed_args]);
  verboseinfo = newverboseinfo(sfxmapoptions.verbose);
  if (sfxmapoptions.inputtis)
  {
    demand |= SARR_ESQTAB;
  }
  if (sfxmapoptions.inputdes)
  {
    demand |= SARR_DESTAB;
  }
  if (sfxmapoptions.inputsuf)
  {
    demand |= SARR_SUFTAB;
  }
  if (sfxmapoptions.inputlcp)
  {
    demand |= SARR_LCPTAB;
  }
  if (sfxmapoptions.inputbwt)
  {
    demand |= SARR_BWTTAB;
  }
  if ((sfxmapoptions.usestream ? streamsuffixarray
                               : mapsuffixarray)(&suffixarray,
                                                 &totallength,
                                                 demand,
                                                 indexname,
                                                 verboseinfo,
                                                 err) != 0)
  {
    haserr = true;
  }
  freeverboseinfo(&verboseinfo);
  if (!haserr)
  {
    int readmode;

    for (readmode = 0; readmode < 4; readmode++)
    {
      if (isdnaalphabet(suffixarray.alpha) ||
         ((Readmode) readmode) == Forwardmode ||
         ((Readmode) readmode) == Reversemode)
      {
        if (testencodedsequence(suffixarray.filenametab,
                                suffixarray.encseq,
                                (Readmode) readmode,
                                getsymbolmapAlphabet(suffixarray.alpha),
                                sfxmapoptions.trials,
                                err) != 0)
        {
          haserr = true;
          break;
        }
      }
    }
  }
  if (!haserr)
  {
    if (checkspecialrangesfast(suffixarray.encseq) != 0)
    {
      haserr = true;
    }
  }
  if (!haserr)
  {
    if (checkmarkpos(suffixarray.encseq,suffixarray.numofdbsequences,
                     err) != 0)
    {
      haserr = true;
    }
  }
  if (suffixarray.prefixlength > 0 && !haserr)
  {
    if (verifymappedstr(&suffixarray,err) != 0)
    {
      haserr = true;
    }
  }
  if (!haserr && sfxmapoptions.inputsuf && !sfxmapoptions.usestream)
  {
    Sequentialsuffixarrayreader *ssar;

    if (sfxmapoptions.inputlcp)
    {
      ssar = newSequentialsuffixarrayreaderfromfile(indexname,
                                                    SARR_LCPTAB,
                                                    SEQ_scan,
                                                    err);
    } else
    {
      ssar = NULL;
    }
    checkentiresuftab(suffixarray.encseq,
                      suffixarray.readmode,
                      getcharactersAlphabet(suffixarray.alpha),
                      suffixarray.suftab,
                      ssar,
                      false, /* specialsareequal  */
                      false,  /* specialsareequalatdepth0 */
                      0,
                      err);
    if (ssar != NULL)
    {
      freeSequentialsuffixarrayreader(&ssar);
    }
  }
  if (sfxmapoptions.inputdes && !haserr)
  {
    checkalldescriptions(suffixarray.destab,suffixarray.destablength,
                         suffixarray.numofdbsequences);
  }
  str_delete(indexname);
  freesuffixarray(&suffixarray);
  return haserr ? -1 : 0;
}
