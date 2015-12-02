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
#include "designstate.hh"
#include "global.hh"

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
  
  DesignState &ds;
  const ChipDB *chipdb;
  const Package &package;
  Design *d;
  Models &models;
  Model *top;
  const CarryChains &chains;
  const Constraints &constraints;
  const std::map<Instance *, uint8_t, IdLess> &gb_inst_gc;
  std::map<Instance *, int, IdLess> &placement;
  Configuration &conf;
  
  std::vector<int> logic_columns;
  std::vector<int> logic_tiles,
    ramt_tiles;
  
  std::vector<std::vector<int>> related_tiles;
  
  std::vector<Net *> nets;
  std::map<Net *, int, IdLess> net_idx;
  
  int n_gates;
  BasedVector<Instance *, 1> gates;
  std::map<Instance *, int, IdLess> gate_idx;
  
  BasedBitVector<1> locked;
  BasedBitVector<1> chained;
  
  BasedVector<int, 1> gate_clk, gate_sr, gate_cen, gate_latch;
  
  BasedVector<std::vector<int>, 1> gate_local_np;
  UllmanSet tmp_local_np;
  
  BitVector net_global;
  
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
  
  bool move_failed;
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
  
#ifndef NDEBUG
  void check();
#endif
  
public:
  Placer(random_generator &rg_, DesignState &ds_);
  
  void place();
  void set_placement(const std::map<Instance *, int, IdLess> &new_placement);
};

