/*
  Copyright (c) 2007 Sascha Steinbiss <ssteinbiss@zbh.uni-hamburg.de>
  Copyright (c) 2007 Malte Mader <mmader@zbh.uni-hamburg.de>
  Copyright (c) 2007 Christin Schaerfer <cschaerfer@zbh.uni-hamburg.de>
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

#include "core/cstr.h"
#include "core/ensure.h"
#include "core/getbasename.h"
#include "core/hashmap.h"
#include "core/ma.h"
#include "core/str.h"
#include "core/undef.h"
#include "core/unused.h"
#include "extended/feature_type_factory_builtin.h"
#include "extended/genome_node.h"
#include "extended/genome_feature.h"
#include "extended/genome_feature_type.h"
#include "annotationsketch/canvas.h"
#include "annotationsketch/diagram.h"
#include "annotationsketch/feature_index.h"
#include "annotationsketch/line_breaker_captions.h"
#include "annotationsketch/line_breaker_bases.h"
#include "annotationsketch/track.h"

/* used to separate a filename from the type in a track name */
#define FILENAME_TYPE_SEPARATOR  '|'

struct Diagram {
  /* Tracks indexed by track keys */
  Hashmap *tracks;
  /* GT_Block lists indexed by track keys */
  Hashmap *blocks;
  /* Reverse lookup structure (per node) */
  Hashmap *nodeinfo;
  /* Cache tables for configuration data */
  Hashmap *collapsingtypes, *caption_display_status;
  int nof_tracks;
  Style *style;
  Range range;
};

/* holds a GT_Block with associated type */
typedef struct {
  GenomeFeatureType *gft;
  GT_Block *block;
} GT_BlockTuple;

/* a node in the reverse lookup structure used for collapsing */
typedef struct {
  GenomeNode *parent;
  Array *blocktuples;
} NodeInfoElement;

typedef struct {
  GenomeNode *parent;
  Diagram *diagram;
} NodeTraverseInfo;

typedef struct {
  GT_Canvas *canvas;
  Diagram *dia;
} TrackTraverseInfo;

static GT_BlockTuple* blocktuple_new(GenomeFeatureType *gft, GT_Block *block)
{
  GT_BlockTuple *bt;
  assert(block);
  bt = ma_malloc(sizeof (GT_BlockTuple));
  bt->gft = gft;
  bt->block = block;
  return bt;
}

static NodeInfoElement* get_or_create_node_info(Diagram *d, GenomeNode *node)
{
  NodeInfoElement *ni;
  assert(d && node);
  ni = hashmap_get(d->nodeinfo, node);
  if (ni == NULL) {
    NodeInfoElement *new_ni = ma_malloc(sizeof (NodeInfoElement));
    new_ni->blocktuples = array_new(sizeof (GT_BlockTuple*));
    hashmap_add(d->nodeinfo, node, new_ni);
    ni = new_ni;
  }
  return ni;
}

static GT_Block* find_block_for_type(NodeInfoElement* ni,
                                     GenomeFeatureType *gft)
{
  GT_Block *block = NULL;
  unsigned long i;
  assert(ni);
  for (i = 0; i < array_size(ni->blocktuples); i++) {
    GT_BlockTuple *bt = *(GT_BlockTuple**) array_get(ni->blocktuples, i);
    if (bt->gft == gft) {
      block = bt->block;
      break;
    }
  }
  return block;
}

static const char* get_node_name_or_id(GenomeNode *gn)
{
  const char *ret;
  if (!gn) return NULL;
  if (!(ret = genome_feature_get_attribute(gn, "Name"))) {
    if (!(ret = genome_feature_get_attribute(gn, "ID")))
      ret = NULL;
  }
  return ret;
}

