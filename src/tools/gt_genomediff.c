/*
  Copyright (c) 2010 Willrodt <dwillrodt@zbh.uni-hamburg.de>
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

#include <stdio.h>

#include "core/array2dim_api.h"
#include "core/encseq_api.h"
#include "core/log_api.h"
#include "core/logger.h"
#include "core/mathsupport.h"
#include "core/str_array_api.h"
#include "core/unused_api.h"

#include "match/eis-voiditf.h"
#include "match/genomediff.h"
#include "match/idx-limdfs.h"
#include "match/shu-dfs.h"
#include "match/shu-divergence.h"
#include "match/shu-genomediff-simple.h"
#include "match/shu_encseq_gc.h"

#include "tools/gt_genomediff.h"

static void* gt_genomediff_arguments_new(void)
{
  GtGenomediffArguments *arguments = gt_calloc((size_t) 1, sizeof *arguments);
  arguments->indexname = gt_str_new();
  arguments->queryname = gt_str_array_new();
  return arguments;
}

static void gt_genomediff_arguments_delete(void *tool_arguments)
{
  GtGenomediffArguments *arguments = tool_arguments;
  if (!arguments) return;
  gt_str_delete(arguments->indexname);
  gt_str_array_delete(arguments->queryname);
  gt_option_delete(arguments->ref_esaindex);
  gt_option_delete(arguments->ref_pckindex);
  gt_option_delete(arguments->ref_queryname);
  gt_free(arguments);
}

static GtOptionParser* gt_genomediff_option_parser_new(void *tool_arguments)
{
  GtGenomediffArguments *arguments = tool_arguments;
  GtOptionParser *op;
  GtOption *option, *optionquery, *optionesaindex, *optionpckindex;
  gt_assert(arguments);

  /* init */
  op = gt_option_parser_new("[option ...] [-esa|-pck] indexname "
                            "-query sequencefile",
                            "Reads in a index of type fm or esa.");

  /* -maxdepth */
  option =  gt_option_new_int("maxdepth", "max depth of .pbi-file",
                              &arguments->user_max_depth, -1);
  gt_option_is_development_option(option);
  gt_option_parser_add_option(op, option);

  /* -max_n */
  option = gt_option_new_ulong("max_n", "Number of precalculated valuesi "
                             "for ln(n!) and pmax(x)",
                             &arguments->max_ln_n_fac, 1000UL);
  gt_option_is_development_option(option);
  gt_option_parser_add_option(op, option);

  /* -v */
  option = gt_option_new_verbose(&arguments->verbose);
  gt_option_parser_add_option(op, option);

  /* -esa */
  optionesaindex = gt_option_new_string("esa",
                                     "Specify index (enhanced suffix array)",
                                     arguments->indexname, NULL);
  gt_option_is_development_option(optionesaindex);
  gt_option_parser_add_option(op, optionesaindex);

  /* -pck */
  optionpckindex = gt_option_new_string("pck",
                                        "Specify index (packed index)",
                                        arguments->indexname, NULL);
  gt_option_parser_add_option(op, optionpckindex);

  gt_option_exclude(optionesaindex,optionpckindex);
  gt_option_is_mandatory_either(optionesaindex,optionpckindex);

  /* ref esa */
  arguments->ref_esaindex = gt_option_ref(optionesaindex);

  /* ref pck */
  arguments->ref_pckindex = gt_option_ref(optionpckindex);

  /* -query */
  optionquery = gt_option_new_filenamearray("query",
                                       "Files containing the query sequences "
                                       "if this option is set a simple "
                                       "shustring search will be used." ,
                                       arguments->queryname);
  gt_option_parser_add_option(op, optionquery);

  /* ref query */
  arguments->ref_queryname = gt_option_ref(optionquery);

  /* mail */
  gt_option_parser_set_mailaddress(op, "<dwillrodt@zbh.uni-hamburg.de>");
  return op;
}

static int gt_genomediff_arguments_check(GT_UNUSED int rest_argc,
                                       void *tool_arguments,
                                       GT_UNUSED GtError *err)
{
  GtGenomediffArguments *arguments = tool_arguments;
  int had_err = 0;
  gt_error_check(err);
  gt_assert(arguments);

  /* XXX: do some checking after the option have been parsed (usally this is not
     necessary and this function can be removed completely). */
  if (gt_option_is_set(arguments->ref_esaindex))
  {
    arguments->withesa = true;
  } else
  {
    gt_assert(gt_option_is_set(arguments->ref_pckindex));
    arguments->withesa = false;
  }
  if (gt_option_is_set(arguments->ref_queryname))
    arguments->simplesearch = true;
  else
    arguments->simplesearch = false;
  if (arguments->withesa)
  {
    printf("not implemented option -esa used, sorry, try -pck instead\n");
    had_err = 1;
  }
  return had_err;
}

