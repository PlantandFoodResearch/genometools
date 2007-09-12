/*
  Copyright (c) 2005-2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2005-2007 Center for Bioinformatics, University of Hamburg

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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <libgtcore/cstr.h>
#include <libgtcore/genfile.h>
#include <libgtcore/xansi.h>
#include <libgtcore/xbzlib.h>
#include <libgtcore/xzlib.h>

struct GenFile {
  GenFileMode mode;
  union {
    FILE *file;
    gzFile gzfile;
    BZFILE *bzfile;
  } fileptr;
  char *orig_path,
       *orig_mode;
};

GenFileMode genfilemode_determine(const char *path)
{
  assert(path);
  if (!strcmp(".gz", path + strlen(path) - 3))
    return GFM_GZIP;
  if (!strcmp(".bz2", path + strlen(path) - 4))
    return GFM_BZIP2;
  return GFM_UNCOMPRESSED;
}

const char* genfilemode_suffix(GenFileMode mode)
{
  switch (mode) {
    case GFM_UNCOMPRESSED:
      return "";
    case GFM_GZIP:
      return ".gz";
    case GFM_BZIP2:
      return ".bz2";
    default:
      assert(0);
      return "";
  }
  /* due do warning on solaris:
     warning: control reaches end of non-void function
  */
  return "";
}

size_t genfile_basename_length(const char *path)
{
  size_t path_length;
  assert(path);
  path_length = strlen(path);
  if (!strcmp(".gz", path + path_length - 3))
    return path_length - 3;
  if (!strcmp(".bz2", path + path_length - 4))
    return path_length - 4;
  return path_length;
}

GenFile* genfile_open(GenFileMode genfilemode, const char *path,
                      const char *mode, Env *env)
{
  GenFile *genfile;
  assert(path && mode);
  genfile = env_ma_calloc(env, 1, sizeof (GenFile));
  genfile->mode = genfilemode;
  switch (genfilemode) {
    case GFM_UNCOMPRESSED:
      genfile->fileptr.file = env_fa_fopen(env, path, mode);
      if (!genfile->fileptr.file) {
        genfile_delete(genfile, env);
        return NULL;
      }
      break;
    case GFM_GZIP:
      genfile->fileptr.gzfile = env_fa_gzopen(env, path, mode);
      if (!genfile->fileptr.gzfile) {
        genfile_delete(genfile, env);
        return NULL;
      }
      break;
    case GFM_BZIP2:
      genfile->fileptr.bzfile = env_fa_bzopen(env, path, mode);
      if (!genfile->fileptr.bzfile) {
        genfile_delete(genfile, env);
        return NULL;
      }
      genfile->orig_path = cstr_dup(path, env);
      genfile->orig_mode = cstr_dup(path, env);
      break;
    default: assert(0);
  }
  return genfile;
}

GenFile* genfile_xopen(GenFileMode genfilemode, const char *path,
                       const char *mode, Env *env)
{
  GenFile *genfile;
  assert(path && mode);
  genfile = env_ma_calloc(env, 1, sizeof (GenFile));
  genfile->mode = genfilemode;
  switch (genfilemode) {
    case GFM_UNCOMPRESSED:
      genfile->fileptr.file = env_fa_xfopen(env, path, mode);
      break;
    case GFM_GZIP:
      genfile->fileptr.gzfile = env_fa_xgzopen(env, path, mode);
      break;
    case GFM_BZIP2:
      genfile->fileptr.bzfile = env_fa_xbzopen(env, path, mode);
      genfile->orig_path = cstr_dup(path, env);
      genfile->orig_mode = cstr_dup(path, env);
      break;
    default: assert(0);
  }
  return genfile;
}

GenFile* genfile_new(FILE *fp, Env *env)
{
  GenFile *genfile;
  env_error_check(env);
  assert(fp);
  genfile = env_ma_calloc(env, 1, sizeof (GenFile));
  genfile->mode = GFM_UNCOMPRESSED;
  genfile->fileptr.file = fp;
  return genfile;
}

GenFileMode genfile_mode(GenFile *genfile)
{
  assert(genfile);
  return genfile->mode;
}

static int bzgetc(BZFILE *bzfile)
{
  char c;
  return BZ2_bzread(bzfile, &c, 1) == 1 ? (int) c : -1;
}

int genfile_getc(GenFile *genfile)
{
  int c = -1;
  if (genfile) {
    switch (genfile->mode) {
      case GFM_UNCOMPRESSED:
        c = fgetc(genfile->fileptr.file);
        break;
      case GFM_GZIP:
        c = gzgetc(genfile->fileptr.gzfile);
        break;
      case GFM_BZIP2:
        c = bzgetc(genfile->fileptr.bzfile);
        break;
      default: assert(0);
    }
  }
  else
    c = fgetc(stdin);
  return c;
}

int genfile_putc(int c, GenFile *genfile)
{
  int rval = -1;
  if (!genfile)
    return putc(c, stdout);
  switch (genfile->mode) {
    case GFM_UNCOMPRESSED:
      rval = putc(c, genfile->fileptr.file);
      break;
    case GFM_GZIP:
      rval = gzputc(genfile->fileptr.gzfile, c);
      break;
    case GFM_BZIP2:
      rval = bzputc(c, genfile->fileptr.bzfile);
      break;
    default: assert(0);
  }
  return rval;
}