static bool get_caption_display_status(Diagram *d, GenomeFeatureType *gft)
{
  assert(d && gft);
  bool *status;

  status = (bool*) hashmap_get(d->caption_display_status, gft);
  if (!status)
  {
    unsigned long threshold;
    double tmp;
    status = ma_malloc(sizeof (bool*));
    if (!style_get_bool(d->style, "format", "show_block_captions",
                       status, NULL))
      *status = true;
    if (*status)
    {
      if (style_get_num(d->style, genome_feature_type_get_cstr(gft),
                         "max_capt_show_width", &tmp, NULL))
        threshold = tmp;
      else
        threshold = UNDEF_ULONG;
      if (threshold == UNDEF_ULONG)
        *status = true;
      else
        *status = (range_length(d->range) <= threshold);
    }
    hashmap_add(d->caption_display_status, gft, status);
  }
  return *status;
}

static void add_to_current(Diagram *d, GenomeNode *node, GenomeNode *parent)
{
  NodeInfoElement *ni;
  GT_Block *block;
  GT_BlockTuple *bt;
  Str *caption = NULL;
  const char *nnid_p = NULL, *nnid_n = NULL;
  assert(d && node);

  /* Lookup node info and set itself as parent */
  ni = get_or_create_node_info(d, node);
  ni->parent = node;
  /* create new GT_Block tuple and add to node info */
  block = gt_block_new_from_node(node);
  /* assign block caption */

  caption = str_new();
  if (!style_get_str(d->style,
                     genome_feature_type_get_cstr(
                         genome_feature_get_type((GenomeFeature*) node)),
                     "block_caption",
                     caption,
                     node))
  {
    nnid_p = get_node_name_or_id(parent);
    nnid_n = get_node_name_or_id(node);
    if ((nnid_p || nnid_n) && get_caption_display_status(d,
                  genome_feature_get_type((GenomeFeature*) node)))
    {
      if (parent) {
        if (genome_node_has_children(parent))
          str_append_cstr(caption, nnid_p);
        else
          str_append_cstr(caption, "-");
        str_append_cstr(caption, "/");
      }
      if (nnid_n)
        str_append_cstr(caption, nnid_n);
    }
  }
  gt_block_set_caption(block, caption);
  /* insert node into block */
  gt_block_insert_element(block, node);
  bt = blocktuple_new(genome_feature_get_type((GenomeFeature*) node), block);
  array_add(ni->blocktuples, bt);
}

static void add_to_parent(Diagram *d, GenomeNode *node, GenomeNode* parent)
{
  GT_Block *block = NULL;
  NodeInfoElement *par_ni, *ni;
  const char *nnid_p = NULL, *nnid_n = NULL;

  assert(d && node);

  if (!parent) return;

  par_ni = get_or_create_node_info(d, parent);
  ni = get_or_create_node_info(d, node);
  ni->parent = parent;

  /* try to find the right block to insert */
  block = find_block_for_type(par_ni,
                              genome_feature_get_type((GenomeFeature*) node));
  /* no fitting block was found, create a new one */
  if (block == NULL) {
    GT_BlockTuple *bt;
    Str *caption = NULL;
    block = gt_block_new_from_node(parent);
    /* assign block caption */
    nnid_p = get_node_name_or_id(parent);
    nnid_n = get_node_name_or_id(node);
    if ((nnid_p || nnid_n) && get_caption_display_status(d,
                  genome_feature_get_type((GenomeFeature*) node)))
    {
      caption = str_new_cstr("");
      if (parent) {
        if (genome_node_has_children(parent))
          str_append_cstr(caption, nnid_p);
        else
          str_append_cstr(caption, "-");
        str_append_cstr(caption, "/");
      }
      if (nnid_n)
        str_append_cstr(caption, nnid_n);
    }
    gt_block_set_caption(block, caption);
    /* add block to nodeinfo */
    bt = blocktuple_new(genome_feature_get_type((GenomeFeature*) node), block);
    array_add(par_ni->blocktuples, bt);
  }
  /* now we have a block to insert into */
  gt_block_insert_element(block, node);
}

