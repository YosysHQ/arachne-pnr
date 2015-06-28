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

#include "netlist.hh"
#include "global.hh"
#include "chipdb.hh"
#include "casting.hh"
#include "util.hh"

#include <queue>
#include <cassert>
#include <set>

std::vector<uint8_t> global_classes = {
  gc_clk, gc_cen, gc_sr, gc_rclke, gc_re,
};

const char *
global_class_name(uint8_t gc)
{
  switch(gc)
    {
    case gc_clk: return "clk";
    case gc_cen: return "cen/wclke";
    case gc_sr: return "sr/we";
    case gc_rclke: return "rclke";
    case gc_re: return "re";
    default:
      abort();
      return nullptr;
    }
}

class Promoter
{
  Design *d;
  Models models;
  
  std::map<Instance *, uint8_t, IdLess> gb_inst_gc;
  
  uint8_t port_gc(Port *conn, bool indirect);
  
public:
  Promoter(const ChipDB *chipdb, Design *d);
  
  std::map<Instance *, uint8_t, IdLess> promote(bool do_promote);
};

Promoter::Promoter(const ChipDB *cdb, Design *d_)
  : d(d_),
    models(d)

{
}

uint8_t
Promoter::port_gc(Port *conn, bool indirect)
{
  Instance *inst = dyn_cast<Instance>(conn->node());
  assert(inst);
  
  if (models.is_lc(inst))
    {
      if (conn->name() == "CLK")
	return gc_clk;
      else if (conn->name() == "CEN")
	return gc_cen;
      else if (conn->name() == "SR")
	return gc_sr;
      else if (indirect
	       && (conn->name() == "I0"
		   || conn->name() == "I1"
		   || conn->name() == "I2"
		   || conn->name() == "I3"))
	return gc_clk;
    }
  else if (models.is_io(inst))
    {
      if (conn->name() == "INPUT_CLOCK"
	  || conn->name() == "OUTPUT_CLOCK")
	return gc_clk;
    }
  else
    {
      assert(models.is_ramX(inst));
      if (conn->name() == "WCLK"
	  || conn->name() == "RCLK")
	return gc_clk;
      else if (conn->name() == "WCLKE")
	return gc_wclke;
      else if (conn->name() == "WE")
	return gc_we;
      else if (conn->name() == "RCLKE")
	return gc_rclke;
      else if (conn->name() == "RE")
	return gc_re;
    }
  
  return 0;
}

