/* Copyright (C) 2015 Cotton Seed
   
   This file is part of arachne-pnr.  Arachne-pnr is free software;
   you can redistribute it and/or modify it under the terms of the GNU
   General Public License version 2 as published by the Free Software
   Foundation.
   
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>. */

#include "util.hh"
#include "casting.hh"
#include "place.hh"
#include "netlist.hh"
#include "chipdb.hh"
#include "location.hh"
#include "configuration.hh"
#include "pcf.hh"
#include "carry.hh"
#include "bitvector.hh"
#include "ullmanset.hh"
#include "hashmap.hh"

#include <iomanip>
#include <vector>
#include <set>
#include <random>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <cmath>
#include <ctime>

class Placer
{
public:
  random_generator &rg;
  
  const ChipDB *chipdb;
  const Package &package;
  
  std::vector<int> logic_columns;
  std::vector<int> logic_tiles,
    ramt_tiles;
  
  Design *d;
  CarryChains &chains;
  const Constraints &constraints;
  const std::map<Instance *, uint8_t, IdLess> &gb_inst_gc;
  Configuration &conf;
  
  std::map<Instance *, int, IdLess> placement;
  
  Models models;
  Model *top;
  
  std::vector<Net *> nets;
  std::map<Net *, int, IdLess> net_idx;
  
  int n_gates;
  BasedVector<Instance *, 1> gates;
  std::map<Instance *, int, IdLess> gate_idx;
  BasedBitVector<1> chained;
  
  BasedVector<int, 1> gate_clk, gate_sr, gate_cen, gate_latch;
  
  BasedVector<std::vector<int>, 1> gate_local_np;
  UllmanSet tmp_local_np;
  
  BitVector net_global;
  
  std::vector<std::vector<int>> cell_type_free_cells;
  
  std::vector<int> free_io_cells;
  std::vector<int> free_gates;
  
  BasedVector<int, 1> gate_chain;
  
  CellType gate_cell_type(int g);
  int gate_random_cell(int g);
  std::pair<Location, bool> chain_random_loc(int c);
  
  void move_gate(int g, int cell);
  void move_chain(int c, const Location &new_loc);
  
  int tile_n_pos(int t);
  
  std::vector<std::vector<int>> net_gates;
  BasedVector<std::vector<int>, 1> gate_nets;
  
  int diameter;
  double temp;
  bool improved;
  int n_move;
  int n_accept;
  
  UllmanSet changed_tiles;
  std::vector<std::pair<int, int>> restore_cell;
  std::vector<std::tuple<int, int, int>> restore_chain;
  std::vector<std::pair<int, int>> restore_net_length;
  UllmanSet recompute;
  
  void save_set(int cell, int g);
  
  void save_set_chain(int c, int x, int start);
  int save_recompute_wire_length();
  void restore();
  void discard();
  void accept_or_restore();
  
  std::vector<int> chain_x, chain_start;
  
  BasedVector<int, 1> gate_cell;
  BasedVector<int, 1> cell_gate;
  
  std::vector<int> net_length;
  
  bool valid(int t);
  
  int wire_length() const;
  int compute_net_length(int w);
  unsigned top_port_io_gate(const std::string &net_name);
  
  void place_initial();
  void configure();
  
#ifndef NDEBUG
  void check();
#endif
  
public:
  Placer(random_generator &rg_,
	 const ChipDB *chipdb,
	 const Package &package_,
	 Design *d,
	 CarryChains &chains_,
	 const Constraints &constraints_,
	 const std::map<Instance *, uint8_t, IdLess> &gb_inst_gc_,
	 Configuration &conf_);
  
  std::map<Instance *, int, IdLess> place();
};

CellType
Placer::gate_cell_type(int g)
{
  Instance *inst = gates[g];
  if (models.is_lc(inst))
    return CellType::LOGIC;
  else if (models.is_io(inst))
    return CellType::IO;
  else if (models.is_gb(inst))
    return CellType::GB;
  else if (models.is_warmboot(inst))
    return CellType::WARMBOOT;
  else
    {
      assert(models.is_ramX(inst));
      return CellType::RAM;
    }
}

int
Placer::gate_random_cell(int g)
{
  CellType ct = gate_cell_type(g);
  if (ct == CellType::LOGIC)
    {
      int cell = gate_cell[g];
      int t = chipdb->cell_location[cell].tile();
      int x = chipdb->tile_x(t),
	y = chipdb->tile_y(t);
      
    L:
      int new_x = rg.random_int(std::max(0, x - diameter),
				std::min(chipdb->width-1, x + diameter)),
	new_y = rg.random_int(std::max(0, y - diameter),
			      std::min(chipdb->height-1, y + diameter));
      int new_t = chipdb->tile(new_x, new_y);
      if (chipdb->tile_type[new_t] != TileType::LOGIC)
	goto L;
      
      Location loc(new_t, rg.random_int(0, 7));
      return chipdb->loc_cell(loc);
    }
  else
    {
      int ct_idx = cell_type_idx(ct);
      return random_element(cell_type_free_cells[ct_idx], rg);
    }
}

std::pair<Location, bool>
Placer::chain_random_loc(int c)
{
  const auto &v = chains.chains[c];
  int nt = (v.size() + 7) / 8;
  
  int new_x = random_element(logic_columns, rg);
  int new_start = random_int(1, chipdb->height - 2 - (nt - 1), rg);
  int new_end = new_start + nt - 1;
  
  for (unsigned e = 0; e < chains.chains.size(); ++e)
    {
      if (chain_x[e] != new_x)  // including self
	continue;
      
      int e_nt = (chains.chains[e].size() + 7) / 8;
      int e_start = chain_start[e],
	e_end = e_start + e_nt - 1;
      if ((new_start > e_start && new_start <= e_end)
	  || (new_end >= e_start && new_end < e_end))
	return std::make_pair(Location(), false);
    }
  
  int t = chipdb->tile(new_x, new_start);
  return std::make_pair(Location(t, 0), true);
}