static void add_recursive(Diagram *d, GenomeNode *node,
                          GenomeNode* parent, GenomeNode *original_node)
{
  NodeInfoElement *ni;

  assert(d && node && original_node);
  if (!parent) return;

  ni = get_or_create_node_info(d, node);

  /* end of recursion, insert into target block */
  if (parent == node) {
    GT_Block *block ;
    GT_BlockTuple *bt;
    /* try to find the right block to insert */
    block = find_block_for_type(ni,
                                genome_feature_get_type((GenomeFeature*) node));
    if (block == NULL) {
      block = gt_block_new_from_node(node);
      bt = blocktuple_new(genome_feature_get_type((GenomeFeature*) node),
                          block);
      array_add(ni->blocktuples, bt);
    }
    gt_block_insert_element(block, original_node);
  }
  else {
    /* not at target type block yet, set up reverse entry and follow */
    NodeInfoElement *parent_ni;
    /* set up reverse entry */
    ni->parent = parent;
    /* recursively call with parent node and its parent */
    parent_ni = hashmap_get(d->nodeinfo, parent);
    if (parent_ni)
      add_recursive(d, parent, parent_ni->parent, original_node);
  }
}

static void process_node(Diagram *d, GenomeNode *node, GenomeNode *parent)
{
  Range elem_range;
  bool *collapse, do_not_overlap=false;
  const char *feature_type = NULL, *parent_gft = NULL;
  double tmp;
  unsigned long max_show_width = UNDEF_ULONG,
                par_max_show_width = UNDEF_ULONG;

  assert(d && node);

  feature_type = genome_feature_type_get_cstr(
                        genome_feature_get_type((GenomeFeature*) node));
  if (parent)
    parent_gft = genome_feature_type_get_cstr(
                        genome_feature_get_type((GenomeFeature*) parent));

  /* discard elements that do not overlap with visible range */
  elem_range = genome_node_get_range(node);
  if (!range_overlap(d->range, elem_range))
    return;

  /* get maximal view widths in nucleotides to show this type */
  if (style_get_num(d->style, feature_type, "max_show_width", &tmp, NULL))
    max_show_width = tmp;
  else
    max_show_width = UNDEF_ULONG;
  if (parent)
  {
    if (style_get_num(d->style, parent_gft, "max_show_width", &tmp, NULL))
    par_max_show_width = tmp;
  else
    par_max_show_width = UNDEF_ULONG;

  }
  /* check if this type is to be displayed */
  if (max_show_width != UNDEF_ULONG && range_length(d->range) > max_show_width)
    return;
  if (parent && par_max_show_width != UNDEF_ULONG
        && range_length(d->range) > par_max_show_width)
    parent = NULL;

  /* check if this is a collapsing type, cache result */
  if ((collapse = (bool*) hashmap_get(d->collapsingtypes,
                                        feature_type)) == NULL)
  {
    collapse = ma_malloc(sizeof (bool));
    if (!style_get_bool(d->style, feature_type, "collapse_to_parent",
                        collapse, NULL))
      *collapse = false;
    hashmap_add(d->collapsingtypes, (char*) feature_type, collapse);
  }

  /* check if direct children overlap */
  if (parent)
    do_not_overlap =
      genome_node_direct_children_do_not_overlap_st(parent, node);

  /* decide how to continue: */
  if (*collapse && parent) {
    /* collapsing features recursively search their target blocks */
    add_recursive(d, node, parent, node);
  }
  else if (do_not_overlap
            && genome_node_number_of_children(parent) > 1)
  {
    /* group non-overlapping child nodes of a non-collapsing type by parent */
    add_to_parent(d, node, parent);
  }
  else {
    /* nodes that belong into their own track and block */
    add_to_current(d, node, parent);
  }

  /* we can now assume that this node has been processed into the reverse
     lookup structure */
  assert(hashmap_get(d->nodeinfo, node));
}

static int diagram_add_tracklines(UNUSED void *key, void *value, void *data,
                                  UNUSED Error *err)
{
  TracklineInfo *add = (TracklineInfo*) data;
  add->total_lines += track_get_number_of_lines((Track*) value);
  add->total_captionlines += track_get_number_of_lines_with_captions(
                                                               (Track*) value);
  return 0;
}

