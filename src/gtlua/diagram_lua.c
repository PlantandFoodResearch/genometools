/*
  Copyright (c) 2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
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

#ifndef WITHOUT_CAIRO

#include "lauxlib.h"
#include "annotationsketch/canvas.h"
#include "annotationsketch/diagram.h"
#include "annotationsketch/feature_index.h"
#include "annotationsketch/luastyle.h"
#include "extended/luahelper.h"
#include "gtlua/canvas_lua.h"
#include "gtlua/diagram_lua.h"
#include "gtlua/feature_index_lua.h"
#include "gtlua/genome_node_lua.h"
#include "gtlua/range_lua.h"

static int diagram_lua_new(lua_State *L)
{
  GT_Diagram **diagram;
  GT_FeatureIndex **feature_index;
  GtRange *range;
  const char *seqid;
  GtStyle *style;
  /* get feature index */
  feature_index = check_feature_index(L, 1);
  /* get seqid */
  seqid = luaL_checkstring(L, 2);
  luaL_argcheck(L, gt_feature_index_has_seqid(*feature_index, seqid),
                2, "feature index does not contain the given sequence id");
  /* get range */
  range = check_range(L, 3);
  /* create diagram */
  style = lua_get_style_from_registry(L);
  diagram = lua_newuserdata(L, sizeof (GT_Diagram*));
  assert(diagram);
  *diagram = gt_diagram_new(*feature_index, seqid, range, style);
  luaL_getmetatable(L, DIAGRAM_METATABLE);
  lua_setmetatable(L, -2);
  return 1;
}

static GtArray* genome_node_table_to_array(lua_State *L)
{
  lua_Integer i = 1;
  GtArray *nodes;
  GT_GenomeNode **gn;
  const char *msg;
  bool error;
  /* make sure we got a table as first argument */
  luaL_checktype(L, 1, LUA_TTABLE);
  /* traverse table and save the ranges */
  nodes = gt_array_new(sizeof (GT_GenomeNode*));
  lua_pushinteger(L, i);
  lua_gettable(L, 1);
  while (!lua_isnil(L, -1)) {
    error = false;
    gn = lua_touserdata(L, -1);
    if (gn && lua_getmetatable(L, -1)) {
      lua_getfield(L, LUA_REGISTRYINDEX, GENOME_NODE_METATABLE);
      if (lua_rawequal(L, -1, -2)) {
        lua_pop(L, 2); /* remove both metatables */
        gt_array_add(nodes, *gn);
      }
      else
        error = true;
    }
    else
      error = true;
    if (error) {
      /* we have a non-GenomeNode in the table */
      msg = lua_pushfstring(L, "expected %s as type of table entry %d",
                            GENOME_NODE_METATABLE, i);
      gt_array_delete(nodes);
      lua_error(L);
    }
    i++;
    lua_pop(L, 1); /* pop last result */
    lua_pushinteger(L, i);
    lua_gettable(L, 1);
  }
  return nodes;
}

static int diagram_lua_new_from_array(lua_State *L)
{
  GT_Diagram **diagram;
  GtArray *nodes;
  GtRange range;
  GtStyle *style;
  /* get array */
  nodes = genome_node_table_to_array(L);
  /* get range */
  range.start = luaL_checklong(L, 2);
  range.end   = luaL_checklong(L, 3);
  luaL_argcheck(L, range.start > 0, 2, "must be > 0");
  luaL_argcheck(L, range.end > 0, 3, "must be > 0");
  luaL_argcheck(L, range.start <= range.end, 2, "must be <= endpos");
  /* create diagram */
  style = lua_get_style_from_registry(L);
  diagram = lua_newuserdata(L, sizeof (GT_Diagram*));
  assert(diagram);
  *diagram = gt_diagram_new_from_array(nodes, &range, style);
  luaL_getmetatable(L, DIAGRAM_METATABLE);
  lua_setmetatable(L, -2);
  gt_array_delete(nodes);
  return 1;
}

static int diagram_lua_sketch(lua_State *L)
{
  GT_Diagram **diagram;
  GtCanvas **canvas;
  diagram = check_diagram(L,1);
  canvas = check_canvas(L,2);
  return gt_diagram_sketch(*diagram, *canvas);
}

static int diagram_lua_delete(lua_State *L)
{
  GT_Diagram **diagram;
  diagram = check_diagram(L, 1);
  gt_diagram_delete(*diagram);
  return 0;
}

static const struct luaL_Reg diagram_lib_f [] = {
  { "diagram_new", diagram_lua_new },
  { "diagram_new_from_array", diagram_lua_new_from_array },
  { NULL, NULL }
};

static const struct luaL_Reg diagram_lib_m [] = {
  { "sketch",      diagram_lua_sketch },
  { NULL, NULL }
};

int gt_lua_open_diagram(lua_State *L)
{
  assert(L);
  luaL_newmetatable(L, DIAGRAM_METATABLE);
  /* metatable.__index = metatable */
  lua_pushvalue(L, -1); /* duplicate the metatable */
  lua_setfield(L, -2, "__index");
  /* set its _gc field */
  lua_pushstring(L, "__gc");
  lua_pushcfunction(L, diagram_lua_delete);
  lua_settable(L, -3);
  /* register functions */
  luaL_register(L, NULL, diagram_lib_m);
  luaL_register(L, "gt", diagram_lib_f);
  return 1;
}

#endif
