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

#include <iomanip>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <cmath>
#include <ctime>

class Placer
{
public:
  std::default_random_engine rg;
  
  const ChipDB *chipdb;
  const Package &package;
  
  std::vector<int> logic_columns;
  std::vector<int> logic_tiles,
    ramt_tiles;
  std::vector<Location> gb_locs;
  
  Design *d;
  CarryChains &chains;
  const Constraints &constraints;
  const std::unordered_map<Instance *, uint8_t> &gb_inst_gc;
  Configuration &conf;
  
  std::unordered_map<Instance *, Location> placement;
  
  Models models;
  Model *top;
  
  std::vector<Net *> nets;
  std::unordered_map<Net *, int> net_idx;
  
  std::vector<Instance *> gates;
  std::unordered_map<Instance *, int> gate_idx;
  BitVector chained;
  
  std::unordered_set<int> glb_nets;
  std::vector<Location> free_io_locs;
  std::vector<int> free_gates;
  std::unordered_map<int, int> gate_chain;
  
  Location gate_random_loc(int g);
  std::pair<Location, bool> chain_random_loc(int c);
  
  void move_gate(int g, const Location &new_loc);
  void move_chain(int c, const Location &new_loc);
  void move_tile(int t, int new_t);
  
  int tile_n_pos(int t);
  
  std::vector<std::vector<int>> net_gates;
  std::vector<std::vector<int>> gate_nets;
  
  double temp;
  
  std::unordered_set<int> changed_tiles;
  std::vector<std::pair<Location, int>> restore_loc;
  std::vector<std::tuple<int, int, int>> restore_chain;
  std::vector<std::pair<int, int>> restore_net_length;
  std::unordered_set<int> recompute;
  
  void save_set(const Location &loc, unsigned g);
  void save_set_chain(int c, int x, int start);
  int save_recompute_wire_length();
  void restore();
  void discard();
  void accept_or_restore();
  
  std::vector<int> chain_x, chain_start;
  
  std::vector<Location> gate_loc;
  std::unordered_map<Location, unsigned> loc_gate;
  
  std::vector<int> net_length;
  
  bool valid(int t) const;
  bool valid(const Location &loc) const;

  int wire_length() const;
  int compute_net_length(int w);
  unsigned top_port_io_gate(const std::string &net_name);
  
  void place_initial();
  void configure();
  
#ifndef NDEBUG
  void check();
#endif
  
public:
  Placer(const ChipDB *chipdb,
	 const Package &package_,
	 Design *d,
	 CarryChains &chains_,
	 const Constraints &constraints_,
	 const std::unordered_map<Instance *, uint8_t> &gb_inst_gc_,
	 Configuration &conf_);
  
  std::unordered_map<Instance *, Location> place();
};

