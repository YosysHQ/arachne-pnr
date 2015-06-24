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

#include <cassert>
#include <ostream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <set>
#include <unordered_set>
#include <map>
#include <vector>
#include <ctime>

class Router;

class CompareSecond
{
public:
  CompareSecond() {}
  
  bool operator()(const std::pair<int, int> &lhs,
		  const std::pair<int, int> &rhs) const
  {
    return lhs.second > rhs.second;
  }
};

class Router
{
  const ChipDB *chipdb;
  Design *d;
  Configuration &conf;
  const std::unordered_map<Instance *, Location> &placement;
  
  Models models;
  
  BitVector cnet_global,
    cnet_local;
  std::vector<std::vector<int>> cnet_outs;
  
  std::unordered_map<std::string, std::pair<std::string, bool>> ram_gate_chip;
  
  std::vector<Net *> cnet_net;
  std::vector<std::vector<int>> cnet_tiles;
  // cnet_bbox
  std::vector<int> cnet_xmin,
    cnet_xmax,
    cnet_ymin,
    cnet_ymax;
  
  int n_nets;  // to route
  std::vector<int> net_source;
  std::vector<std::vector<int>> net_targets;
  
  static const int max_passes = 50;
  int passes;
  
  int n_shared;
  std::vector<int> demand;
  std::vector<int> historical_demand;
  std::vector<std::vector<std::pair<int, int>>> net_route;
  
  // per net
  int current_net;
  UllmanSet current_net_target_tiles;
  UllmanSet unrouted;
  
  UllmanSet visited;
  
  UllmanSet frontier;
  // cn, cost[cn]
  PriorityQ<std::pair<int, int>, CompareSecond> frontierq;
  
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
  Router(const ChipDB *cdb,
	 Design *d_,
	 Configuration &c,
	 const std::unordered_map<Instance *, Location> &p);
  
  std::vector<Net *> route();
};