void
Placer::move_gate(int g, int new_cell)
{
  assert(g);
  int cell = gate_cell[g]; // copy
  if (new_cell == cell)
    return;
  
  int new_g = cell_gate[new_cell];
  
  save_set(new_cell, g);
  save_set(cell, new_g);
}

void
Placer::move_chain(int c, const Location &new_base)
{
  assert(new_base.pos() == 0);
  
  int nt = (chains.chains[c].size() + 7) / 8;
  
  int x = chain_x[c],
    start = chain_start[c];
  
  int new_t = new_base.tile();
  int new_x = chipdb->tile_x(new_t),
    new_start = chipdb->tile_y(new_t);
  if (new_x == x
      && new_start == start)
    return;
  
  for (int i = 0; i < nt; ++i)
    for (unsigned k = 0; k < 8; ++k)
      {
	Location loc(chipdb->tile(x, start + i), k),
	  new_loc(chipdb->tile(new_x, new_start + i), k);
	
	int cell = chipdb->loc_cell(loc),
	  new_cell = chipdb->loc_cell(new_loc);
	
	unsigned g = cell_gate[cell],
	  new_g = cell_gate[new_cell];
	if (g)
	  move_gate(g, new_cell);
	if (new_g)
	  move_gate(new_g, cell);
      }
}

int
Placer::tile_n_pos(int t)
{
  switch(chipdb->tile_type[t])
    {
    case TileType::LOGIC:
      return 8;
    case TileType::IO:
      return 2;
    case TileType::RAMT:
      return 1;
    default:
      abort();
      return 0;
    }
}

void
Placer::save_set(int cell, int g)
{
  const Location &loc = chipdb->cell_location[cell];
  int t = loc.tile();
  
  restore_cell.push_back(std::make_pair(cell, cell_gate[cell]));
  if (g)
    {
      for (int w : gate_nets[g])
	recompute.insert(w);
      gate_cell[g] = cell;
      
      int c = gate_chain[g];
      if (c != -1)
	{
	  int x = chipdb->tile_x(t),
	    y = chipdb->tile_y(t);
	  save_set_chain(c, x, y);
	}
    }
  
  cell_gate[cell] = g;
  
  changed_tiles.insert(t);
}

void
Placer::save_set_chain(int c, int x, int start)
{
  restore_chain.push_back(std::make_tuple(c, chain_x[c], chain_start[c]));
  chain_x[c] = x;
  chain_start[c] = start;
}

int
Placer::save_recompute_wire_length()
{
  int delta = 0;
  for (int i = 0; i < (int)recompute.size(); ++i)
    {
      int w = recompute.ith(i);
      int new_length = compute_net_length(w),
	old_length = net_length.at(w);
      restore_net_length.push_back(std::make_pair(w, old_length));
      net_length[w] = new_length;
      delta += (new_length - old_length);
    }
  return delta;
}

void
Placer::restore()
{
  for (const auto &p : restore_cell)
    {
      cell_gate[p.first] = p.second;
      if (p.second)
	gate_cell[p.second] = p.first;
    }
  for (const auto &p : restore_net_length)
    net_length[p.first] = p.second;
  for (const auto &t : restore_chain)
    {
      int e, x, start;
      std::tie(e, x, start) = t;
      chain_x[e] = x;
      chain_start[e] = start;
    }
}

void
Placer::discard()
{
  changed_tiles.clear();
  restore_cell.clear();
  restore_chain.clear();
  restore_net_length.clear();
  recompute.clear();
}