Location
Placer::gate_random_loc(int g)
{
  Instance *inst = gates[g];
  if (models.is_lc(inst))
    {
      int t = random_element(logic_tiles, rg);
      return Location(chipdb->tile_x(t),
		      chipdb->tile_y(t),
		      random_int(0, 7, rg));
    }
  else if (models.is_io(inst))
    return random_element(free_io_locs, rg);
  else if (models.is_gb(inst))
    return random_element(gb_locs, rg);
  else
    {
      assert(models.is_ramX(inst));
      int t = random_element(ramt_tiles, rg);
      return Location(chipdb->tile_x(t),
		      chipdb->tile_y(t),
		      0);
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
  
  return std::make_pair(Location(new_x, new_start, 0), true);
}

void
Placer::move_gate(int g, const Location &new_loc)
{
  assert(g);
  Location loc = gate_loc[g]; // copy
  if (new_loc == loc)
    return;
  
  int new_g = lookup_or_default(loc_gate, new_loc, 0);
  
  save_set(new_loc, g);
  save_set(loc, new_g);
}

void
Placer::move_chain(int c, const Location &new_base)
{
  assert(new_base.pos() == 0);
  
  int nt = (chains.chains[c].size() + 7) / 8;
  
  int x = chain_x[c],
    start = chain_start[c];
  
  int new_x = new_base.x(),
    new_start = new_base.y();
  if (new_x == x
      && new_start == start)
    return;
  
  for (int i = 0; i < nt; ++i)
    for (unsigned k = 0; k < 8; ++k)
      {
	Location loc(x, start + i, k),
	  new_loc(new_x, new_start + i, k);
	unsigned g = lookup_or_default(loc_gate, loc, 0),
	  new_g = lookup_or_default(loc_gate, new_loc, 0);
	if (g)
	  move_gate(g, new_loc);
	if (new_g)
	  move_gate(new_g, loc);
      }
}

int
Placer::tile_n_pos(int t)
{
  switch(chipdb->tile_type[t])
    {
    case TileType::LOGIC_TILE:
      return 8;
    case TileType::IO_TILE:
      return 2;
    case TileType::RAMT_TILE:
      return 1;
    default:
      abort();
      return 0;
    }
}

void
Placer::move_tile(int t, int new_t)
{
  assert(chipdb->tile_type[t] == chipdb->tile_type[new_t]);
  
  int n_pos = tile_n_pos(t);
  assert(n_pos == tile_n_pos(new_t));
  for (int i = 0; i < n_pos; ++i)
    {
      Location loc(chipdb->tile_x(t),
		   chipdb->tile_y(t),
		   i);
      int g = lookup_or_default(loc_gate, loc, 0);
      
      Location new_loc(chipdb->tile_x(new_t),
		       chipdb->tile_y(new_t),
		       i);
      move_gate(g, new_loc);
    }
}

void
Placer::save_set(const Location &loc, unsigned g)
{
  restore_loc.push_back(std::make_pair(loc, lookup_or_default(loc_gate, loc, 0)));
  if (g)
    {
      for (int w : gate_nets[g])
	recompute.insert(w);
      
      gate_loc[g] = loc;
      loc_gate[loc] = g;
    }
  else
    {
      loc_gate.erase(loc);
      gate_loc[g] = Location();
    }
  
  auto i = gate_chain.find(g);
  if (i != gate_chain.end())
    {
      int c = i->second;
      save_set_chain(c, loc.x(), loc.y());
    }
  
  int t = chipdb->tile(loc.x(), loc.y());
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
  for (int w : recompute)
    {
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
  for (const auto &p : restore_loc)
    {
      if (p.second)
	{
	  gate_loc[p.second] = p.first;
	  loc_gate[p.first] = p.second;
	}
      else
	{
	  gate_loc[p.second] = Location();
	  loc_gate.erase(p.first);
	}
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
  restore_loc.clear();
  restore_chain.clear();
  restore_net_length.clear();
  recompute.clear();
}

bool
Placer::valid(int t) const
{
  int x = chipdb->tile_x(t),
    y = chipdb->tile_y(t);
  if (chipdb->tile_type[t] == TileType::LOGIC_TILE)
    {
      Net *global_clk = nullptr,
	*global_sr = nullptr,
	*global_cen = nullptr;
      for (int q = 0; q < 8; q ++)
	{
	  Location loc2(x, y, q);
	  auto i = loc_gate.find(loc2);
	  if (i != loc_gate.end())
	    {
	      Instance *inst = gates[i->second];
	      
	      Port *clk = inst->find_port("CLK");
	      if (clk->connected())
		{
		  Net *n = clk->connection();
		  if (!global_clk)
		    global_clk = n;
		  else if (global_clk != n)
		    return false;
		}
	      
	      Port *cen = inst->find_port("CEN");
	      if (cen->connected())
		{
		  Net *n = cen->connection();
		  if (!global_cen)
		    global_cen = n;
		  else if (global_cen != n)
		    return false;
		}
	      
	      Port *sr = inst->find_port("SR");
	      if (sr->connected())
		{
		  Net *n = sr->connection();
		  if (!global_sr)
		    global_sr = n;
		  else if (global_sr != n)
		    return false;
		}
	    }
	}
    }
  else if (chipdb->tile_type[t] == TileType::IO_TILE)
    {
      int b = chipdb->tile_bank(t);
      
      Net *latch = nullptr;
      for (int t2 : chipdb->bank_tiles[b])
	for (int p = 0; p <= 1; ++p)
	  {
	    Location loc(chipdb->tile_x(t2),
			 chipdb->tile_y(t2), 
			 p);
	    int g = lookup_or_default(loc_gate, loc, 0);
	    if (g)
	      {
		Instance *inst = gates[g];
		Net *n = inst->find_port("LATCH_INPUT_VALUE")->connection();
		if (n)
		  {
		    if (latch)
		      {
			if (latch != n)
			  return false;
		      }
		    else
		      latch = n;
		  }
	      }
	  }
      
      Location loc0(x, y, 0),
	loc1(x, y, 1);
      int g0 = lookup_or_default(loc_gate, loc0, 0),
	g1 = lookup_or_default(loc_gate, loc1, 0);
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
      
      Location loc2(x, y, 2);
      int g2 = lookup_or_default(loc_gate, loc2, 0);
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
    assert(chipdb->tile_type[t] == TileType::RAMT_TILE);
      
  return true;
}

bool
Placer::valid(const Location &loc) const
{
  return valid(chipdb->tile(loc.x(), loc.y()));
}

void
Placer::accept_or_restore()
{
  int delta;
  std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
  
  for (int t : changed_tiles)
    {
      if (!valid(t))
	goto L;
    }
  
  delta = save_recompute_wire_length();
  
  // check();
  
  if (delta <= 0
      || (temp > 1e-6
	  && prob_dist(rg) <= exp(-delta/temp)))
    ;
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
  assert(loc_gate.size() == gates.size() - 1);  // no 0
  
  for (Instance *inst : top->instances())
    {
      unsigned g = gate_idx.at(inst);
      assert(loc_gate.at(gate_loc[g]) == g);
    }
  for (const auto &p : loc_gate)
    assert(gate_loc[p.second] == p.first);
  
  for (unsigned c = 0; c < chains.chains.size(); ++c)
    {
      const auto &v = chains.chains[c];
      for (unsigned i = 0; i < v.size(); ++i)
	{
	  Location loc (chain_x[c],
			chain_start[c] + i / 8,
			i % 8);
	  int g = gate_idx.at(v[i]);
	  int loc_g = loc_gate.at(loc);
	  assert(g == loc_g);
	}
    }
  for (int w = 0; w < (int)nets.size(); ++w)
    assert(net_length[w] == compute_net_length(w));
}
#endif

int
Placer::compute_net_length(int w)
{
  if (contains(glb_nets, w)
      || net_gates[w].empty())
    return 0;
  
  const std::vector<int> &w_gates = net_gates[w];
  
  int g0 = w_gates[0];
  const Location &loc0 = gate_loc[g0];
  int x_min = loc0.x(),
    x_max = loc0.x(),
    y_min = loc0.y(),
    y_max = loc0.y();
  
  for (int i = 1; i < (int)w_gates.size(); ++i)
    {
      int g = w_gates[i];
      const Location &loc = gate_loc[g];
      x_min = std::min(x_min, loc.x());
      x_max = std::max(x_max, loc.x());
      y_min = std::min(y_min, loc.y());
      y_max = std::max(y_max, loc.y());
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

Placer::Placer(const ChipDB *cdb,
	       const Package &package_,
	       Design *d_,
	       CarryChains &chains_,
	       const Constraints &constraints_,
	       const std::unordered_map<Instance *, uint8_t> &gb_inst_gc_,
	       Configuration &conf_)
  : chipdb(cdb),
    package(package_),
    d(d_),
    chains(chains_),
    constraints(constraints_),
    gb_inst_gc(gb_inst_gc_),
    conf(conf_),
    models(d),
    top(d->top()),
    temp(1000.0)
{
  for (int i = 0; i < chipdb->width; ++i)
    {
      int t = chipdb->tile(i, 1);
      if (chipdb->tile_type[t] == TileType::LOGIC_TILE)
	logic_columns.push_back(i);
    }
  for (int i = 0; i < chipdb->n_tiles; ++i)
    {
      switch(chipdb->tile_type[i])
	{
	case TileType::LOGIC_TILE:
	  logic_tiles.push_back(i);
	  break;
	case TileType::RAMT_TILE:
	  ramt_tiles.push_back(i);
	  break;
	default:
	  break;
	}
    }
  for (const auto &p : chipdb->gbufin)
    {
      gb_locs.push_back(Location(p.first.first,
				 p.first.second,
				 2));
    }
  
  std::tie(nets, net_idx) = top->index_nets();
  int n_nets = nets.size();
  
  net_length.resize(n_nets);
  net_gates.resize(n_nets);
  
  std::tie(gates, gate_idx) = top->index_instances();
  int n_gates = gates.size();
  
  gate_loc.resize(n_gates);
  gate_nets.resize(n_gates);
  
  for (Instance *inst : top->instances())
    {
      if (models.is_gb(inst))
	{
	  Net *n = inst->find_port("GLOBAL_BUFFER_OUTPUT")->connection();
	  glb_nets.insert(net_idx.at(n));
	}
    }
}

void
Placer::place_initial()
{
  int n_gates = gates.size();
  BitVector locked(n_gates);

  chained.resize(n_gates);
  
  // place chains
  int logic_rows = chipdb->height - 2;
  std::vector<int> logic_column_free(logic_columns.size(), 1);
  for (unsigned i = 0; i < chains.chains.size(); ++i)
    {
      const auto &v = chains.chains[i];
      extend(gate_chain, gate_idx.at(v[0]), i);
      
      int nt = (v.size() + 7) / 8;
      for (unsigned k = 0; k < logic_columns.size(); ++k)
	{
	  if (logic_column_free[k] + nt - 1 <= logic_rows)
	    {
	      int x = logic_columns[k];
	      int y = logic_column_free[k];
	      
	      for (unsigned j = 0; j < v.size(); ++j)
		{
		  Instance *inst = v[j];
		  unsigned g = gate_idx.at(inst);
		  Location loc(x,
			       y + j / 8,
			       j % 8);
		  extend(loc_gate, loc, g);
		  gate_loc[g] = loc;
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
  
  // place io, logic, ram
  int n_io_placed = 0,
    n_lc_placed = 0,
    n_ramt_placed = 0,
    n_gb_placed = 0;
  
  std::unordered_set<Location> io_locs;
  for (const auto &p : package.pin_loc)
    extend(io_locs, p.second);
  
  std::vector<Net *> bank_latch(4, nullptr);
  for (const auto &p : constraints.net_pin)
    {
      int g = top_port_io_gate(p.first);
      Instance *inst = gates[g];
      
      Location loc = package.pin_loc.at(p.second);
      int t = chipdb->tile(loc.x(),
			   loc.y());
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
      
      Location loc_other(loc.x(),
			 loc.y(),
			 loc.pos() ? 0 : 1);
      int g_other = lookup_or_default(loc_gate, loc_other, 0);
      if (g_other)
	{
	  Instance *inst_other = gates[g_other];
	  if (inst->get_param("NEG_TRIGGER").get_bit(0)
	      != inst_other->get_param("NEG_TRIGGER").get_bit(0))
	    fatal(fmt("pcf error: incompatible NEG_TRIGGER parameters in PIO at (" 
		      << loc.x() << ", " << loc.y() << ")"));
	}
      
      extend(loc_gate, loc, g);
      gate_loc[g] = loc;
      io_locs.erase(loc);
      locked[g] = true;
      ++n_io_placed;
      
      assert(valid(loc));
    }
  
  free_io_locs = std::vector<Location>(io_locs.begin(),
				       io_locs.end());
  
  std::vector<Location> empty_io_locs = free_io_locs,
    empty_gb_locs = gb_locs,
    empty_logic_locs,
    empty_ramt_locs;
  for (int t : logic_tiles)
    for (int p = 0; p < 8; p ++)
      {
	Location loc(chipdb->tile_x(t),
		     chipdb->tile_y(t),
		     p);
	if (contains_key(loc_gate, loc))  // chains
	  continue;
	empty_logic_locs.push_back(loc);
      }
  for (int t : ramt_tiles)
    {
      empty_ramt_locs.push_back(Location(chipdb->tile_x(t),
					 chipdb->tile_y(t),
					 0));
    }
  
  int n_io_gates = 0,
    n_lc_gates = 0,
    n_bram_gates = 0,
    n_gb_gates = 0;
  for (unsigned i = 1; i < gates.size(); i ++)  // skip 0, nullptr
    {
      Instance *inst = gates[i];
      if (models.is_lc(inst))
	++n_lc_gates;
      else if (models.is_io(inst))
	++n_io_gates;
      else if (models.is_gb(inst))
	++n_gb_gates;
      else
	{
	  assert(models.is_ramX(inst));
	  ++n_bram_gates;
	}
    }
  
  std::set<std::pair<uint8_t, int>> io_q;
  
  for (unsigned i = 1; i < gates.size(); ++i)  // skip 0, nullptr
    {
      if (locked[i]
	  || chained[i])
	continue;
      
      free_gates.push_back(i);
      Instance *inst = gates[i];
      if (models.is_lc(inst))
	{
	  for (unsigned j = 0; j < empty_logic_locs.size(); ++j)
	    {
	      const Location &loc = empty_logic_locs[j];
	      extend(loc_gate, loc, i);
	      gate_loc[i] = loc;
	      if (!valid(loc))
		loc_gate.erase(loc);
	      else
		{
		  ++n_lc_placed;
		  empty_logic_locs[j] = empty_logic_locs.back();
		  empty_logic_locs.pop_back();
		  goto placed_gate;
		}
	    }
	  
	  fatal(fmt("failed to place: placed " 
		    << n_lc_placed 
		    << " LCs of " << n_lc_gates
		    << " / " << 8*logic_tiles.size()));
	}
      else if (models.is_io(inst))
	{
	  for (unsigned j = 0; j < empty_io_locs.size(); ++j)
	    {
	      const Location &loc = empty_io_locs[j];
	      extend(loc_gate, loc, i);
	      gate_loc[i] = loc;
	      if (!valid(loc))
		loc_gate.erase(loc);
	      else
		{
		  ++n_io_placed;
		  empty_io_locs[j] = empty_io_locs.back();
		  empty_io_locs.pop_back();
		  goto placed_gate;
		}
	    }
	  fatal(fmt("failed to place: placed "
		    << n_io_placed
		    << " IOs of " << n_io_gates
		    << " / " << package.pin_loc.size()));
	}
      else if (models.is_gb(inst))
	{
	  io_q.insert(std::make_pair(gb_inst_gc.at(inst), i));
	}
      else
	{
	  assert(models.is_ramX(inst));
	  for (unsigned j = 0; j < empty_ramt_locs.size(); ++j)
	    {
	      const Location &loc = empty_ramt_locs[j];
	      extend(loc_gate, loc, i);
	      gate_loc[i] = loc;
	      if (!valid(loc))
		loc_gate.erase(loc);
	      else
		{
		  ++n_ramt_placed;
		  empty_ramt_locs[j] = empty_ramt_locs.back();
		  empty_ramt_locs.pop_back();
		  goto placed_gate;
		}
	    }
	  fatal(fmt("failed to place: placed "
		    << n_ramt_placed
		    << " BRAMs of " << n_bram_gates
		    << " / " << ramt_tiles.size()));
	  
	}
    placed_gate:;
    }
  
  // place gb
  while (!io_q.empty())
    {
      std::pair<uint8_t, int> p = *io_q.begin();
      io_q.erase(io_q.begin());
      
      int i = p.second;
      for (unsigned j = 0; j < empty_gb_locs.size(); ++j)
	{
	  const Location &loc = empty_gb_locs[j];
	  extend(loc_gate, loc, i);
	  gate_loc[i] = loc;
	  if (!valid(loc))
	    loc_gate.erase(loc);
	  else
	    {
	      ++n_gb_placed;
	      empty_gb_locs[j] = empty_gb_locs.back();
	      empty_gb_locs.pop_back();
	      goto placed_gb;
	    }
	}
      fatal(fmt("failed to place: placed "
		<< n_gb_placed
		<< " GBs of " << n_gb_gates
		<< " / 8"));
    placed_gb:;
    }
  
  assert(loc_gate.size() == gates.size() - 1);  // no 0
  
  for (unsigned g = 1; g < gates.size(); g ++)  // skip 0, nullptr
    {
      Instance *inst = gates[g];
      
      for (const auto &p : inst->ports())
	{
	  Net *n = p.second->connection();
	  if (n)
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
  for (const auto &p : loc_gate)
    {
      const Location &loc = p.first;
      int t = chipdb->tile(loc.x(), loc.y());
      const auto &func_cbits = chipdb->tile_nonrouting_cbits.at(chipdb->tile_type[t]);
      
      Instance *inst = gates[p.second];
      if (models.is_lc(inst))
	{
	  BitVector lut_init = inst->get_param("LUT_INIT").as_bits();
	  lut_init.resize(16);
	  
	  static std::vector<int> lut_perm = {
	    4, 14, 15, 5, 6, 16, 17, 7, 3, 13, 12, 2, 1, 11, 10, 0,
	  };
	  
	  const auto &cbits = func_cbits.at(fmt("LC_" << loc.pos()));
	  for (int i = 0; i < 16; ++i)
	    conf.set_cbit(CBit(loc.x(),
			       loc.y(),
			       cbits[lut_perm[i]].row,
			       cbits[lut_perm[i]].col),
			  lut_init[i]);
	  
	  bool carry_enable = inst->get_param("CARRY_ENABLE").get_bit(0);
	  if (carry_enable)
	    {
	      conf.set_cbit(CBit(loc.x(),
				 loc.y(),
				 cbits[8].row,
				 cbits[8].col), (bool)carry_enable);
	      if (loc.pos() == 0)
		{
		  Net *n = inst->find_port("CIN")->connection();
		  if (n && n->is_constant())
		    {
		      const CBit &carryinset_cbit = func_cbits.at("CarryInSet")[0];
		      conf.set_cbit(CBit(loc.x(),
					 loc.y(),
					 carryinset_cbit.row,
					 carryinset_cbit.col), 
				    n->constant() == Value::ONE);
		    }
		}
	    }
	  
	  bool dff_enable = inst->get_param("DFF_ENABLE").get_bit(0);
	  conf.set_cbit(CBit(loc.x(),
			     loc.y(),
			     cbits[9].row,
			     cbits[9].col), dff_enable);
	  
	  if (dff_enable)
	    {
	      bool neg_clk = inst->get_param("NEG_CLK").get_bit(0);
	      const CBit &neg_clk_cbit = func_cbits.at("NegClk")[0];
	      conf.set_cbit(CBit(loc.x(),
				 loc.y(),
				 neg_clk_cbit.row,
				 neg_clk_cbit.col),
			    (bool)neg_clk);
	      
	      bool set_noreset = inst->get_param("SET_NORESET").get_bit(0);
	      conf.set_cbit(CBit(loc.x(),
				 loc.y(),
				 cbits[18].row,
				 cbits[18].col), (bool)set_noreset);
	      
	      bool async_sr = inst->get_param("ASYNC_SR").get_bit(0);
	      conf.set_cbit(CBit(loc.x(),
				 loc.y(),
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
	      conf.set_cbit(CBit(loc.x(), loc.y(), cbit.row, cbit.col),
			    pin_type[i]);
	    }
	  
	  const auto &negclk_cbits = func_cbits.at("NegClk");
	  bool neg_trigger = inst->get_param("NEG_TRIGGER").get_bit(0);
	  for (int i = 0; i <= 1; ++i)
	    conf.set_cbit(CBit(loc.x(),
			       loc.y(),
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
	  const auto &ramb_func_cbits = chipdb->tile_nonrouting_cbits.at(TileType::RAMB_TILE);
	  const CBit &cbit0 = func_cbits.at("RamConfig.CBIT_0")[0],
	    &cbit1 = func_cbits.at("RamConfig.CBIT_1")[0],
	    &cbit2 = func_cbits.at("RamConfig.CBIT_2")[0],
	    &cbit3 = func_cbits.at("RamConfig.CBIT_3")[0],
	    &negclk = func_cbits.at("NegClk")[0],
	    &ramb_negclk = ramb_func_cbits.at("NegClk")[0];
	  
	  conf.set_cbit(CBit(loc.x(),
			     loc.y(),
			     cbit0.row,
			     cbit0.col),
			wm[0]);
	  conf.set_cbit(CBit(loc.x(),
			     loc.y(),
			     cbit1.row,
			     cbit1.col),
			wm[1]);
	  conf.set_cbit(CBit(loc.x(),
			     loc.y(),
			     cbit2.row,
			     cbit2.col),
			rm[0]);
	  conf.set_cbit(CBit(loc.x(),
			     loc.y(),
			     cbit3.row,
			     cbit3.col),
			rm[1]);
	  
	  if (models.is_ramnr(inst)
	      || models.is_ramnrnw(inst))
	    conf.set_cbit(CBit(loc.x(),
			       loc.y(),
			       negclk.row,
			       negclk.col),
			  true);
	  
	  if (models.is_ramnw(inst)
	      || models.is_ramnrnw(inst))
	    conf.set_cbit(CBit(loc.x(),
			       loc.y() - 1,
			       ramb_negclk.row,
			       ramb_negclk.col),
			  true);
	}
      
      placement[inst] = p.first;
    }
  
  // set IoCtrl configuration bits
  {
    const auto &func_cbits = chipdb->tile_nonrouting_cbits.at(TileType::IO_TILE);
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
	int g = lookup_or_default(loc_gate, loc, 0);
	if (g)
	  {
	    Instance *inst = gates[g];
	    
	    if (inst->find_port("D_IN_0")->connected()
		|| inst->find_port("D_IN_1")->connected())
	      enable_input = true;
	    pullup = inst->get_param("PULLUP").get_bit(0);
	    conf.set_cbit(CBit(loc.x(),
			       loc.y(),
			       lvds.row,
			       lvds.col),
			  inst->get_param("IO_STANDARD").as_string() == "SB_LVDS_INPUT");
	  }
	
	if (ieren_loc.pos() == 0)
	  {
	    conf.set_cbit(CBit(ieren_loc.x(),
			       ieren_loc.y(),
			       ren_0.row,
			       ren_0.col),
			  !pullup);  // active low
	  }
	else
	  {
	    assert(ieren_loc.pos() == 1);
	    conf.set_cbit(CBit(ieren_loc.x(),
			       ieren_loc.y(),
			       ren_1.row,
			       ren_1.col),
			  !pullup);  // active low
	  }
	if (ieren_loc.pos() == 0)
	  {
	    conf.set_cbit(CBit(ieren_loc.x(),
			       ieren_loc.y(),
			       ie_0.row,
			       ie_0.col),
			  !enable_input);  // active low
	  }
	else
	  {
	    assert(ieren_loc.pos() == 1);
	    conf.set_cbit(CBit(ieren_loc.x(),
			       ieren_loc.y(),
			       ie_1.row,
			       ie_1.col),
			  !enable_input); // active low
	  }
      }
    
    std::unordered_set<Location> ieren_image;
    for (const auto &p : chipdb->ieren)
      extend(ieren_image, p.second);
    for (int t = 0; t < chipdb->n_tiles; ++t)
      {
	if (chipdb->tile_type[t] != TileType::IO_TILE)
	  continue;
	for (int p = 0; p <= 1; ++p)
	  {
	    bool enable_input = false;
	    bool pullup = true;  // default pullup
	    
	    Location loc(chipdb->tile_x(t),
			 chipdb->tile_y(t),
			 p);
	    if (contains(ieren_image, loc))
	      continue;
	    
	    if (loc.pos() == 0)
	      {
		conf.set_cbit(CBit(loc.x(),
				   loc.y(),
				   ren_0.row,
				   ren_0.col),
			      !pullup);  // active low
	      }
	    else
	      {
		assert(loc.pos() == 1);
		conf.set_cbit(CBit(loc.x(),
				   loc.y(),
				   ren_1.row,
				   ren_1.col),
			      !pullup);  // active low
	      }
	    if (loc.pos() == 0)
	      {
		conf.set_cbit(CBit(loc.x(),
				   loc.y(),
				   ie_0.row,
				   ie_0.col),
			      !enable_input);  // active low
	      }
	    else
	      {
		assert(loc.pos() == 1);
		conf.set_cbit(CBit(loc.x(),
				   loc.y(),
				   ie_1.row,
				   ie_1.col),
			      !enable_input); // active low
	      }
	  }
      }
  }
  
  // set RamConfig.PowerUp configuration bit
  {
    const CBit &powerup = (chipdb->tile_nonrouting_cbits.at(TileType::RAMB_TILE)
			   .at("RamConfig.PowerUp")
			   [0]);
    for (int t : ramt_tiles)
      {
	Location loc(chipdb->tile_x(t),
		     chipdb->tile_y(t)-1, // PowerUp on ramb tile
		     0);
	int g = lookup_or_default(loc_gate, loc, 0);
	assert(!g || models.is_ramX(gates[g]));
	conf.set_cbit(CBit(loc.x(),
			   loc.y(),
			   powerup.row,
			   powerup.col),
		      // active low
		      !g);
      }
  }
}

std::unordered_map<Instance *, Location>
Placer::place()
{
  place_initial();
  // check();
  
  *logs << "  initial wire length = " << wire_length() << "\n";
  
  for (int n = 0; n < chipdb->n_tiles * 8; ++n)
    {
      for (int g : free_gates)
	{
	  Location new_loc = gate_random_loc(g);
	  
	  int new_g = lookup_or_default(loc_gate, new_loc, 0);
	  if (new_g && chained[new_g])
	    continue;
	  
	  move_gate(g, new_loc);
	  accept_or_restore();
	}
      
      for (int c = 0; c < (int)chains.chains.size(); ++c)
	{
	  std::pair<Location, bool> new_loc = chain_random_loc(c);
	  if (new_loc.second)
	    {
	      move_chain(c, new_loc.first);
	      accept_or_restore();
	    }
	}
      
      temp *= 0.99;
    }
  
  *logs << "  final wire length = " << wire_length() << "\n";
  
  configure();
  
  int n_pio = 0,
    n_plb = 0,
    n_bram = 0;
  std::unordered_set<int> seen;
  for (const Location &loc : gate_loc)
    {
      int t = chipdb->tile(loc.x(), loc.y());
      seen.insert(t);
    }
  for (int t : seen)
    {
      if (chipdb->tile_type[t] == TileType::LOGIC_TILE)
	++n_plb;
      else if (chipdb->tile_type[t] == TileType::IO_TILE)
	++n_pio;
      else if (chipdb->tile_type[t] == TileType::RAMT_TILE)
	++n_bram;
    }
  
  *logs << "\nAfter placement:\n"
	<< "PIOs       " << n_pio << " / " << package.pin_loc.size() << "\n"
	<< "PLBs       " << n_plb << " / " << logic_tiles.size() << "\n"
	<< "BRAMs      " << n_bram << " / " << ramt_tiles.size() << "\n"
	<< "\n";
  
  return std::move(placement);
}

std::unordered_map<Instance *, Location>
place(const ChipDB *chipdb,
      const Package &package,
      Design *d,
      CarryChains &chains,
      const Constraints &constraints,
      const std::unordered_map<Instance *, uint8_t> &gb_inst_gc,
      Configuration &conf)
{
  Placer placer(chipdb, package, d, chains, constraints, gb_inst_gc, conf);
  
  clock_t start = clock();
  std::unordered_map<Instance *, Location> placement = placer.place();
  clock_t end = clock();
  
  *logs << "  place time "
	<< std::fixed << std::setprecision(2)
	<< (double)(end - start) / (double)CLOCKS_PER_SEC << "s\n";
  
  return std::move(placement);
}