int
Router::port_cnet(Instance *inst, Port *p)
{
  const auto &p_name = p->name();
  const auto &loc = placement.at(inst);
  int t = chipdb->tile(loc.x(), loc.y());
  
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
	return -1;
      else if (p_name == "COUT")
	tile_net_name = fmt("lutff_" << loc.pos() << "/cout");
      else
	{
	  assert(p_name == "O");
	  tile_net_name = fmt("lutff_" << loc.pos() << "/out");
	}
    }
  else if (models.is_io(inst))
    {
      if (p_name == "LATCH_INPUT_VALUE")
	tile_net_name = fmt("io_global/latch");
      if (p_name == "CLOCK_ENABLE")
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
      else
	{
	  assert(p_name == "D_IN_1");
	  tile_net_name = fmt("io_" << loc.pos() << "/D_IN_1");
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
  else
    {
      assert(models.is_ramX(inst));
      
      auto r = ram_gate_chip.at(p_name);
      tile_net_name = r.first;
      if (r.second)
	// ramb tile
	t = chipdb->tile(loc.x(),
			 loc.y() - 1);
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

Router::Router(const ChipDB *cdb, 
	       Design *d_, 
	       Configuration &c,
	       const std::unordered_map<Instance *, Location> &placement_)
  : chipdb(cdb),
    d(d_),
    conf(c),
    placement(placement_),
    models(d),
    cnet_global(chipdb->n_nets),
    cnet_local(chipdb->n_nets),
    cnet_outs(chipdb->n_nets),
    cnet_net(chipdb->n_nets, nullptr),
    cnet_tiles(chipdb->n_nets),
    cnet_xmin(chipdb->n_nets),
    cnet_xmax(chipdb->n_nets),
    cnet_ymin(chipdb->n_nets),
    cnet_ymax(chipdb->n_nets),
    n_nets(0),
    n_shared(0),
    demand(chipdb->n_nets, 0),
    historical_demand(chipdb->n_nets, 0),
    current_net_target_tiles(chipdb->n_tiles),
    unrouted(chipdb->n_nets),
    visited(chipdb->n_nets),
    frontier(chipdb->n_nets),
    backptr(chipdb->n_nets),
    cost(chipdb->n_nets)
{
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
  
  for (int i = 0; i <= 7; ++i)
    extend(ram_gate_chip,
	   fmt("MASK[" << i << "]"),
	   std::make_pair(fmt("ram/MASK_" << i), true));
  for (int i = 8; i <= 15; ++i)
    extend(ram_gate_chip,
	   fmt("MASK[" << i << "]"),
	   std::make_pair(fmt("ram/MASK_" << i), false));
  
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
	 "RE",
	 std::make_pair("ram/RE", false));

  extend(ram_gate_chip,
	 "WCLKE",
	 std::make_pair("ram/WCLKE", true));
  extend(ram_gate_chip,
	 "WCLK",
	 std::make_pair("ram/WCLK", true));
  extend(ram_gate_chip,
	 "WE",
	 std::make_pair("ram/WE", true));
  
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
      for (int j = 1; j < cnet_tiles[i].size(); ++j)
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
      
      if (cnet_local[cn2])
	{
	  assert(cnet_tiles[cn2].size() == 1);
	  int t = cnet_tiles[cn2][0];
	  if (!current_net_target_tiles.contains(t))
	    continue;
	}
      
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
  
#if 0
  std::cout << "pop cn " << cn
	    << " cn_cost " << cn_cost 
	    << " cost[cn] " << cost[cn] << "\n";
#endif
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

std::vector<Net *>
Router::route()
{
  // d->dump();
  
  Model *top = d->top();
  
  std::unordered_set<Net *> boundary_nets = top->boundary_nets(d);
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
		  const Location &loc = placement.at(inst);
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
	  net_source.push_back(source);
	  net_targets.push_back(std::move(targets));
	}
    }
  
  net_route.resize(n_nets);
  
  for (passes = 1; passes <= max_passes; ++passes)
    {
      for (int n = 0; n < n_nets; ++n)
	{
	  current_net = n;
	  
	  const auto &targets = net_targets[n];
	  
	  current_net_target_tiles.clear();
	  for (int cn : targets)
	    {
	      assert(cnet_tiles[cn].size() == 1);
	      int t = cnet_tiles[cn][0];
	      current_net_target_tiles.insert(t);
	    }
	  
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
    }
  
  if (n_shared)
    fatal("failed to route");
  
  for (const auto &v : net_route)
    for (const auto &p : v)
      {
	int s = chipdb->find_switch(p.first, p.second);
	const Switch &sw = chipdb->switches[s];
	
	assert(p.second >= chipdb->n_global_nets);
	if (p.first < chipdb->n_global_nets)
	  {
	    // FIXME no default
	    int cb_t = lookup_or_default(chipdb->tile_colbuf_tile, sw.tile, sw.tile);
	    
	    // FIXME
	    if (chipdb->tile_type[cb_t] == TileType::RAMT_TILE)
	      {
		cb_t = chipdb->tile(chipdb->tile_x(cb_t),
				    chipdb->tile_y(cb_t) - 1);
		assert(chipdb->tile_type[cb_t] == TileType::RAMB_TILE);
	      }
	    
	    const CBit &colbuf_cbit = (chipdb->tile_nonrouting_cbits
				       .at(chipdb->tile_type[cb_t])
				       .at(fmt("ColBufCtrl.glb_netwk_" << p.first))
				       [0]);
	    conf.set_cbit(CBit(chipdb->tile_x(cb_t),
			       chipdb->tile_y(cb_t),
			       colbuf_cbit.row,
			       colbuf_cbit.col),
			  1);
	  }
	
	conf.set_cbits(sw.cbits,
		       sw.in_val.at(p.first));
      }
  
  return std::move(cnet_net);
}

std::vector<Net *>
route(const ChipDB *chipdb, 
      Design *d, 
      Configuration &conf,
      const std::unordered_map<Instance *, Location> &placement)
{
  Router router(chipdb, d, conf, placement);
  
  clock_t start = clock();
  std::vector<Net *> cnet_net = router.route();
  clock_t end = clock();
  
  *logs << "  route time "
	<< std::fixed << std::setprecision(2)
	<< (double)(end - start) / (double)CLOCKS_PER_SEC << "s\n";
  
  return std::move(cnet_net);
}