static int visit_child(GenomeNode* gn, void* genome_node_children, Error* e)
{
  NodeTraverseInfo* genome_node_info;
  genome_node_info = (NodeTraverseInfo*) genome_node_children;
  int had_err;
  if (genome_node_has_children(gn))
  {
    GenomeNode *oldparent = genome_node_info->parent;
    process_node(genome_node_info->diagram, gn, genome_node_info->parent);
    genome_node_info->parent = gn;
    had_err = genome_node_traverse_direct_children(gn, genome_node_info,
                                                   visit_child, e);
    assert(!had_err); /* visit_child() is sane */
    genome_node_info->parent = oldparent;
  }
  else
    process_node(genome_node_info->diagram, gn, genome_node_info->parent);
  return 0;
}

static Str* track_key_new(const char *filename, GenomeFeatureType *type)
{
  Str *track_key;
  track_key = str_new_cstr(filename);
  str_append_char(track_key, FILENAME_TYPE_SEPARATOR);
  str_append_cstr(track_key, genome_feature_type_get_cstr(type));
  return track_key;
}

/* Create lists of all GT_Blocks in the diagram. */
static int collect_blocks(UNUSED void *key, void *value, void *data,
                          UNUSED Error *err)
{
  NodeInfoElement *ni = (NodeInfoElement*) value;
  Diagram *diagram = (Diagram*) data;
  unsigned long i = 0;

  for (i = 0; i < array_size(ni->blocktuples); i++) {
    Array *list;
    GT_BlockTuple *bt = *(GT_BlockTuple**) array_get(ni->blocktuples, i);
    list = (Array*) hashmap_get(diagram->blocks, bt->gft);
    if (!list)
    {
      list = array_new(sizeof (GT_Block*));
      hashmap_add(diagram->blocks, bt->gft, list);
    }
    assert(list);
    array_add(list, bt->block);
    ma_free(bt);
  }
  array_delete(ni->blocktuples);
  ma_free(ni);
  return 0;
}

/* Traverse a genome node graph with depth first search. */
static void traverse_genome_nodes(GenomeNode *gn, void *genome_node_children)
{
  NodeTraverseInfo* genome_node_info;
  int had_err;
  assert(genome_node_children);
  genome_node_info = (NodeTraverseInfo*) genome_node_children;
  genome_node_info->parent = gn;
  /* handle root nodes */
  process_node(genome_node_info->diagram, gn, NULL);
  if (genome_node_has_children(gn)) {
    had_err = genome_node_traverse_direct_children(gn, genome_node_info,
                                                   visit_child, NULL);
    assert(!had_err); /* visit_child() is sane */
  }
}

static void diagram_build(Diagram *diagram, Array *features)
{
  unsigned long i = 0;
  int had_err;
  NodeTraverseInfo genome_node_children;
  genome_node_children.diagram = diagram;

  /* initialise caches */
  diagram->collapsingtypes = hashmap_new(HASH_STRING, NULL, ma_free_func);
  diagram->caption_display_status = hashmap_new(HASH_DIRECT,
                                                  NULL, ma_free_func);

  /* do node traversal for each root feature */
  for (i = 0; i < array_size(features); i++) {
    GenomeNode *current_root = *(GenomeNode**) array_get(features,i);
    traverse_genome_nodes(current_root, &genome_node_children);
  }
  /* collect blocks from nodeinfo structures and create the tracks */
  had_err = hashmap_foreach_ordered(diagram->nodeinfo, collect_blocks,
                                      diagram, (Compare) genome_node_cmp, NULL);
  assert(!had_err); /* collect_blocks() is sane */

  /* clear caches */
  hashmap_delete(diagram->collapsingtypes);
  hashmap_delete(diagram->caption_display_status);
}

static int blocklist_delete(void *value)
{
  unsigned long i;
  Array *a = (Array*) value;
  for (i=0;i<array_size(a);i++)
    gt_block_delete(*(GT_Block**) array_get(a, i));
  array_delete(a);
  return 0;
}

