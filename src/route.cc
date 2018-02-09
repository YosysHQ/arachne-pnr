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
#include "netlist.hh"
#include "location.hh"
#include "chipdb.hh"
#include "configuration.hh"
#include "bitvector.hh"
#include "ullmanset.hh"
#include "priorityq.hh"
#include "designstate.hh"

#include <cassert>
#include <ostream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <set>
#include <map>
#include <vector>
#include <ctime>

class Router;

class Comp
{
public:
  Comp() {}
  
  bool operator()(const std::pair<int, int> &lhs,
                  const std::pair<int, int> &rhs) const
  {
    return (lhs.second > rhs.second
            || (lhs.second == rhs.second
                && lhs.first > rhs.first));
  }
};

class Router
{
  const ChipDB *chipdb;
  Design *d;
  Models &models;
  const std::map<Instance *, int, IdLess> &placement;
  std::vector<Net *> &cnet_net;
  Configuration &conf;
  
  BitVector cnet_global,
    cnet_local;
  std::vector<std::vector<int>> cnet_outs;
  
  std::map<std::string, std::pair<std::string, bool>> ram_gate_chip;
  std::map<std::string, std::string> pll_gate_chip;
  
  std::vector<std::vector<int>> cnet_tiles;
  // cnet_bbox
  std::vector<int> cnet_xmin,
    cnet_xmax,
    cnet_ymin,
    cnet_ymax;
  
  int n_nets;  // to route
  std::vector<int> net_source;
  std::vector<std::vector<int>> net_targets;
  std::vector<Net *> net_net;
  
  int max_passes;
  int passes;
  
  int n_shared;
  std::vector<int> demand;
  std::vector<int> historical_demand;
  std::vector<std::vector<std::pair<int, int>>> net_route;
  
  // per net
  int current_net;
  UllmanSet unrouted;
  
  UllmanSet visited;
  
  UllmanSet frontier;
  // cn, cost[cn]
  PriorityQ<std::pair<int, int>, Comp> frontierq;
  
  std::vector<int> backptr;
  std::vector<int> cost;
  
  void start(int net);
  int pop();
  void visit(int cn);
  void ripup(int net);
  void traceback(int net, int target);
  
  int port_cnet(Instance *inst, Port *p);

#ifndef NDEBUG
  void check();
#endif
  
public:
  Router(DesignState &ds, int max_passes_v);
  
  void route();
};