bool
Placer::valid(int t)
{
  int x = chipdb->tile_x(t),
    y = chipdb->tile_y(t);
  if (chipdb->tile_type[t] == TileType::LOGIC)
    {
      int global_clk = 0,
	global_sr = 0,
	global_cen = 0;
      int neg_clk = -1;
      tmp_local_np.clear();
      for (int q = 0; q < 8; q ++)
	{
	  Location loc(t, q);
	  int cell = chipdb->loc_cell(loc);
	  int g = cell_gate[cell];
	  if (g)
	    {
	      Instance *inst = gates[g];
	      
	      int clk = gate_clk[g],
		sr = gate_sr[g],
		cen = gate_cen[g];
	      
	      if (!global_clk)
		global_clk = clk;
	      else if (global_clk != clk)
		return false;
	      
	      if (!global_sr)
		global_sr = sr;
	      else if (global_sr != sr)
		return false;
	      
	      if (!global_cen)
		global_cen = cen;
	      else if (global_cen != cen)
		return false;
	      
	      int g_neg_clk = (int)inst->get_param("NEG_CLK").get_bit(0);
	      if (neg_clk == -1)
		neg_clk = g_neg_clk;
	      else if (neg_clk != g_neg_clk)
		return false;
	      
	      for (int np : gate_local_np[g])
		tmp_local_np.insert(np ^ (q & 1));
	    }
	}
      
      if (global_clk
	  && !net_global[global_clk])
	tmp_local_np.insert(global_clk << 1);
      if (global_sr
	  && !net_global[global_sr])
	tmp_local_np.insert(global_sr << 1);
      if (global_cen
	  && !net_global[global_cen])
	tmp_local_np.insert(global_cen << 1);
      
      if (tmp_local_np.size() > 29)
	return false;
    }
  else if (chipdb->tile_type[t] == TileType::IO)
    {
      int b = chipdb->tile_bank(t);
      
      int latch = 0;
      for (int cell : chipdb->bank_cells[b])
	{
	  int g = cell_gate[cell];
	  if (g)
	    {
	      int n = gate_latch[g];
	      if (latch)
		{
		  if (latch != n)
		    return false;
		}
	      else
		latch = n;
	    }
	}
      
      Location loc0(t, 0),
	loc1(t, 1);
      int cell0 = chipdb->loc_cell(loc0),
	cell1 = chipdb->loc_cell(loc1);
      int g0 = cell0 ? cell_gate[cell0] : 0,
	g1 = cell1 ? cell_gate[cell1] : 0;
      if (g0)
	{
	  Instance *inst0 = gates[g0];
	  if (inst0->get_param("IO_STANDARD").as_string() == "SB_LVDS_INPUT")
	    {
	      if (b != 3)
		return false;
	      if (g1)
		return false;
	    }
	}
      if (g1)
	{
	  Instance *inst0 = gates[g1];
	  if (inst0->get_param("IO_STANDARD").as_string() == "SB_LVDS_INPUT")
	    return false;
	}
      
      if (g0 && g1)
	{
	  Instance *inst0 = gates[g0],
	    *inst1 = gates[g1];
	  if (inst0->get_param("NEG_TRIGGER").get_bit(0)
	      != inst1->get_param("NEG_TRIGGER").get_bit(0))
	    return false;
	}
      
      Location loc2(t, 2);
      int cell2 = chipdb->loc_cell(loc2);
      int g2 = cell2 ? cell_gate[cell2] : 0;
      if (g2)
	{
	  Instance *inst = gates[g2];
	  int gc = gb_inst_gc.at(inst);
	  int global = chipdb->gbufin.at(std::make_pair(x, y));
	  if (! (gc & (1 << global)))
	    return false;
	}
    }
  else
    assert(chipdb->tile_type[t] == TileType::RAMT);
  
  return true;
}

void
Placer::accept_or_restore()
{
  int delta;
  
  for (int i = 0; i < (int)changed_tiles.size(); ++i)
    {
      int t = changed_tiles.ith(i);
      if (!valid(t))
	goto L;
    }
  
  delta = save_recompute_wire_length();
  
  // check();
  
  ++n_move;
  if (delta < 0
      || (temp > 1e-6
	  && rg.random_real(0.0, 1.0) <= exp(-delta/temp)))
    {
      if (delta < 0)
	{
	  // std::cout << "delta " << delta << "\n";
	  improved = true;
	}
      ++n_accept;
    }
  else
    {
    L:
      restore();
    }
  discard();
  
  // check();
}

unsigned
Placer::top_port_io_gate(const std::string &net_name)
{
  Port *p = top->find_port(net_name);
  assert(p);
  
  Port *p2 = p->connection_other_port();
  
  Instance *inst = cast<Instance>(p2->node());
  assert(models.is_io(inst));
  
  return gate_idx.at(inst);
}

#ifndef NDEBUG
void
Placer::check()
{
  for (Instance *inst : top->instances())
    {
      int g = gate_idx.at(inst);
      assert(cell_gate[gate_cell[g]] == g);
    }
  for (int i = 1; i <= chipdb->n_cells; ++i)
    {
      int g = cell_gate[i];
      if (g)
	assert(gate_cell[g] == i);
    }
  
  for (unsigned c = 0; c < chains.chains.size(); ++c)
    {
      const auto &v = chains.chains[c];
      for (unsigned i = 0; i < v.size(); ++i)
	{
	  Location loc (chipdb->tile(chain_x[c],
				     chain_start[c] + i / 8),
			i % 8);
	  int g = gate_idx.at(v[i]);
	  int cell = chipdb->loc_cell(loc);
	  int cell_g = cell_gate[cell];
	  assert(g == cell_g);
	}
      
      int nt = (v.size() + 7) / 8;
      int start = chain_start[c];
      assert(start + nt - 1 <= chipdb->height - 2);
    }
  for (int w = 1; w < (int)nets.size(); ++w) // skip 0, nullptr
    assert(net_length[w] == compute_net_length(w));
}
#endif

int
Placer::compute_net_length(int w)
{
  if (net_global[w]
      || net_gates[w].empty())
    return 0;
  
  const std::vector<int> &w_gates = net_gates[w];
  
  int g0 = w_gates[0];
  int cell0 = gate_cell[g0];
  const Location &loc0 = chipdb->cell_location[cell0];
  int t0 = loc0.tile();
  int x_min = chipdb->tile_x(t0),
    x_max = chipdb->tile_x(t0),
    y_min = chipdb->tile_y(t0),
    y_max = chipdb->tile_y(t0);
  
  for (int i = 1; i < (int)w_gates.size(); ++i)
    {
      int g = w_gates[i];
      int cell = gate_cell[g];
      const Location &loc = chipdb->cell_location[cell];
      int t = loc.tile();
      int x = chipdb->tile_x(t),
	y = chipdb->tile_y(t);
      x_min = std::min(x_min, x);
      x_max = std::max(x_max, x);
      y_min = std::min(y_min, y);
      y_max = std::max(y_max, y);
    }
  
  assert(x_min <= x_max && y_min <= y_max);
  return (x_max - x_min) + (y_max - y_min);
}

int
Placer::wire_length() const
{
  int length = 0;
  for (int ell : net_length)
    length += ell;
  return length;
}