static Diagram* diagram_new_generic(Array *features, const Range *range,
                                    Style *style)
{
  Diagram *diagram;
  diagram = ma_malloc(sizeof (Diagram));
  diagram->tracks = hashmap_new(HASH_STRING, ma_free_func,
                                (FreeFunc) track_delete);
  diagram->blocks = hashmap_new(HASH_DIRECT, NULL,
                                  (FreeFunc) blocklist_delete);
  diagram->nodeinfo = hashmap_new(HASH_DIRECT, NULL, NULL);
  diagram->nof_tracks = 0;
  diagram->style = style;
  diagram->range = *range;
  diagram_build(diagram, features);
  return diagram;
}

Diagram* diagram_new(GT_FeatureIndex *fi, const char *seqid, const Range *range,
                     Style *style)
{
  Diagram *diagram;
  int had_err = 0;
  Array *features = array_new(sizeof (GenomeNode*));
  assert(features && seqid && range && style);
  had_err = gt_feature_index_get_features_for_range(fi, features, seqid, *range,
                                                 NULL);
  assert(!had_err); /* <fi> must contain <seqid> */
  diagram = diagram_new_generic(features, range, style);
  array_delete(features);
  return diagram;
}

Diagram* diagram_new_from_array(Array *features, const Range *range,
                                Style *style)
{
  assert(features && range && style);
  return diagram_new_generic(features, range, style);
}

Range diagram_get_range(Diagram* diagram)
{
  assert(diagram);
  return diagram->range;
}

void diagram_set_style(Diagram *diagram, Style *style)
{
  assert(diagram && style);
  diagram->style = style;
}

Hashmap* diagram_get_tracks(const Diagram *diagram)
{
  assert(diagram);
  return diagram->tracks;
}

void diagram_get_lineinfo(const Diagram *diagram, TracklineInfo *tli)
{
  int had_err;
  assert(diagram);
  had_err = hashmap_foreach(diagram->tracks, diagram_add_tracklines,
                              tli, NULL);
  assert(!had_err); /* diagram_add_tracklines() is sane */
}

int diagram_get_number_of_tracks(const Diagram *diagram)
{
  assert(diagram);
  return diagram->nof_tracks;
}

static int blocklist_block_compare(const void *item1, const void *item2)
{
  assert(item1 && item2);
  return gt_block_compare(*(GT_Block**) item1, *(GT_Block**) item2);
}

static int layout_tracks(void *key, void *value, void *data,
                         UNUSED Error *err)
{
  unsigned long i, max;
  Track *track;
  TrackTraverseInfo *tti = (TrackTraverseInfo*) data;
  GenomeFeatureType *gft = (GenomeFeatureType*) key;
  Array *list = (Array*) value;
  char *filename;
  Str *track_key;
  const char *type;
  GT_Block *block;
  bool split;
  double tmp;
  assert(gft && list);

  /* to get a deterministic layout, we sort the GT_Blocks for each type */
  array_sort(list, blocklist_block_compare);
  /* we take the basename of the filename to have nicer output in the
     generated graphic. this might lead to ``collapsed'' tracks, if two files
     with different paths have the same basename. */
  block = *(GT_Block**) array_get(list, 0);
  filename = getbasename(genome_node_get_filename(
                                        gt_block_get_top_level_feature(block)));
  track_key = track_key_new(filename, gft);
  ma_free(filename);
  type = genome_feature_type_get_cstr(gft);

  if (!style_get_bool(tti->dia->style, "format", "split_lines", &split, NULL))
    split = true;
  if (split)
    if (!style_get_bool(tti->dia->style, type, "split_lines", &split, NULL))
      split = true;
  if (style_get_num(tti->dia->style, type, "max_num_lines", &tmp, NULL))
    max = tmp;
  else
    max = 50;

  /* For now, use the captions line breaker */
  track = track_new(track_key, max, split,
                    line_breaker_captions_new(tti->canvas));
  tti->dia->nof_tracks++;
  for (i=0;i<array_size(list);i++)
  {
    block = *(GT_Block**) array_get(list, i);
    track_insert_block(track, block);
  }
  hashmap_add(tti->dia->tracks, cstr_dup(str_get(track_key)), track);
  str_delete(track_key);
  return 0;
}