int
Router::port_cnet(Instance *inst, Port *p)
{
  const auto &p_name = p->name();
  int cell = placement.at(inst);
  const Location &loc = chipdb->cell_location[cell];
  int t = loc.tile();
  
  std::string tile_net_name;
  if (models.is_lc(inst))
    {
      if (p_name == "CLK")
        tile_net_name = fmt("lutff_global/clk");
      else if (p_name == "CEN")
        tile_net_name = fmt("lutff_global/cen");
      else if (p_name == "SR")
        tile_net_name = fmt("lutff_global/s_r");
      else if (p_name == "I0")
        tile_net_name = fmt("lutff_" << loc.pos() << "/in_0");
      else if (p_name == "I1")
        tile_net_name = fmt("lutff_" << loc.pos() << "/in_1");
      else if (p_name == "I2")
        tile_net_name = fmt("lutff_" << loc.pos() << "/in_2");
      else if (p_name == "I3")
        tile_net_name = fmt("lutff_" << loc.pos() << "/in_3");
      else if (p_name == "CIN")
        {
          if (loc.pos() == 0)
            tile_net_name = "carry_in_mux";
          else
            return -1;
        }
      else if (p_name == "COUT")
        tile_net_name = fmt("lutff_" << loc.pos() << "/cout");
      else if (p_name == "LO")
        tile_net_name = fmt("lutff_" << loc.pos() << "/lout");
      else
        {
          assert(p_name == "O");
          tile_net_name = fmt("lutff_" << loc.pos() << "/out");
        }
      if (chipdb->tile_nets[t].find(tile_net_name) == chipdb->tile_nets[t].end())
      {
        fatal(fmt("failed to rote:  " << p->name() << " to " << tile_net_name));
      }
    }
  else if (models.is_ioX(inst))
    {
      if (p_name == "LATCH_INPUT_VALUE")
        tile_net_name = fmt("io_global/latch");
      else if (p_name == "CLOCK_ENABLE")
        tile_net_name = fmt("io_global/cen");
      else if (p_name == "INPUT_CLK")
        tile_net_name = fmt("io_global/inclk");
      else if (p_name == "OUTPUT_CLK")
        tile_net_name = fmt("io_global/outclk");
      else if (p_name == "OUTPUT_ENABLE")
        tile_net_name = fmt("io_" << loc.pos() << "/OUT_ENB");
      else if (p_name == "D_OUT_0")
        tile_net_name = fmt("io_" << loc.pos() << "/D_OUT_0");
      else if (p_name == "D_OUT_1")
        tile_net_name = fmt("io_" << loc.pos() << "/D_OUT_1");
      else if (p_name == "D_IN_0")
        tile_net_name = fmt("io_" << loc.pos() << "/D_IN_0");
      else if (p_name == "D_IN_1")
        tile_net_name = fmt("io_" << loc.pos() << "/D_IN_1");
      else if (models.is_io_i3c(inst))
        {
          assert(p_name == "PU_ENB" || p_name == "WEAK_PU_ENB");
          bool found = false;
          int i3c_cell = -1;
          for (int c : chipdb->cell_type_cells[cell_type_idx(CellType::IO_I3C)])
          {
            auto pin = chipdb->cell_mfvs.at(c).at("PACKAGE_PIN");
            if(loc.tile() == pin.first && loc.pos() == std::stoi(pin.second))
            {
              found = true;
              i3c_cell = c;
              break;
            }
          }
          assert(found);
          const auto &p2 = chipdb->cell_mfvs.at(i3c_cell).at(p_name);
          t = p2.first;
          tile_net_name = p2.second;
        }
      else
        {
          assert(models.is_gb_io(inst)
                 && p_name == "GLOBAL_BUFFER_OUTPUT");
          int g = chipdb->loc_pin_glb_num.at(loc);
          tile_net_name = fmt("glb_netwk_" << g);
        }
    }
  else if (models.is_gb(inst))
    {
      if (p_name == "USER_SIGNAL_TO_GLOBAL_BUFFER")
        tile_net_name = fmt("fabout");
      else
        {
          assert(p_name == "GLOBAL_BUFFER_OUTPUT");
          int g = chipdb->gbufin.at(std::make_pair(chipdb->tile_x(t),
                                                   chipdb->tile_y(t)));
          tile_net_name = fmt("glb_netwk_" << g);
        }
    }
  else if (models.is_warmboot(inst))
    {
      const auto &p2 = chipdb->cell_mfvs.at(cell).at(p_name);
      t = p2.first;
      tile_net_name = p2.second;
    }
  else if (models.is_ramX(inst))
    {
      auto r = ram_gate_chip.at(p_name);
      tile_net_name = r.first;
      
      // FIXME if (r.second)
      if (!contains(chipdb->tile_nets[t], tile_net_name))
        t = chipdb->tile(chipdb->tile_x(loc.tile()),
                         chipdb->tile_y(loc.tile()) - 1);
    }
  else if (models.is_mac16(inst) || models.is_spram(inst) || models.is_lfosc(inst) ||
           models.is_hfosc(inst) || models.is_rgba_drv(inst) || models.is_ledda_ip(inst) || 
           models.is_spi(inst) || models.is_i2c(inst))
    {
      //Convert [x] to _x
      std::string db_name;
      //Deal with ROUTE_THROUGH_FABRIC
      if((models.is_hfosc(inst) || models.is_lfosc(inst)) &&
          inst->is_attr_set("ROUTE_THROUGH_FABRIC")) {
        if(p_name == "CLKHF" || p_name == "CLKLF")
            db_name = std::string(p_name) + "_FABRIC";
        else
            db_name = p_name;
      } else {
        for(auto c : p_name) {
          if(c == '[')
            db_name += "_";
          else if(c == ']')
            ;
          else
            db_name += c;
        }          
      }
      
      if(models.is_mac16(inst)) {
        if(p_name == "ACCUMCI" || p_name == "SIGNEXTIN") {
          assert(p->connection()->is_constant() && (p->connection()->constant() == Value::ZERO));
          return -1;
        }
      }
      
      const auto &p2 = chipdb->cell_mfvs.at(cell).at(db_name);
      t = p2.first;
      tile_net_name = p2.second;
    }
  else
    {
      assert(models.is_pllX(inst));
      
      // FIXME
      std::string r = lookup_or_default(pll_gate_chip, p_name, p_name);
      if (r == "PLLOUTGLOBAL"
          || r == "PLLOUTGLOBALA")
        {
          const auto &p2 = chipdb->cell_mfvs.at(cell).at("PLLOUT_A");
          Location g_loc(p2.first, std::stoi(p2.second));
          int g = chipdb->loc_pin_glb_num.at(g_loc);
          tile_net_name = fmt("glb_netwk_" << g);
        }
      else if (r == "PLLOUTGLOBALB")
        {
          const auto &p2 = chipdb->cell_mfvs.at(cell).at("PLLOUT_B");
          Location g_loc(p2.first, std::stoi(p2.second));
          int g = chipdb->loc_pin_glb_num.at(g_loc);
          tile_net_name = fmt("glb_netwk_" << g);
        }
      else
        {
          const auto &p2 = chipdb->cell_mfvs.at(cell).at(r);
          t = p2.first;
          if (r == "PLLOUT_A"
              || r == "PLLOUT_B")
            tile_net_name = fmt("io_" << p2.second << "/D_IN_0");
          else
            tile_net_name = p2.second;
        }
      
#if 0
      *logs << p_name << " aka " << r
            << " -> (" << chipdb->tile_x(t) << " " << chipdb->tile_y(t) << ") "
            << tile_net_name << "\n";
#endif
    }
  
  int n = chipdb->tile_nets[t].at(tile_net_name);
  return n;
}