static int vgzprintf(gzFile file, const char *format, va_list va)
{
  char buf[BUFSIZ];
  int len;
  len = vsnprintf(buf, sizeof (buf), format, va);
  assert(len <= BUFSIZ);
  return gzwrite(file, buf, (unsigned) len);
}

static int vbzprintf(BZFILE *file, const char *format, va_list va)
{
  char buf[BUFSIZ];
  int len;
  len = vsnprintf(buf, sizeof (buf), format, va);
  assert(len <= BUFSIZ);
  return BZ2_bzwrite(file, buf, len);
}

static int xvprintf(GenFile *genfile, const char *format, va_list va)
{
  int rval = -1;

  if (!genfile) /* implies stdout */
    rval = vfprintf(stdout, format, va);
  else {
    switch (genfile->mode) {
      case GFM_UNCOMPRESSED:
        rval = vfprintf(genfile->fileptr.file, format, va);
        break;
      case GFM_GZIP:
        rval = vgzprintf(genfile->fileptr.gzfile, format, va);
        break;
      case GFM_BZIP2:
        rval = vbzprintf(genfile->fileptr.bzfile, format, va);
        break;
      default: assert(0);
    }
  }
  return rval;
}

void genfile_xprintf(GenFile *genfile, const char *format, ...)
{
  va_list va;
  va_start(va, format);
  if (xvprintf(genfile, format, va) < 0) {
    fprintf(stderr, "genfile_xprintf(): xvprintf() returned negative value\n");
    exit(EXIT_FAILURE);
  }
  va_end(va);
}

void genfile_xfputc(int c, GenFile *genfile)
{
  if (!genfile)
    return xfputc(c, stdout);
  switch (genfile->mode) {
    case GFM_UNCOMPRESSED:
      xfputc(c, genfile->fileptr.file);
      break;
    case GFM_GZIP:
      xgzfputc(c, genfile->fileptr.gzfile);
      break;
    case GFM_BZIP2:
      xbzfputc(c, genfile->fileptr.bzfile);
      break;
    default: assert(0);
  }
}

void genfile_xfputs(const char *str, GenFile *genfile)
{
  if (!genfile)
    return xfputs(str, stdout);
  switch (genfile->mode) {
    case GFM_UNCOMPRESSED:
      xfputs(str, genfile->fileptr.file);
      break;
    case GFM_GZIP:
      xgzfputs(str, genfile->fileptr.gzfile);
      break;
    case GFM_BZIP2:
      xbzfputs(str, genfile->fileptr.bzfile);
      break;
    default: assert(0);
  }
}

int genfile_xread(GenFile *genfile, void *buf, size_t nbytes)
{
  int rval = -1;
  if (genfile) {
    switch (genfile->mode) {
      case GFM_UNCOMPRESSED:
        rval = xfread(buf, 1, nbytes, genfile->fileptr.file);
        break;
      case GFM_GZIP:
        rval = xgzread(genfile->fileptr.gzfile, buf, nbytes);
        break;
      case GFM_BZIP2:
        rval = xbzread(genfile->fileptr.bzfile, buf, nbytes);
        break;
      default: assert(0);
    }
  }
  else
    rval = xfread(buf, 1, nbytes, stdin);
  return rval;
}

void genfile_xwrite(GenFile *genfile, void *buf, size_t nbytes)
{
  if (!genfile) {
    xfwrite(buf, 1, nbytes, stdout);
    return;
  }
  switch (genfile->mode) {
    case GFM_UNCOMPRESSED:
      xfwrite(buf, 1, nbytes, genfile->fileptr.file);
      break;
    case GFM_GZIP:
      xgzwrite(genfile->fileptr.gzfile, buf, nbytes);
      break;
    case GFM_BZIP2:
      xbzwrite(genfile->fileptr.bzfile, buf, nbytes);
      break;
    default: assert(0);
  }
}

void genfile_xrewind(GenFile *genfile)
{
  assert(genfile);
  switch (genfile->mode) {
    case GFM_UNCOMPRESSED:
      rewind(genfile->fileptr.file);
      break;
    case GFM_GZIP:
      xgzrewind(genfile->fileptr.gzfile);
      break;
    case GFM_BZIP2:
      xbzrewind(&genfile->fileptr.bzfile, genfile->orig_path,
                genfile->orig_mode);
      break;
    default: assert(0);
  }
}

void genfile_delete(GenFile *genfile, Env *env)
{
  if (!genfile) return;
  env_ma_free(genfile->orig_path, env);
  env_ma_free(genfile->orig_mode, env);
  env_ma_free(genfile, env);
}

/* the following function can only fail, if no error is set. This makes sure,
   that it can be used safely after an error has been set (i.e., the error is
   always propagated upwards). */
void genfile_xclose(GenFile *genfile, Env *env)
{
  if (!genfile) return;
  switch (genfile->mode) {
    case GFM_UNCOMPRESSED:
      if (env_error_is_set(env))
        env_fa_fclose(genfile->fileptr.file, env);
      else
        env_fa_xfclose(genfile->fileptr.file, env);
      break;
    case GFM_GZIP:
      if (env_error_is_set(env))
        env_fa_gzclose(genfile->fileptr.gzfile, env);
      else
        env_fa_xgzclose(genfile->fileptr.gzfile, env);
      break;
    case GFM_BZIP2:
      if (env_error_is_set(env))
        env_fa_bzclose(genfile->fileptr.bzfile, env);
      else
        env_fa_xbzclose(genfile->fileptr.bzfile, env);
      break;
    default: assert(0);
  }
  genfile_delete(genfile, env);
}