static int render_tracks(UNUSED void *key, void *value, void *data,
                     UNUSED Error *err)
{
  TrackTraverseInfo *tti = (TrackTraverseInfo*) data;
  UNUSED Track *track = (Track*) value;
  int had_err = 0;
  assert(tti && track);
  had_err = track_sketch((Track*) value, tti->canvas);
  return had_err;
}

int diagram_sketch(Diagram *dia, GT_Canvas *canvas)
{
  int had_err = 0;
  TrackTraverseInfo tti;
  tti.dia = dia;
  tti.canvas = canvas;
  gt_canvas_visit_diagram_pre(canvas, dia);
  hashmap_reset(dia->tracks);
  dia->nof_tracks = 0;
  (void) hashmap_foreach(dia->blocks, layout_tracks, &tti, NULL);
  gt_canvas_visit_diagram_post(canvas, dia);
  had_err = hashmap_foreach_in_key_order(dia->tracks, render_tracks,
                                         &tti, NULL);

  return had_err;
}

int diagram_unit_test(Error *err)
{
  FeatureTypeFactory *feature_type_factory;
  GenomeFeatureType *gene_type, *exon_type, *CDS_type;
  GenomeNode *gn1, *gn2, *ex1, *ex2, *ex3, *cds1;
  GT_FeatureIndex *fi;
  Range r1, r2, r3, r4, r5, dr1, rs;
  Str *seqid1, *seqid2, *track_key;
  SequenceRegion *sr1, *sr2;
  int had_err=0;
  Style *sty = NULL;
  Diagram *dia = NULL, *dia2 = NULL, *dia3 = NULL;
  Array *features;
  GT_Canvas *canvas = NULL;
  error_check(err);

  feature_type_factory = feature_type_factory_builtin_new();
  gene_type = feature_type_factory_create_gft(feature_type_factory, gft_gene);
  exon_type = feature_type_factory_create_gft(feature_type_factory, gft_exon);
  CDS_type = feature_type_factory_create_gft(feature_type_factory, gft_CDS);

  /* generating some ranges */
  r1.start=100UL; r1.end=1000UL;
  r2.start=100UL; r2.end=300UL;
  r3.start=500UL; r3.end=1000UL;
  r4.start=600UL; r4.end=1200UL;
  r5.start=600UL; r5.end=1000UL;
  rs.start=100UL; rs.end=1200UL;

  /* generating sequence IDs */
  seqid1 = str_new_cstr("test1");
  seqid2 = str_new_cstr("test2");

  sr1 = (SequenceRegion*) sequence_region_new(seqid1, rs);
  sr2 = (SequenceRegion*) sequence_region_new(seqid2, rs);

  gn1 = genome_feature_new(seqid1, gene_type, r1, STRAND_UNKNOWN);

  gn2 = genome_feature_new(seqid2, gene_type, r4, STRAND_UNKNOWN);

  ex1 = genome_feature_new(seqid1, exon_type, r2, STRAND_UNKNOWN);

  ex2 = genome_feature_new(seqid1, exon_type, r3, STRAND_UNKNOWN);

  ex3 = genome_feature_new(seqid2, exon_type, r4, STRAND_UNKNOWN);

  cds1 = genome_feature_new(seqid2, CDS_type, r5, STRAND_UNKNOWN);

  /* determine the structure of our feature tree */
  genome_node_is_part_of_genome_node(gn1, ex1);
  genome_node_is_part_of_genome_node(gn1, ex2);
  genome_node_is_part_of_genome_node(gn2, ex3);
  genome_node_is_part_of_genome_node(gn2, cds1);

  /* create a new feature index on which we can perform some tests */
  fi = gt_feature_index_new();

  /* add features to every sequence region */
  gt_feature_index_add_genome_feature(fi, (GenomeFeature*) gn1);
  gt_feature_index_add_genome_feature(fi, (GenomeFeature*) gn2);

  /* set the Range for the diagram */
  dr1.start = 400UL;
  dr1.end   = 900UL;

  /* create a style object */
  if (!had_err) {
    if (!(sty = style_new(false, err)))
      had_err = -1;
  }

  /* create a diagram object and test it */
  if (!had_err)
    dia = diagram_new(fi, "test1", &dr1, sty);

  ensure(had_err, dia->style);
  ensure(had_err, dia->range.start == 400UL);
  ensure(had_err, dia->range.end == 900UL);

  if (!had_err)
  {
    canvas = gt_canvas_new(sty, GRAPHICS_PNG, 600, NULL);
    diagram_sketch(dia, canvas);
  }

  if (!had_err &&
      !style_get_bool(dia->style, "gene", "collapse_to_parent", false, NULL))
  {
    track_key = track_key_new("generated", gene_type);
    ensure(had_err, hashmap_get(dia->tracks, str_get(track_key)));
    str_delete(track_key);
  }

  if (!had_err &&
      !style_get_bool(dia->style, "exon", "collapse_to_parent", false, NULL))
  {
    track_key = track_key_new("generated", exon_type);
    ensure(had_err, hashmap_get(dia->tracks, str_get(track_key)));
    str_delete(track_key);
  }
  ensure(had_err, range_compare(diagram_get_range(dia),dr1) == 0);

  /* create a diagram object and test it */
  if (!had_err) {
    dia2 = diagram_new(fi, "test2", &dr1, sty);
    ensure(had_err, dia->range.start == 400UL);
    ensure(had_err, dia->range.end == 900UL);
  }

  if (!had_err &&
      !style_get_bool(dia2->style, "gene", "collapse_to_parent", false, NULL))
  {
    diagram_sketch(dia2, canvas);
    track_key = track_key_new("generated", gene_type);
    ensure(had_err, hashmap_get(dia2->tracks, str_get(track_key)));
    str_delete(track_key);
  }

  if (!had_err &&
      !style_get_bool(dia2->style, "exon", "collapse_to_parent", false, NULL))
  {
    track_key = track_key_new("generated", exon_type);
    ensure(had_err, hashmap_get(dia2->tracks, str_get(track_key)));
    str_delete(track_key);
  }

  if (!had_err &&
      !style_get_bool(dia2->style, "CDS", "collapse_to_parent", false, NULL))
  {
    track_key = track_key_new("generated", CDS_type);
    ensure(had_err, hashmap_get(dia2->tracks, str_get(track_key)));
    str_delete(track_key);
  }
  ensure(had_err, range_compare(diagram_get_range(dia),dr1) == 0);

  features = array_new(sizeof (GenomeNode*));
  array_add(features, gn1);
  array_add(features, gn2);
  dia3 = diagram_new_from_array(features, &rs, sty);

  ensure(had_err, dia3->style);

  if (!had_err &&
      !style_get_bool(dia3->style, "gene", "collapse_to_parent", false, NULL))
  {
    diagram_sketch(dia3, canvas);
    track_key = track_key_new("generated", gene_type);
    ensure(had_err, hashmap_get(dia3->tracks, str_get(track_key)));
    str_delete(track_key);
  }

  if (!had_err &&
      !style_get_bool(dia3->style, "exon", "collapse_to_parent", false, NULL))
  {
    track_key = track_key_new("generated", exon_type);
    ensure(had_err, hashmap_get(dia3->tracks, str_get(track_key)));
    str_delete(track_key);
  }
  ensure(had_err, range_compare(diagram_get_range(dia3),rs) == 0);

  /* delete all generated objects */
  style_delete(sty);
  array_delete(features);
  diagram_delete(dia);
  diagram_delete(dia2);
  diagram_delete(dia3);
  gt_canvas_delete(canvas);
  gt_feature_index_delete(fi);
  genome_node_rec_delete(gn1);
  genome_node_rec_delete(gn2);
  genome_node_rec_delete((GenomeNode*) sr1);
  genome_node_rec_delete((GenomeNode*) sr2);
  str_delete(seqid1);
  str_delete(seqid2);
  feature_type_factory_delete(feature_type_factory);

  return had_err;
}

void diagram_delete(Diagram *diagram)
{
  if (!diagram) return;
  hashmap_delete(diagram->tracks);
  hashmap_delete(diagram->blocks);
  hashmap_delete(diagram->nodeinfo);
  ma_free(diagram);
}