#ifndef NDEBUG
void
Router::check()
{
  std::vector<int> demand2 (chipdb->n_nets, 0);
  for (int i = 0; i < n_nets; ++i)
    {
      for (const auto &p : net_route[i])
        ++demand2[p.second];
    }
  int n_shared2 = 0;
  for (int i = 0; i < chipdb->n_nets; ++i)  
    {
      assert(demand2[i] == demand[i]);
      if (demand2[i] > 1)
        ++n_shared2;
    }
  assert(n_shared2 == n_shared);
}
#endif

Router::Router(DesignState &ds, int max_passes_v)
  : chipdb(ds.chipdb),
    d(ds.d),
    models(ds.models),
    placement(ds.placement),
    cnet_net(ds.cnet_net),
    conf(ds.conf),
    cnet_global(chipdb->n_nets),
    cnet_local(chipdb->n_nets),
    cnet_outs(chipdb->n_nets),
    cnet_tiles(chipdb->n_nets),
    cnet_xmin(chipdb->n_nets),
    cnet_xmax(chipdb->n_nets),
    cnet_ymin(chipdb->n_nets),
    cnet_ymax(chipdb->n_nets),
    n_nets(0),
    max_passes(max_passes_v),
    n_shared(0),
    demand(chipdb->n_nets, 0),
    historical_demand(chipdb->n_nets, 0),
    unrouted(chipdb->n_nets),
    visited(chipdb->n_nets),
    frontier(chipdb->n_nets),
    backptr(chipdb->n_nets),
    cost(chipdb->n_nets)
{
  cnet_net = std::vector<Net *>(chipdb->n_nets, nullptr);
  
  for (int t = 0; t < chipdb->n_tiles; ++t)
    {
      for (const auto &p : chipdb->tile_nets[t])
        {
          if (is_prefix("local_", p.first))
            {
              if (!cnet_local[p.second])
                {
                  cnet_local[p.second] = true;
                  break;
                }
            }
          else if (is_prefix("glb_netwk_", p.first))
            {
              if (!cnet_global[p.second])
                {
                  cnet_global[p.second] = true;
                  break;
                }
            }
        }
    }
  
  for (int i = 0; i < chipdb->n_nets; ++i)
    {
      for (int s : chipdb->in_switches[i])
        {
          assert(contains_key(chipdb->switches[s].in_val, i));
          int j = chipdb->switches[s].out;
          assert(j != i);
          
          cnet_outs[i].push_back(j);
        }
    }
  
  for (int i = 0; i <= 7; ++i)
    extend(ram_gate_chip,
           fmt("RDATA[" << i << "]"),
           std::make_pair(fmt("ram/RDATA_" << i), true));
  for (int i = 8; i <= 15; ++i)
    extend(ram_gate_chip,
           fmt("RDATA[" << i << "]"),
           std::make_pair(fmt("ram/RDATA_" << i), false));
  
  for (int i = 0; i <= 10; ++i)
    extend(ram_gate_chip,
           fmt("RADDR[" << i << "]"),
           std::make_pair(fmt("ram/RADDR_" << i), false));
  
  for (int i = 0; i <= 10; ++i)
    extend(ram_gate_chip,
           fmt("WADDR[" << i << "]"),
           std::make_pair(fmt("ram/WADDR_" << i), true));
  
  if (chipdb->device == "1k")
    {
      for (int i = 0; i <= 7; ++i)
        extend(ram_gate_chip,
               fmt("MASK[" << i << "]"),
               std::make_pair(fmt("ram/MASK_" << i), true));
      for (int i = 8; i <= 15; ++i)
        extend(ram_gate_chip,
               fmt("MASK[" << i << "]"),
               std::make_pair(fmt("ram/MASK_" << i), false));
    }
  else if (chipdb->device == "8k")
    {
      for (int i = 0; i <= 7; ++i)
        extend(ram_gate_chip,
               fmt("MASK[" << i << "]"),
               std::make_pair(fmt("ram/MASK_" << i), false));
      for (int i = 8; i <= 15; ++i)
        extend(ram_gate_chip,
               fmt("MASK[" << i << "]"),
               std::make_pair(fmt("ram/MASK_" << i), true));

    }  
  else if (chipdb->device == "5k")
    {
      for (int i = 0; i <= 7; ++i)
        extend(ram_gate_chip,
               fmt("MASK[" << i << "]"),
               std::make_pair(fmt("ram/MASK_" << i), false));
      for (int i = 8; i <= 15; ++i)
        extend(ram_gate_chip,
               fmt("MASK[" << i << "]"),
               std::make_pair(fmt("ram/MASK_" << i), true));

    }
  else
    assert(chipdb->device == "384");
  
  for (int i = 0; i <= 7; ++i)
    extend(ram_gate_chip,
           fmt("WDATA[" << i << "]"),
           std::make_pair(fmt("ram/WDATA_" << i), true));
  for (int i = 8; i <= 15; ++i)
    extend(ram_gate_chip,
           fmt("WDATA[" << i << "]"),
           std::make_pair(fmt("ram/WDATA_" << i), false));
  
  extend(ram_gate_chip,
         "RCLKE",
         std::make_pair("ram/RCLKE", false));
  extend(ram_gate_chip,
         "RCLK",
         std::make_pair("ram/RCLK", false));
  extend(ram_gate_chip,
         "RCLKN",
         std::make_pair("ram/RCLK", false));
  extend(ram_gate_chip,
         "RE",
         std::make_pair("ram/RE", false));

  extend(ram_gate_chip,
         "WCLKE",
         std::make_pair("ram/WCLKE", true));
  extend(ram_gate_chip,
         "WCLK",
         std::make_pair("ram/WCLK", true));
  extend(ram_gate_chip,
         "WCLKN",
         std::make_pair("ram/WCLK", true));
  extend(ram_gate_chip,
         "WE",
         std::make_pair("ram/WE", true));
  
  for (int i = 0; i < 8; ++i)
    extend(pll_gate_chip, 
           fmt("DYNAMICDELAY[" << i << "]"),
           fmt("DYNAMICDELAY_" << i));
  extend(pll_gate_chip, "PLLOUTCORE", "PLLOUT_A");
  extend(pll_gate_chip, "PLLOUTCOREA", "PLLOUT_A");
  extend(pll_gate_chip, "PLLOUTCOREB", "PLLOUT_B");
  
  for (int t = 0; t < chipdb->n_tiles; ++t)
    for (const auto &p : chipdb->tile_nets[t])
      cnet_tiles[p.second].push_back(t);
  
  for (int i = 0; i < chipdb->n_nets; ++i)
    {
      assert(!cnet_tiles[i].empty());
      int t0 = cnet_tiles[i][0];
      int xmin, xmax, ymin, ymax;
      xmin = xmax = chipdb->tile_x(t0);
      ymin = ymax = chipdb->tile_y(t0);
      for (int j = 1; j < (int)cnet_tiles[i].size(); ++j)
        {
          int t = cnet_tiles[i][j];
          xmin = std::min(xmin, chipdb->tile_x(t));
          xmax = std::max(xmax, chipdb->tile_x(t));
          ymin = std::min(ymin, chipdb->tile_y(t));
          ymax = std::max(ymax, chipdb->tile_y(t));
        }
      cnet_xmin[i] = xmin;
      cnet_xmax[i] = xmax;
      cnet_ymin[i] = ymin;
      cnet_ymax[i] = ymax;
    }
}