Placer::Placer(random_generator &rg_,
	       const ChipDB *cdb,
	       const Package &package_,
	       Design *d_,
	       CarryChains &chains_,
	       const Constraints &constraints_,
	       const std::map<Instance *, uint8_t, IdLess> &gb_inst_gc_,
	       Configuration &conf_)
  : rg(rg_),
    chipdb(cdb),
    package(package_),
    d(d_),
    chains(chains_),
    constraints(constraints_),
    gb_inst_gc(gb_inst_gc_),
    conf(conf_),
    models(d),
    top(d->top()),
    diameter(std::max(chipdb->width,
		      chipdb->height)),
    temp(10000.0),
    changed_tiles(chipdb->n_tiles),
    cell_gate(chipdb->n_cells, 0)
{
  for (int i = 0; i < chipdb->width; ++i)
    {
      int t = chipdb->tile(i, 1);
      if (chipdb->tile_type[t] == TileType::LOGIC)
	logic_columns.push_back(i);
    }
  for (int i = 0; i < chipdb->n_tiles; ++i)
    {
      switch(chipdb->tile_type[i])
	{
	case TileType::LOGIC:
	  logic_tiles.push_back(i);
	  break;
	case TileType::RAMT:
	  ramt_tiles.push_back(i);
	  break;
	default:
	  break;
	}
    }

  std::tie(nets, net_idx) = top->index_nets();
  int n_nets = nets.size();
  
  net_global.resize(n_nets);
  
  net_length.resize(n_nets);
  net_gates.resize(n_nets);
  recompute.resize(n_nets);
  
  std::tie(gates, gate_idx) = top->index_instances();
  n_gates = gates.size();
  
  gate_clk.resize(n_gates, 0);
  gate_sr.resize(n_gates, 0);
  gate_cen.resize(n_gates, 0);
  gate_latch.resize(n_gates, 0);
  gate_local_np.resize(n_gates);
  tmp_local_np.resize(n_nets * 2);
  gate_chain.resize(n_gates, -1);
  
  gate_cell.resize(n_gates);
  gate_nets.resize(n_gates);
  
  for (int i = 1; i <= n_gates; ++i)
    {
      Instance *inst = gates[i];
      if (models.is_lc(inst))
	{
	  Net *clk = inst->find_port("CLK")->connection();
	  if (clk)
	    gate_clk[i] = net_idx.at(clk);
	  
	  Net *sr = inst->find_port("SR")->connection();
	  if (sr)
	    gate_sr[i] = net_idx.at(sr);
	  
	  Net *cen = inst->find_port("CEN")->connection();
	  if (cen)
	    gate_cen[i] = net_idx.at(cen);
	  
	  tmp_local_np.clear();
	  for (int j = 0; j < 4; ++j)
	    {
	      Net *n = inst->find_port(fmt("I" << j))->connection();
	      if (n
		  && !n->is_constant())
		tmp_local_np.insert((net_idx.at(n) << 1) | (j & 1));
	    }
	  
	  for (int j = 0; j < (int)tmp_local_np.size(); ++j)
	    gate_local_np[i].push_back(tmp_local_np.ith(j));
	}
      else if (models.is_io(inst))
	{
	  Net *latch = inst->find_port("LATCH_INPUT_VALUE")->connection();
	  if (latch)
	    gate_cen[i] = net_idx.at(latch);
	}
      else if (models.is_gb(inst))
	{
	  Net *n = inst->find_port("GLOBAL_BUFFER_OUTPUT")->connection();
	  if (n)
	    net_global[net_idx.at(n)] = true;
	}
    }
}