std::map<Instance *, uint8_t, IdLess>
Promoter::promote(bool do_promote)
{
  Model *top = d->top();
  // top->dump();
  
  std::vector<Net *> nets;
  std::map<Net *, int, IdLess> net_idx;
  std::tie(nets, net_idx) = top->index_nets();
  int n_nets = nets.size();
  
  int n_global = 0;
  
  std::map<uint8_t, int> gc_global;
  std::map<uint8_t, int> gc_used;
  for (uint8_t gc : global_classes)
    {
      extend(gc_global, gc, 0);
      extend(gc_used, gc, 0);
    }
  
  std::set<Net *, IdLess> boundary_nets = top->boundary_nets(d);
  
  std::set<std::pair<int, int>, std::greater<std::pair<int, int>>> promote_q;
  std::map<int, uint8_t> net_gc;
  std::map<int, Port *> net_driver;
  for (int i = 0; i < n_nets; ++i)
    {
      Net *n = nets[i];
      if (contains(boundary_nets, n)
	  || n->is_constant())
	continue;
      
      std::map<uint8_t, int> n_gc;
      for (uint8_t gc : global_classes)
	extend(n_gc, gc, 0);
      
      Port *driver = nullptr;
      for (Port *conn : n->connections())
	{
	  assert(!conn->is_bidir());
	  if (conn->is_output())
	    {
	      assert(!driver);
	      driver = conn;
	    }
	  
	  int gc = port_gc(conn, false);
	  if (gc)
	    ++n_gc[gc];
	}
      
      int max_gc = 0;
      int max_n = 0;
      for (const auto &p : n_gc)
	{
	  if (p.second > max_n)
	    {
	      max_gc = p.first;
	      max_n = p.second;
	    }
	}
      
      if (driver
	  && isa<Instance>(driver->node())
	  && models.is_gb(cast<Instance>(driver->node()))
	  && driver->name() == "GLOBAL_BUFFER_OUTPUT")
	{
	  Instance *gb_inst = cast<Instance>(driver->node());
	  
	  uint8_t gc = max_gc ? max_gc : gc_clk;
	  
	  ++n_global;
	  ++gc_global[gc];
	  extend(gb_inst_gc, gb_inst, gc);
	  for (uint8_t gc2 : global_classes)
	    {
	      if ((gc2 & gc) == gc)
		++gc_used[gc2];
	    }
	}
      else if (do_promote
	       && driver
	       && max_gc
	       && max_n > 4)
	{
	  extend(net_driver, i, driver);
	  extend(net_gc, i, max_gc);
	  promote_q.insert(std::make_pair(max_n, i));
	}
    }
  
  int n_promoted = 0;
  std::map<uint8_t, int> gc_promoted;
  for (int gc : global_classes)
    extend(gc_promoted, gc, 0);
  
  while(!promote_q.empty())
    {
      std::pair<int, int> p = *promote_q.begin();
      promote_q.erase(promote_q.begin());
      assert(promote_q.empty()
	     || promote_q.begin()->first <= p.first);
      
      Net *n = nets[p.second];
      uint8_t gc = net_gc.at(p.second);
      
      for (int gc2 : global_classes)
	{
	  int k2 = 0;
	  for (int i = 0; i < 8; ++i)
	    {
	      if (gc2 & (1 << i))
		++k2;
	    }
	  
	  if ((gc2 & gc) == gc)
	    {
	      if (gc_used.at(gc2) >= k2)
		goto L;
	    }
	}
      
      {
	++n_promoted;
	++gc_promoted[gc];
	
	Instance *gb_inst = top->add_instance(models.gb);
	Net *t = top->add_net(n);
	
	int n_conn = 0;
	int n_conn_promoted = 0;
	for (auto i = n->connections().begin();
	     i != n->connections().end();)
	  {
	    Port *conn = *i;
	    ++i;
	    if (conn->is_output()
		|| conn->is_bidir())
	      continue;
	    
	    ++n_conn;
	    int conn_gc = port_gc(conn, true);
	    if ((conn_gc & gc) == gc)
	      {
		++n_conn_promoted;
		conn->connect(t);
	      }
	  }
	
	gb_inst->find_port("USER_SIGNAL_TO_GLOBAL_BUFFER")->connect(n);
	gb_inst->find_port("GLOBAL_BUFFER_OUTPUT")->connect(t);
	
	++n_global;
	++gc_global[gc];
	extend(gb_inst_gc, gb_inst, gc);
	for (uint8_t gc2 : global_classes)
	  {
	    if ((gc2 & gc) == gc)
	      ++gc_used[gc2];
	  }
	
	*logs << "  promoted " << n->name()
	      << ", " << n_conn_promoted << " / " << n_conn << "\n";
      }
    L:;
    }
  
  *logs << "  promoted " << n_promoted << " nets\n";
  for (const auto &p : gc_promoted)
    {
      if (p.second)
	*logs << "    " << p.second << " " << global_class_name(p.first) << "\n";
    }
  *logs << "  " << n_global << " globals\n";
  for (const auto &p : gc_global)
    {
      if (p.second)
	*logs << "    " << p.second << " " << global_class_name(p.first) << "\n";
    }
  
  return gb_inst_gc;
}

std::map<Instance *, uint8_t, IdLess>
promote_globals(const ChipDB *chipdb, Design *d, bool do_promote)
{
  Promoter promoter(chipdb, d);
  return promoter.promote(do_promote);
}