void
Router::start(int net)
{
  visited.clear();
  
  frontier.clear();
  frontierq.clear();
  
  int source = net_source[net];
  cost[source] = 0;
  backptr[source] = -1;
  visit(source);
  
  for (const auto &p : net_route[net])
    {
      frontier.erase(p.second);
      
      cost[p.second] = 0;
      backptr[p.second] = -1;
      visit(p.second);
    }
}

void
Router::visit(int cn)
{
  assert(!frontier.contains(cn));
  visited.extend(cn);
  
  for (int cn2 : cnet_outs[cn])
    {
      if (visited.contains(cn2))
        continue;
      
      int cn2_cost = 1;  // base
      if (passes == max_passes)
        {
          if (demand[cn2])
            cn2_cost = 1000000;
        }
      else // if (passes > 1)
        {
          cn2_cost += historical_demand[cn2];
          cn2_cost *= (1 + 3 * demand[cn2]);
        }
      
      int new_cost = cost[cn] + cn2_cost;
      
      if (frontier.contains(cn2))
        {
          if (new_cost < cost[cn2])
            {
#if 0
              std::cout << "update cn " << cn2
                        << " old_cost " << cost[cn2]
                        << " new_cost " << new_cost << "\n";
#endif
              cost[cn2] = new_cost;
              backptr[cn2] = cn;
              frontierq.push(std::make_pair(cn2, new_cost));
            }
        }
      else
        {
          cost[cn2] = new_cost;
          backptr[cn2] = cn;
#if 0
          std::cout << "add cn " << cn2
                    << " cost " << new_cost << "\n";
#endif
          frontier.insert(cn2);
          frontierq.push(std::make_pair(cn2, new_cost));
        }
    }
}