void
Placer::place_initial()
{
  BasedBitVector<1> locked(n_gates);
  
  chained.resize(n_gates);
  
  // place chains
  std::vector<int> logic_column_free(logic_columns.size(), 1);
  for (unsigned i = 0; i < chains.chains.size(); ++i)
    {
      const auto &v = chains.chains[i];
      
      int gate0 = gate_idx.at(v[0]);
      assert(gate_chain[gate0] == -1);
      gate_chain[gate0] = i;
      
      int nt = (v.size() + 7) / 8;
      for (unsigned k = 0; k < logic_columns.size(); ++k)
	{
	  if (logic_column_free[k] + nt - 1 <= chipdb->height - 2)
	    {
	      int x = logic_columns[k];
	      int y = logic_column_free[k];
	      
	      for (unsigned j = 0; j < v.size(); ++j)
		{
		  Instance *inst = v[j];
		  int g = gate_idx.at(inst);
		  Location loc(chipdb->tile(x, y + j / 8),
			       j % 8);
		  int cell = chipdb->loc_cell(loc);
		  
		  assert(cell_gate[cell] == 0);
		  cell_gate[cell] = g;
		  gate_cell[g] = cell;
		  chained[g] = true;
		}
	      
	      chain_x.push_back(x);
	      chain_start.push_back(y);
	      
	      logic_column_free[k] += nt;
	      goto placed_chain;
	    }
	}
      fatal(fmt("failed to place: placed " 
		<< i
		<< " of " << chains.chains.size()
		<< " carry chains"));
      
    placed_chain:;
    }
  
  std::vector<int> cell_type_n_placed(n_cell_types, 0);
  
  int io_idx = cell_type_idx(CellType::IO);
  const std::vector<int> &io_cells_vec = chipdb->cell_type_cells[io_idx];
  std::set<int> io_cells(io_cells_vec.begin(),
			 io_cells_vec.end());
  
  std::vector<Net *> bank_latch(4, nullptr);
  for (const auto &p : constraints.net_pin_loc)
    {
      int g = top_port_io_gate(p.first);
      Instance *inst = gates[g];
      
      const Location &loc = p.second;
      int t = loc.tile();
      int b = chipdb->tile_bank(t);
      
      Net *latch = inst->find_port("LATCH_INPUT_VALUE")->connection();
      if (latch)
	{
	  if (bank_latch[b])
	    {
	      if (bank_latch[b] != latch)
		fatal(fmt("pcf error: multiple LATCH_INPUT_VALUE drivers in bank " << b));
	    }
	  else
	    bank_latch[b] = latch;
	}
      
      if (inst->get_param("IO_STANDARD").as_string() == "SB_LVDS_INPUT"
	  && b != 3)
	fatal(fmt("pcf error: LVDS port `" << p.first << "' not in bank 3\n"));
      
      Location loc_other(t,
			 loc.pos() ? 0 : 1);
      int cell_other = chipdb->loc_cell(loc_other);
      if (cell_other)
	{
	  int g_other = cell_gate[cell_other];
	  if (g_other)
	    {
	      Instance *inst_other = gates[g_other];
	      if (inst->get_param("NEG_TRIGGER").get_bit(0)
		  != inst_other->get_param("NEG_TRIGGER").get_bit(0))
		{
		  int x = chipdb->tile_x(t),
		    y = chipdb->tile_y(t);
		  fatal(fmt("pcf error: incompatible NEG_TRIGGER parameters in PIO at (" 
			    << x << ", " << y << ")"));
		}
	    }
	}
      
      int c = chipdb->loc_cell(loc);
      
      assert(cell_gate[c] == 0);
      cell_gate[c] = g;
      gate_cell[g] = c;
      
      assert(contains(io_cells, c));
      io_cells.erase(c);
      
      locked[g] = true;
      ++cell_type_n_placed[io_idx];
      
      assert(valid(t));
    }
  
  cell_type_free_cells = chipdb->cell_type_cells;
  cell_type_free_cells[io_idx] = std::vector<int>(io_cells.begin(),
						  io_cells.end());
  
  std::vector<std::vector<int>> cell_type_empty_cells = cell_type_free_cells;
  for (int i = 0; i < n_cell_types; ++i)
    for (int j = 0; j < (int)cell_type_empty_cells[i].size(); ++j)
      {
	int c = cell_type_empty_cells[i][j];
	if (cell_gate[c] != 0)
	  pop(cell_type_empty_cells[i], j);
      }
  
  std::vector<int> cell_type_n_gates(n_cell_types, 0);
  for (int i = 1; i <= n_gates; ++i)
    ++cell_type_n_gates[cell_type_idx(gate_cell_type(i))];
  
  std::set<std::pair<uint8_t, int>> io_q;
  
  for (int i = 1; i <= n_gates; ++i)
    {
      if (locked[i]
	  || chained[i])
	continue;
      
      free_gates.push_back(i);
      CellType ct = gate_cell_type(i);
      if (ct == CellType::GB)
	{
	  Instance *inst = gates[i];
	  io_q.insert(std::make_pair(gb_inst_gc.at(inst), i));
	}
      else
	{
	  int ct_idx = cell_type_idx(ct);
	  auto &v = cell_type_empty_cells[ct_idx];
	  
	  for (int j = 0; j < (int)v.size(); ++j)
	    {
	      int c = v[j];
	  
	      assert(cell_gate[c] == 0);
	      cell_gate[c] = i;
	      gate_cell[i] = c;
	      
	      if (ct != CellType::WARMBOOT &&
		  !valid(chipdb->cell_location[c].tile()))
		cell_gate[c] = 0;
	      else
		{
		  ++cell_type_n_placed[ct_idx];
		  pop(v, j);
		  goto placed_gate;
		}
	    }
	  
	  fatal(fmt("failed to place: placed "
		    << cell_type_n_placed[ct_idx]
		    << " " << cell_type_name(ct) << "s of " << cell_type_n_gates[ct_idx]
		    << " / " << chipdb->cell_type_cells[ct_idx].size()));
	placed_gate:;
	}
    }
  
  // place gb
  int gb_idx = cell_type_idx(CellType::GB);
  while (!io_q.empty())
    {
      std::pair<uint8_t, int> p = *io_q.begin();
      io_q.erase(io_q.begin());
      
      int i = p.second;
      auto &v = cell_type_empty_cells[gb_idx];
      
      for (unsigned j = 0; j < v.size(); ++j)
	{
	  int c = v[j];
	  
	  assert(cell_gate[c] == 0);
	  cell_gate[c] = i;
	  gate_cell[i] = c;
	  
	  if (!valid(chipdb->cell_location[c].tile()))
	    cell_gate[c] = 0;
	  else
	    {
	      ++cell_type_n_placed[gb_idx];
	      pop(v, j);
	      goto placed_gb;
	    }
	}
      fatal(fmt("failed to place: placed "
		<< cell_type_n_placed[gb_idx]
		<< " GBs of " << cell_type_n_gates[gb_idx]
		<< " / " << chipdb->cell_type_cells[gb_idx].size()));
    placed_gb:;
    }
  
  for (int g = 1; g <= n_gates; ++g)
    {
      Instance *inst = gates[g];
      for (const auto &p : inst->ports())
	{
	  Net *n = p.second->connection();
	  if (n
	      && !n->is_constant())  // constants are not routed
	    {
	      int w = net_idx.at(n);
	      net_gates[w].push_back(g);
	      gate_nets[g].push_back(w);
	    }
	}
    }
  
  for (int w = 0; w < (int)nets.size(); ++w)
    net_length[w] = compute_net_length(w);
}