static int gt_genomediff_runner(GT_UNUSED int argc,
                                GT_UNUSED const char **argv,
                                GT_UNUSED int parsed_args,
                                void *tool_arguments, GtError *err)
{
  GtGenomediffArguments *arguments = tool_arguments;
  int had_err = 0;
  Genericindex *genericindexSubject;
  GtLogger *logger;
  const GtEncseq *encseq = NULL;

  gt_error_check(err);
  gt_assert(arguments);

  logger = gt_logger_new(arguments->verbose,
                         GT_LOGGER_DEFLT_PREFIX,
                         stdout);
  gt_assert(logger);

  genericindexSubject = genericindex_new(gt_str_get(
                                           arguments->indexname),
                                         arguments->withesa,
                                         true,
                                         false,
                                         true,
                                         arguments->user_max_depth,
                                         logger,
                                         err);
  if (genericindexSubject == NULL)
    had_err = 1;
  else
    encseq = genericindex_getencseq(genericindexSubject);

  if (!had_err)
  {
    if (arguments->simplesearch)
      had_err = gt_genomediff_run_simple_search(genericindexSubject,
                                             encseq,
                                             logger,
                                             arguments,
                                             err);
    else
    {
      unsigned long numofchars,
                    numoffiles,
                    totallength,
                    start = 0UL,
                    end = 0UL,
                    i, j,
                    *filelength;
      double **shulen, *gc_contents;
      const GtAlphabet *alphabet;
      const GtStrArray *filenames;
      const FMindex *subjectindex;

      alphabet = gt_encseq_alphabet(encseq);
      numofchars = (unsigned long) gt_alphabet_num_of_chars(alphabet);
      totallength = genericindex_get_totallength(genericindexSubject);
      gt_logger_log(logger, "totallength=%lu", totallength);
      filenames = gt_encseq_filenames(encseq);
      subjectindex = genericindex_get_packedindex(genericindexSubject);
      numoffiles = gt_encseq_num_of_files(encseq);
      gt_logger_log(logger, "number of files=%lu", numoffiles);
      gt_array2dim_calloc(shulen, numoffiles, numoffiles);
      filelength = gt_calloc((size_t) numoffiles, sizeof (unsigned long));

      for (i = 0UL; i < numoffiles; i++)
      {
        start = gt_encseq_filestartpos(encseq, i);
        filelength[i] =
          (unsigned long) gt_encseq_effective_filelength(encseq, i) - 1;
        end = start + filelength[i];
        gt_logger_log(logger,
               "File: %s (No: %lu)\tstart: %lu, end: %lu, sep: %lu",
               gt_str_array_get(filenames, i),
               i,
               start,
               end,
               end + 1);
      }
      had_err = gt_pck_calculate_shulen(subjectindex,
                                         encseq,
                                         shulen,
                                         (unsigned long) numofchars,
                                         totallength,
                                         logger,
                                         err);
      if (!had_err)
      {
        gc_contents = gt_encseq_get_gc(encseq,
                                      true,
                                      false,
                                      err);
        if (gc_contents == NULL)
          had_err = -1;
      }
      if (!had_err)
      {
        for (i = 0; i < numoffiles; i++)
        {
          unsigned long length_i;
          length_i = filelength[i];
          for (j = 0; j < numoffiles; j++)
            shulen[i][j] = shulen[i][j] / length_i;
        }
      }
      gt_logger_log(logger, "table of shulens");
      if (!had_err && gt_logger_enabled(logger))
      {
        for (i = 0; i < numoffiles; i++)
        {
          printf("# ");
          for (j = 0; j < numoffiles; j++)
          {
            printf("%f\t", shulen[i][j]);
          }
          printf("\n");
        }
      }
      if (!had_err && gc_contents != NULL)
      {
        double *ln_n_fac;

        ln_n_fac = gt_get_ln_n_fac(arguments->max_ln_n_fac);
        for (i = 0; i < numoffiles; i++)
        {
          for (j = i+1; j < numoffiles; j++)
          {
            double query_gc, query_shulen;
            unsigned long subject_len;
            if (gt_double_smaller_double(shulen[i][j],
                                         shulen[j][i]))
            { /* S=j Q=i */
              query_gc = gc_contents[i];
              query_shulen = shulen[i][j];
              subject_len = filelength[j];
            } else
            {
              if (gt_double_smaller_double(shulen[j][i],
                                           shulen[i][j]))
              { /* S=i Q=j */
                query_gc = gc_contents[j];
                query_shulen = shulen[j][i];
                subject_len = filelength[i];
              } else
              {
                if (gt_double_smaller_double(fabs(gc_contents[i]-0.5),
                                             fabs(gc_contents[j]-0.5)))
                { /* S=i Q=j XXX check this if right*/
                  query_gc = gc_contents[j];
                  query_shulen = shulen[j][i];
                  subject_len = filelength[i];
                } else
                  query_gc = gc_contents[i];
                  query_shulen = shulen[i][j];
                  subject_len = filelength[j];
                { /* S=j Q=i */
                }
              }
            }

            shulen[i][j] =
              gt_divergence(DEFAULT_E,
                            DEFAULT_T,
                            DEFAULT_M,
                            query_shulen,
                            subject_len,
                            query_gc,
                            ln_n_fac,
                            arguments->max_ln_n_fac);
            shulen[j][i] = shulen[i][j];
          }
        }
        gt_free(ln_n_fac);
      }
      gt_logger_log(logger, "table of divergences");
      if (!had_err && gt_logger_enabled(logger))
      {
        for (i = 0; i < numoffiles; i++)
        {
          printf("# ");
          for (j = 0; j < numoffiles; j++)
          {
            /*XXX*/
            printf("%f\t", shulen[i][j]);
          }
          printf("\n");
        }
      }
      printf("Table of Kr\n");
      if (!had_err)
      {
        for (i = 0; i < numoffiles; i++)
        {
          for (j = 0; j < numoffiles; j++)
          {
            if ( i == j )
              printf("0\t\t");
            else
              printf("%f\t", gt_calculateKr(shulen[i][j]));
          }
          printf("\n");
        }
      }
      gt_free(filelength);
      gt_array2dim_delete(shulen);
    }
  }

/*XXX*/

  genericindex_delete(genericindexSubject);
  gt_logger_delete(logger);

  return had_err;
}

GtTool* gt_genomediff(void)
{
  return gt_tool_new(gt_genomediff_arguments_new,
                  gt_genomediff_arguments_delete,
                  gt_genomediff_option_parser_new,
                  gt_genomediff_arguments_check,
                  gt_genomediff_runner);
}