CellType
Placer::gate_cell_type(int g)
{
  Instance *inst = gates[g];
  if (models.is_lc(inst))
    return CellType::LOGIC;
  else if (models.is_ioX(inst))
    return CellType::IO;
  else if (models.is_gb(inst))
    return CellType::GB;
  else if (models.is_warmboot(inst))
    return CellType::WARMBOOT;
  else if (models.is_pllX(inst))
    return CellType::PLL;
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
      return random_element(chipdb->cell_type_cells[ct_idx], rg);
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
  if (locked[g])
    move_failed = true;
  
  int cell = gate_cell[g]; // copy
  if (new_cell == cell)
    return;
  
  int new_g = cell_gate[new_cell];
  if (new_g && locked[new_g])
    move_failed = true;
  
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
  for (int t2 : related_tiles[t])
    changed_tiles.insert(t2);
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
  move_failed = false;
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
          if (!contains(package.loc_pin, loc0))
            return false;
          
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
          if (!contains(package.loc_pin, loc1))
            return false;
          
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
          if ((g0 && models.is_gb_io(gates[g0]))
              || (g1 && models.is_gb_io(gates[g1])))
            return false;
          
          Instance *inst = gates[g2];
          int gc = lookup_or_default(gb_inst_gc, inst, gc_clk);
          int global = chipdb->gbufin.at(std::make_pair(x, y));
          if (! (gc & (1 << global)))
            return false;
        }
      
      Location loc3(t, 3);
      int cell3 = chipdb->loc_cell(loc3);
      int g3 = cell3 ? cell_gate[cell3] : 0;
      Instance *inst3 = g3 ? gates[g3] : nullptr;
      if (inst3)
        {
          const auto &pA = chipdb->cell_mfvs.at(cell3).at("PLLOUT_A");
          int tA = pA.first;
          int iA = std::stoi(pA.second);
          
          int cA = chipdb->loc_cell(Location(tA, iA));
          int gA = cell_gate[cA];
          Instance *instA = gA ? gates[gA] : nullptr;
          
          if (instA && instA->find_port("D_IN_0")->connection())
            return false;
          
          if (inst3->instance_of()->name() == "SB_PLL40_2F_CORE"
              || inst3->instance_of()->name() == "SB_PLL40_2_PAD"
              || inst3->instance_of()->name() == "SB_PLL40_2F_PAD")
            {
              const auto &pB = chipdb->cell_mfvs.at(cell3).at("PLLOUT_B");
              int tB = pB.first;
              int iB = std::stoi(pB.second);
          
              int cB = chipdb->loc_cell(Location(tB, iB));
              int gB = cell_gate[cB];
              Instance *instB = gB ? gates[gB] : nullptr;
          
              if (instB && instB->find_port("D_IN_0")->connection())
                return false;
            }
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
  
  ++n_move;
  
  if (move_failed)
    {
      *logs << "failed\n";
      goto L;
    }
  for (int i = 0; i < (int)changed_tiles.size(); ++i)
    {
      int t = changed_tiles.ith(i);
      if (!valid(t))
	{
	  *logs << "invalid\n";
	  goto L;
	}
    }
  
  delta = save_recompute_wire_length();
  
  // check();
  
  if (delta < 0
      || (temp > 1e-6
          && rg.random_real(0.0, 1.0) <= exp(-delta/temp))
      || 1)
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

Placer::Placer(random_generator &rg_, DesignState &ds_)
  : rg(rg_),
    ds(ds_),
    chipdb(ds.chipdb),
    package(ds.package),
    d(ds.d),
    models(ds.models),
    top(ds.top),
    chains(ds.chains),
    constraints(ds.constraints),
    gb_inst_gc(ds.gb_inst_gc),
    placement(ds.placement),
    conf(ds.conf),
    related_tiles(chipdb->n_tiles),
    diameter(std::max(chipdb->width,
                      chipdb->height)),
    temp(10000.0),
    move_failed(false),
    changed_tiles(chipdb->n_tiles),
    cell_gate(chipdb->n_cells, 0)
{
  for (int i = 1; i <= (int)chipdb->n_cells; ++i)
    {
      if (chipdb->cell_type[i] == CellType::PLL)
        {
          int t = chipdb->cell_location[i].tile();
          
          std::vector<int> t_related;
          t_related.push_back(t);
          
          const auto &pA = chipdb->cell_mfvs.at(i).at("PLLOUT_A");
          int tA = pA.first;
          t_related.push_back(tA);
          
          const auto &pB = chipdb->cell_mfvs.at(i).at("PLLOUT_B");
          int tB = pB.first;
          t_related.push_back(tB);
          
          for (int t2 : t_related)
            related_tiles[t2] = t_related;
        }
    }
  
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
  locked.resize(n_gates);
  chained.resize(n_gates);
  
  // place chains
  std::vector<int> logic_column_free(logic_columns.size(), 1);
  std::vector<int> logic_column_last(logic_columns.size(),
                                     chipdb->height - 2);
  
  // FIXME apply only if present
  for (unsigned i = 0; i < logic_column_free.size(); ++i)
    {
      if (chipdb->device == "1k"
          && (logic_columns[i] == 1
              || logic_columns[i] == 12))
        logic_column_free[i] = 2;
      else if (chipdb->device == "8k"
               && (logic_columns[i] == 1
                   || logic_columns[i] == 32))
        {
          logic_column_free[i] = 2;
          logic_column_last[i] = 31;
        }
    }
  
  for (unsigned i = 0; i < chains.chains.size(); ++i)
    {
      const auto &v = chains.chains[i];
      
      int gate0 = gate_idx.at(v[0]);
      assert(gate_chain[gate0] == -1);
      gate_chain[gate0] = i;
      
      int nt = (v.size() + 7) / 8;
      for (unsigned k = 0; k < logic_columns.size(); ++k)
        {
          if (logic_column_free[k] + nt - 1 <= logic_column_last[k])
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
  
  for (const auto &p : placement)
    {
      Instance *inst = p.first;
      int g = gate_idx.at(inst);
      int c = p.second;
      
      assert(cell_gate[c] == 0);
      cell_gate[c] = g;
      gate_cell[g] = c;
      
      locked[g] = true;
      
      CellType ct = gate_cell_type(g);
      int ct_idx = cell_type_idx(ct);
      ++cell_type_n_placed[ct_idx];
      
      assert(valid(chipdb->cell_location[c].tile()));
    }
  
  std::vector<std::vector<int>> cell_type_empty_cells = chipdb->cell_type_cells;
  for (int i = 0; i < n_cell_types; ++i)
    for (int j = 0; j < (int)cell_type_empty_cells[i].size();)
      {
        int c = cell_type_empty_cells[i][j];
        if (cell_gate[c] != 0)
          pop(cell_type_empty_cells[i], j);
        else
          ++j;
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
          io_q.insert(std::make_pair(lookup_or_default(gb_inst_gc, inst, gc_clk), i));
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
Placer::set_placement(const std::map<Instance *, int, IdLess> &new_placement)
{
  n_move = n_accept = 0;
  
  for (int i = 0; i < 3; ++i)
    for (const auto &p : new_placement)
      {
	Instance *inst = p.first;
	int g = gate_idx.at(inst);
	if (g >= 15 && g <= 15) 
	  continue;
	
	int new_cell = p.second;
	const Location &new_loc = chipdb->cell_location[new_cell];
	
	int c = gate_chain[g];
	if (c == -1)
	  {
	    if (chained[g])
	      continue;
	  
	    move_gate(g, new_cell);
	    accept_or_restore();
	  }
	else
	  {
	    assert(!move_failed);
	    move_chain(c, new_loc);
	    accept_or_restore();
	  }
	
	if (gate_cell[g] != new_cell)
	  *logs << g << " " << gate_cell[g] << " " << new_cell << "\n";
      }
  
  for (int g = 1; g <= n_gates; g ++)
    {
      Instance *inst = gates[g];
      int cell = gate_cell[g];
      
      placement[inst] = cell;
    }
  
  *logs << "after: n_move " << n_move << ", n_accept " << n_accept << "\n";
}

void
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
              if (new_g 
                  && chained[new_g])
                continue;
              
              assert(!move_failed);
              move_gate(g, new_cell);
              accept_or_restore();
              
              // check();
            }
          
          for (int c = 0; c < (int)chains.chains.size(); ++c)
            {
              std::pair<Location, bool> new_loc = chain_random_loc(c);
              if (new_loc.second)
                {
                  assert(!move_failed);
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

  for (int g = 1; g <= n_gates; g ++)
    {
      Instance *inst = gates[g];
      int cell = gate_cell[g];
      
      placement[inst] = cell;
    }
  
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
}

void
place(random_generator &rg, DesignState &ds)
{
  Placer placer(rg, ds);
  
  clock_t start = clock();
  placer.place();
  clock_t end = clock();
  
  *logs << "  place time "
        << std::fixed << std::setprecision(2)
        << (double)(end - start) / (double)CLOCKS_PER_SEC << "s\n";
}

void
place_set(random_generator &rg, DesignState &ds, 
	  const std::map<Instance *, int, IdLess> &new_placement)
{
  Placer placer(rg, ds);
  placer.place_initial();
  placer.set_placement(new_placement);
}