void
Placer::configure()
{
  for (int g = 1; g <= n_gates; g ++)
    {
      Instance *inst = gates[g];
      int cell = gate_cell[g];
      const Location &loc = chipdb->cell_location[cell];
      
      if (models.is_warmboot(inst)) {
	placement[inst] = cell;
	continue;
      }

      int t = loc.tile();
      const auto &func_cbits = chipdb->tile_nonrouting_cbits.at(chipdb->tile_type[t]);
      
      if (models.is_lc(inst))
	{
	  BitVector lut_init = inst->get_param("LUT_INIT").as_bits();
	  lut_init.resize(16);
	  
	  static std::vector<int> lut_perm = {
	    4, 14, 15, 5, 6, 16, 17, 7, 3, 13, 12, 2, 1, 11, 10, 0,
	  };
	  
	  const auto &cbits = func_cbits.at(fmt("LC_" << loc.pos()));
	  for (int i = 0; i < 16; ++i)
	    conf.set_cbit(CBit(t,
			       cbits[lut_perm[i]].row,
			       cbits[lut_perm[i]].col),
			  lut_init[i]);
	  
	  bool carry_enable = inst->get_param("CARRY_ENABLE").get_bit(0);
	  if (carry_enable)
	    {
	      conf.set_cbit(CBit(t,
				 cbits[8].row,
				 cbits[8].col), (bool)carry_enable);
	      if (loc.pos() == 0)
		{
		  Net *n = inst->find_port("CIN")->connection();
		  if (n && n->is_constant())
		    {
		      const CBit &carryinset_cbit = func_cbits.at("CarryInSet")[0];
		      conf.set_cbit(CBit(t,
					 carryinset_cbit.row,
					 carryinset_cbit.col), 
				    n->constant() == Value::ONE);
		    }
		}
	    }
	  
	  bool dff_enable = inst->get_param("DFF_ENABLE").get_bit(0);
	  conf.set_cbit(CBit(t,
			     cbits[9].row,
			     cbits[9].col), dff_enable);
	  
	  if (dff_enable)
	    {
	      bool neg_clk = inst->get_param("NEG_CLK").get_bit(0);
	      const CBit &neg_clk_cbit = func_cbits.at("NegClk")[0];
	      conf.set_cbit(CBit(t,
				 neg_clk_cbit.row,
				 neg_clk_cbit.col),
			    (bool)neg_clk);
	      
	      bool set_noreset = inst->get_param("SET_NORESET").get_bit(0);
	      conf.set_cbit(CBit(t,
				 cbits[18].row,
				 cbits[18].col), (bool)set_noreset);
	      
	      bool async_sr = inst->get_param("ASYNC_SR").get_bit(0);
	      conf.set_cbit(CBit(t,
				 cbits[19].row,
				 cbits[19].col), (bool)async_sr);
	    }
	}
      else if (models.is_io(inst))
	{
	  const BitVector &pin_type = inst->get_param("PIN_TYPE").as_bits();
	  for (int i = 0; i < 6; ++i)
	    {
	      const CBit &cbit = func_cbits.at(fmt("IOB_" << loc.pos() << ".PINTYPE_" << i))[0];
	      conf.set_cbit(CBit(t, 
				 cbit.row, 
				 cbit.col),
			    pin_type[i]);
	    }
	  
	  const auto &negclk_cbits = func_cbits.at("NegClk");
	  bool neg_trigger = inst->get_param("NEG_TRIGGER").get_bit(0);
	  for (int i = 0; i <= 1; ++i)
	    conf.set_cbit(CBit(t,
			       negclk_cbits[i].row,
			       negclk_cbits[i].col),
			  neg_trigger);
	}
      else if (models.is_gb(inst))
	;
      else
	{
	  assert(models.is_ramX(inst));
	  
	  BitVector wm = inst->get_param("WRITE_MODE").as_bits(),
	    rm = inst->get_param("READ_MODE").as_bits();
	  wm.resize(2);
	  rm.resize(2);
	  
	  // powerup active low, don't set
	  const auto &ramb_func_cbits = chipdb->tile_nonrouting_cbits.at(TileType::RAMB);
	  const CBit &cbit0 = func_cbits.at("RamConfig.CBIT_0")[0],
	    &cbit1 = func_cbits.at("RamConfig.CBIT_1")[0],
	    &cbit2 = func_cbits.at("RamConfig.CBIT_2")[0],
	    &cbit3 = func_cbits.at("RamConfig.CBIT_3")[0],
	    &negclk = func_cbits.at("NegClk")[0],
	    &ramb_negclk = ramb_func_cbits.at("NegClk")[0];
	  
	  conf.set_cbit(CBit(t,
			     cbit0.row,
			     cbit0.col),
			wm[0]);
	  conf.set_cbit(CBit(t,
			     cbit1.row,
			     cbit1.col),
			wm[1]);
	  conf.set_cbit(CBit(t,
			     cbit2.row,
			     cbit2.col),
			rm[0]);
	  conf.set_cbit(CBit(t,
			     cbit3.row,
			     cbit3.col),
			rm[1]);
	  
	  if (models.is_ramnr(inst)
	      || models.is_ramnrnw(inst))
	    conf.set_cbit(CBit(t,
			       negclk.row,
			       negclk.col),
			  true);
	  
	  if (models.is_ramnw(inst)
	      || models.is_ramnrnw(inst))
	    conf.set_cbit(CBit(chipdb->ramt_ramb_tile(t),
			       ramb_negclk.row,
			       ramb_negclk.col),
			  true);
	}
      
      placement[inst] = cell;
    }
  
  // set IoCtrl configuration bits
  {
    const auto &func_cbits = chipdb->tile_nonrouting_cbits.at(TileType::IO);
    const CBit &ie_0 = func_cbits.at("IoCtrl.IE_0")[0],
      &ie_1 = func_cbits.at("IoCtrl.IE_1")[0],
      &ren_0 = func_cbits.at("IoCtrl.REN_0")[0],
      &ren_1 = func_cbits.at("IoCtrl.REN_1")[0],
      &lvds = func_cbits.at("IoCtrl.LVDS")[0];
    
    for (const auto &p : package.pin_loc)
      {
	// unused io
	bool enable_input = false;
	bool pullup = true;  // default pullup
	
	const Location &loc = p.second;
	const Location &ieren_loc = chipdb->ieren.at(loc);
	int cell = chipdb->loc_cell(loc);
	int g = cell_gate[cell];
	if (g)
	  {
	    Instance *inst = gates[g];
	    
	    if (inst->find_port("D_IN_0")->connected()
		|| inst->find_port("D_IN_1")->connected())
	      enable_input = true;
	    pullup = inst->get_param("PULLUP").get_bit(0);
	    conf.set_cbit(CBit(loc.tile(),
			       lvds.row,
			       lvds.col),
			  inst->get_param("IO_STANDARD").as_string() == "SB_LVDS_INPUT");
	  }
	
	if (ieren_loc.pos() == 0)
	  {
	    conf.set_cbit(CBit(ieren_loc.tile(),
			       ren_0.row,
			       ren_0.col),
			  !pullup);  // active low
	  }
	else
	  {
	    assert(ieren_loc.pos() == 1);
	    conf.set_cbit(CBit(ieren_loc.tile(),
			       ren_1.row,
			       ren_1.col),
			  !pullup);  // active low
	  }
	if (ieren_loc.pos() == 0)
	  {
	    conf.set_cbit(CBit(ieren_loc.tile(),
			       ie_0.row,
			       ie_0.col),
			  // active low 1k, active high 8k
			  (chipdb->device == "1k"
			   ? !enable_input
			   : enable_input));
	  }
	else
	  {
	    assert(ieren_loc.pos() == 1);
	    conf.set_cbit(CBit(ieren_loc.tile(),
			       ie_1.row,
			       ie_1.col),
			  // active low 1k, active high 8k
			  (chipdb->device == "1k"
			   ? !enable_input
			   : enable_input));
	  }
      }
    
    std::set<Location> ieren_image;
    for (const auto &p : chipdb->ieren)
      extend(ieren_image, p.second);
    for (int t = 0; t < chipdb->n_tiles; ++t)
      {
	if (chipdb->tile_type[t] != TileType::IO)
	  continue;
	for (int p = 0; p <= 1; ++p)
	  {
	    bool enable_input = false;
	    bool pullup = true;  // default pullup
	    
	    Location loc(t, p);
	    if (contains(ieren_image, loc))
	      continue;
	    
	    if (loc.pos() == 0)
	      {
		conf.set_cbit(CBit(loc.tile(),
				   ren_0.row,
				   ren_0.col),
			      !pullup);  // active low
	      }
	    else
	      {
		assert(loc.pos() == 1);
		conf.set_cbit(CBit(loc.tile(),
				   ren_1.row,
				   ren_1.col),
			      !pullup);  // active low
	      }
	    if (loc.pos() == 0)
	      {
		conf.set_cbit(CBit(loc.tile(),
				   ie_0.row,
				   ie_0.col),
			      // active low 1k, active high 8k
			      (chipdb->device == "1k"
			       ? !enable_input
			       : enable_input));
	      }
	    else
	      {
		assert(loc.pos() == 1);
		conf.set_cbit(CBit(loc.tile(),
				   ie_1.row,
				   ie_1.col),
			      // active low 1k, active high 8k
			      (chipdb->device == "1k"
			       ? !enable_input
			       : enable_input));
	      }
	  }
      }
  }
  
  // set RamConfig.PowerUp configuration bit
  {
    const CBit &powerup = (chipdb->tile_nonrouting_cbits.at(TileType::RAMB)
			   .at("RamConfig.PowerUp")
			   [0]);
    for (int t : ramt_tiles)
      {
	Location loc(t,
		     0);
	int cell = chipdb->loc_cell(loc);
	int g = cell_gate[cell];
	assert(!g || models.is_ramX(gates[g]));
	conf.set_cbit(CBit(chipdb->ramt_ramb_tile(loc.tile()), // PowerUp on ramb tile
			   powerup.row,
			   powerup.col),
		      // active low
		      (chipdb->device == "1k"
		       ? !g
		       : (bool)g));
      }
  }
}