int
Router::pop()
{
 L:
  assert(!frontierq.empty());
  int cn, cn_cost;
  std::tie(cn, cn_cost) = frontierq.pop();
  if (!frontier.contains(cn))
    goto L;
  
  // *logs << "pop " << cn << "\n";
  assert(cn_cost == cost[cn]);
  assert(frontierq.empty()
         || cn_cost <= frontierq.top().second);
  
  frontier.erase(cn);
  
  return cn;
}

void
Router::ripup(int net)
{
  for (const auto &p : net_route[net])
    {
      int cn = p.second;
      --demand[cn];
      if (demand[cn] == 1)
        --n_shared;
    }
  net_route[net].clear();
}

void
Router::traceback(int net, int target)
{
  int cn = target;
  while (cn >= 0)
    {
      int prev = backptr[cn];
      if (prev >= 0)
        {
          if (demand[cn] == 1)
            ++n_shared;
          ++demand[cn];
          net_route[net].push_back(std::make_pair(prev, cn));
        }
      cn = prev;
    }
}

void
Router::route()
{
  // d->dump();
  
  Model *top = d->top();
  
  std::set<Net *, IdLess> boundary_nets = top->boundary_nets(d);
  for (const auto &p : top->nets())
    {
      Net *n = p.second;
      if (contains(boundary_nets, n))
        continue;
      
#ifndef NDEBUG
      if (n->is_constant())
        {
          Value v = n->constant();
          assert(v == Value::ZERO || v == Value::ONE);
          
          for (auto p2 : n->connections())
            {
              Instance *inst = cast<Instance>(p2->node());
              
              if (models.is_lc(inst)
                  && p2->name() == "CIN")
                {
                  int cell = placement.at(inst);
                  const Location &loc = chipdb->cell_location[cell];
                  if (loc.pos() == 0)
                    continue;
                }
              
              assert(p2->is_input()
                     && !p2->is_bidir()
                     && p2->undriven() == v);
            }
        }
#endif
      
      int source = -1;
      std::vector<int> targets;
      
      // *logs << n->name() << "\n";
      
      for (auto p2 : n->connections())
        {
          assert(p2->connection() == n);
          assert(isa<Instance>(p2->node()));
          
          Instance *inst = cast<Instance>(p2->node());
          int cn = port_cnet(inst, p2);
          
          // like lutff_i/cin
          if (cn < 0)
            continue;
          
#if 1
          if (cnet_net[cn]
              && cnet_net[cn] != n)
            *logs << "n " << n->name()
                  << " cn " << cn
                  << " cnet_net[cn] " << cnet_net[cn]->name() << "\n";
#endif
            
          
          assert(cnet_net[cn] == nullptr
                 // like lutff_global/clk
                 || cnet_net[cn] == n);
          cnet_net[cn] = n;
          
          assert(!p2->is_bidir());
          if (p2->is_output())
            {
              assert(source < 0);
              source = cn;
            }
          else
            {
              assert(p2->is_input());
              targets.push_back(cn);
            }
        }
      
      if (source >= 0
          && !targets.empty())
        {
          ++n_nets;
          
#if 0
          *logs << "net " << n_nets
                << " " << n->name()
                << " " << source;
          for (int cn : targets)
            *logs << " " << cn;
          *logs << "\n";
#endif
          
          net_source.push_back(source);
          net_targets.push_back(std::move(targets));
          net_net.push_back(n);
        }
    }
  
  net_route.resize(n_nets);
  
  for (passes = 1; passes <= max_passes; ++passes)
    {
      for (int n = 0; n < n_nets; ++n)
        {
          current_net = n;
          
          const auto &targets = net_targets[n];
          
          if (passes > 1)
            {
              assert(net_route[n].size() > 0);
              for (const auto &p : net_route[n])
                {
                  if (demand[p.second] > 1)
                    goto M;
                }
              continue;
            }
          
        M:
          unrouted.clear();
          for (int i : targets)
            // not extend, e.g., lutff_global/clk
            unrouted.insert(i);
          
          ripup(n);
          
        L:
          // *logs << "start:";
          
          start(n);
          while (!frontier.empty())
            {
              int cn = pop();
              
              if (unrouted.contains(cn))
                {
                  unrouted.erase(cn);
                  traceback(n, cn);
                  
                  if (unrouted.empty())
                    break;
                  else
                    goto L;
                }
              else
                visit(cn);
            }
          
          if (!unrouted.empty())
            {
              *logs << net_source[n] << " ->";
              for (int t : targets)
                *logs << " " << t;
              *logs << "\n";
            }
          
          assert(unrouted.empty());
          
          // *logs << "\n";
          
          // check();
        }
      
      *logs << "  pass " << passes << ", " << n_shared << " shared.\n";
      if (!n_shared)
        break;
      
      if (passes > 1)
        {
          for (int i = 0; i < chipdb->n_nets; ++i)
            {
              if (demand[i] > 1)
                historical_demand[i] += demand[i];
            }
        }

#if 0
      if (n_shared < 5)
        {
          std::map<int, std::set<int>> net_route_reverse;

          for (int i = 0; i < n_nets; ++i)
              for (const auto &p : net_route[i])
                  if (demand[p.second] > 1)
                    net_route_reverse[p.second].insert(i);

          for (int i = 0; i < chipdb->n_nets; ++i)
            if (demand[i] > 1)
              {
                if (chipdb->net_tile_name.empty())
                  *logs << "    shared net #" << i << " (demand = " << demand[i] << ").\n";
                else
                  {
                    auto &net_tile_name = chipdb->net_tile_name.at(i);
                    int tile_x = chipdb->tile_x(net_tile_name.first), tile_y = chipdb->tile_y(net_tile_name.first);
                    *logs << "    shared net #" << i << " (demand = " << demand[i] << ") in tile " << tile_x << "," << tile_y << ": " << net_tile_name.second << "\n";
                  }
                for (auto j : net_route_reverse.at(i))
                  *logs << "      used by wire " << net_net[j]->name() << "\n";
              }
        }
#endif

#if 0
      for (int i = 0; i < n_nets; ++i)
        {
          for (const auto &p : net_route[i])
            {
              bool print = false;
              if (demand[p.second] > 1)
                {
                  print = true;
                  *logs << "demand " << i << " " << p.second << "\n";
                }
              if (print)
                {
                  *logs << "  net " << i << " " << net_net[i]->name() << "\n";
                  *logs << "  " << net_source[i] << " -> ";
                  for (int cn : net_targets[i])
                    *logs << " " << cn;
                  *logs << "\n";
                  *logs << "route\n";
                  for (const auto &p2 : net_route[i])
                    *logs << "  " << p2.first << " -> " << p2.second << "\n";
                }
            }
        }
#endif
    }
  
  if (n_shared)
    fatal("failed to route");
  
  int n_span4 = 0,
    n_span12 = 0;
  BitVector is_span4(chipdb->n_nets),
    is_span12(chipdb->n_nets);
  for (int i = 0; i < chipdb->n_tiles; ++i)
    {
      for (const auto &p : chipdb->tile_nets[i])
        {
          const std::string &name = p.first;
          int cn = p.second;
          
          if (is_span4[cn] || is_span12[cn])
            continue;
          
          if (is_prefix("span4_", name)
              || is_prefix("sp4_", name))      
            {
              is_span4[cn] = true;
              ++n_span4;
            }
          else if (is_prefix("span12_", name)
                   || is_prefix("sp12_", name))
            {
              is_span12[cn] = true;
              ++n_span12;
            }
        }
    }
  
  int n_span4_used = 0,
    n_span12_used = 0;
  for (const auto &v : net_route)
    for (const auto &p : v)
      {
        if (is_span4[p.second])
          ++n_span4_used;
        else if (is_span12[p.second])
          ++n_span12_used;
        
        int s = chipdb->find_switch(p.first, p.second);
        const Switch &sw = chipdb->switches[s];
        
        assert(!contains(chipdb->net_global, p.second));
        if (contains(chipdb->net_global, p.first) && (chipdb->device != "384"))
          {
            int g = chipdb->net_global.at(p.first);
            
            int cb_t = chipdb->tile_colbuf_tile.at(sw.tile);
            
            if (chipdb->device == "1k"
                && chipdb->tile_type[cb_t] == TileType::RAMT)
              {
                cb_t = chipdb->tile(chipdb->tile_x(cb_t),
                                    chipdb->tile_y(cb_t) - 1);
                assert(chipdb->tile_type[cb_t] == TileType::RAMB);
              }
            
            const CBit &colbuf_cbit = (chipdb->tile_nonrouting_cbits
                                       .at(chipdb->tile_type[cb_t])
                                       .at(fmt("ColBufCtrl.glb_netwk_" << g))
                                       [0]);
            conf.set_cbit(CBit(cb_t,
                               colbuf_cbit.row,
                               colbuf_cbit.col),
                          1);
          }
        
        conf.set_cbits(sw.cbits,
                       sw.in_val.at(p.first));
      }
  
  *logs << "\n"
        << "After routing:\n"
        << "span_4     " << n_span4_used << " / " << n_span4 << "\n"
        << "span_12    " << n_span12_used << " / " << n_span12 << "\n\n";
}

void
route(DesignState &ds, int max_passes)
{
  Router router(ds, max_passes);
  
  clock_t start = clock();
  router.route();
  clock_t end = clock();
  
  *logs << "  route time "
        << std::fixed << std::setprecision(2)
        << (double)(end - start) / (double)CLOCKS_PER_SEC << "s\n";
}