std::map<Instance *, int, IdLess>
Placer::place()
{
  place_initial();
  // check();
  
  *logs << "  initial wire length = " << wire_length() << "\n";
  
  int n_no_progress = 0;
  for (;;)
    {
      n_move = n_accept = 0;
      improved = false;
      
      for (int m = 0; m < 15; ++m)
	{
	  for (int g : free_gates)
	    {
	      int new_cell = gate_random_cell(g);	      
	      
	      int new_g = cell_gate[new_cell];
	      if (new_g && chained[new_g])
		continue;
	      
	      move_gate(g, new_cell);
	      accept_or_restore();
	      
	      // check();
	    }
      
	  for (int c = 0; c < (int)chains.chains.size(); ++c)
	    {
	      std::pair<Location, bool> new_loc = chain_random_loc(c);
	      if (new_loc.second)
		{
		  move_chain(c, new_loc.first);
		  accept_or_restore();
		}
	      
	      // check();
	    }
	}
      
      if (improved)
	{
	  n_no_progress = 0;
	  // std::cout << "improved\n";
	}
      else
	++n_no_progress;
      
      if (temp <= 1e-3
	  && n_no_progress >= 5)
	break;
      
      double Raccept = (double)n_accept / (double)n_move;
#if 0
      std::cout << "Raccept " << Raccept
		<< ", diameter = " << diameter
		<< ", temp " << temp << "\n";
#endif
      
      int M = std::max(chipdb->width,
		       chipdb->height);
      
      double upper = 0.6,
	lower = 0.4;
      
      if (Raccept >= 0.8)
	temp *= 0.5;
      else if (Raccept > upper)
	{
	  if (diameter < M)
	    ++diameter;
	  else
	    temp *= 0.9;
	}
      else if (Raccept > lower)
	temp *= 0.95;
      else
	{
	  // Raccept < 0.3
	  if (diameter > 1)
	    --diameter;
	  else
	    temp *= 0.8;
	}
    }
  
  *logs << "  final wire length = " << wire_length() << "\n";
  
  configure();
  
#if 0
  int max_demand = 0;
  for (int t = 0; t < chipdb->n_tiles; ++t)
    {
      if (chipdb->tile_type[t] != TileType::LOGIC)
	continue;
      
      std::set<std::pair<Net *, int>> demand;
      for (int q = 0; q < 8; q ++)
	{
	  Location loc(t, q);
	  int cell = chipdb->loc_cell(loc);
	  int g = cell_gate[cell];
	  if (g)
	    {
	      Instance *inst = gates[g];
	      
	      for (int i = 0; i < 4; ++i)
		{
		  Net *n = inst->find_port(fmt("I" << i))->connection();
		  if (n
		      && !n->is_constant())
		    {
		      int parity = (q + i) & 1;
		      demand.insert(std::make_pair(n, parity));
		    }
		}
	      
	      Net *clk = inst->find_port("CLK")->connection();
	      if (clk
		  && !clk->is_constant())
		{
		  int n = net_idx.at(clk);
		  if (!net_global[n])
		    demand.insert(std::make_pair(clk, 0));
		}

	      Net *cen = inst->find_port("CEN")->connection();
	      if (cen
		  && !cen->is_constant())
		{
		  int n = net_idx.at(cen);
		  if (!net_global[n])
		    demand.insert(std::make_pair(cen, 0));
		}
	      
	      Net *sr = inst->find_port("SR")->connection();
	      if (sr
		  && !sr->is_constant())
		{
		  int n = net_idx.at(sr);
		  if (!net_global[n])
		    demand.insert(std::make_pair(sr, 0));
		}
	    }
	}
      *logs << t 
	    << " " << chipdb->tile_x(t)
	    << " " << chipdb->tile_y(t)
	    << " " << demand.size() << "\n";
      if ((int)demand.size() > max_demand)
	max_demand = demand.size();
    }
  *logs << "max_demand " << max_demand << "\n";
#endif
      
#if 0
  {
    int t = chipdb->tile(17, 16);
    for (const auto &p : placement)
      {
	int cell = p.second;
	const Location &loc = chipdb->cell_location[cell];
	if (loc.tile() == t)
	  {
	    Instance *inst = p.first;
	    *logs << "LC " << inst << " " << loc.pos() << "\n";
	    *logs << "  I0 " << inst->find_port("I0")->connection()->name() << "\n";
	    *logs << "  I1 " << inst->find_port("I1")->connection()->name() << "\n";
	    *logs << "  I2 " << inst->find_port("I2")->connection()->name() << "\n";
	    *logs << "  I3 " << inst->find_port("I3")->connection()->name() << "\n";
	    if (inst->find_port("CIN")->connected())
	      *logs << "  CIN " << inst->find_port("CIN")->connection()->name() << "\n";
	    if (inst->find_port("CLK")->connected())
	      *logs << "  CLK " << inst->find_port("CLK")->connection()->name() << "\n";
	    if (inst->find_port("SR")->connected())
	      *logs << "  SR " << inst->find_port("SR")->connection()->name() << "\n";
	    if (inst->find_port("CEN")->connected())
	      *logs << "  CEN " << inst->find_port("CEN")->connection()->name() << "\n";
	  }
      }
  }
#endif
  
  int n_pio = 0,
    n_plb = 0,
    n_bram = 0;
  std::set<int> seen;
  for (int i = 1; i <= n_gates; ++i)
    {
      int cell = gate_cell[i];
      int t = chipdb->cell_location[cell].tile();
      seen.insert(t);
    }
  for (int t : seen)
    {
      if (chipdb->tile_type[t] == TileType::LOGIC)
	++n_plb;
      else if (chipdb->tile_type[t] == TileType::IO)
	++n_pio;
      else if (chipdb->tile_type[t] == TileType::RAMT)
	++n_bram;
    }
  
  *logs << "\nAfter placement:\n"
	<< "PIOs       " << n_pio << " / " << package.pin_loc.size() << "\n"
	<< "PLBs       " << n_plb << " / " << logic_tiles.size() << "\n"
	<< "BRAMs      " << n_bram << " / " << ramt_tiles.size() << "\n"
	<< "\n";
  
  return std::move(placement);
}

std::map<Instance *, int, IdLess>
place(random_generator &rg,
      const ChipDB *chipdb,
      const Package &package,
      Design *d,
      CarryChains &chains,
      const Constraints &constraints,
      const std::map<Instance *, uint8_t, IdLess> &gb_inst_gc,
      Configuration &conf)
{
  Placer placer(rg, chipdb, package, d, chains, constraints, gb_inst_gc, conf);
  
  clock_t start = clock();
  std::map<Instance *, int, IdLess> placement = placer.place();
  clock_t end = clock();
  
  *logs << "  place time "
	<< std::fixed << std::setprecision(2)
	<< (double)(end - start) / (double)CLOCKS_PER_SEC << "s\n";
  
  return std::move(placement);
}
