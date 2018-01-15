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
#include "designstate.hh"

#include <queue>
#include <cassert>
#include <set>

class Promoter
{
  std::vector<uint8_t> global_classes;
  static const char *global_class_name(uint8_t gc);
  
  DesignState &ds;
  const ChipDB *chipdb;
  Design *d;
  Model *top;
  const Models &models;
  std::map<Instance *, uint8_t, IdLess> &gb_inst_gc;
  
  Net *const0;
  
  uint8_t port_gc(Port *conn, bool indirect);
  void pll_pass_through(Instance *inst, int cell, const char *p_name);
  bool routable(int g, Port *p);
  void make_routable(Net *n, int g);
  
public:
  Promoter(DesignState &ds_);
  
  void promote(bool do_promote);
};

const char *
Promoter::global_class_name(uint8_t gc)
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

Promoter::Promoter(DesignState &ds_)
  : global_classes{
      gc_clk, gc_cen, gc_sr, gc_rclke, gc_re,
    },
    ds(ds_),
    chipdb(ds.chipdb),
    d(ds.d),
    top(ds.top),
    models(ds.models),
    gb_inst_gc(ds.gb_inst_gc),
    const0(nullptr)
{
  for (const auto &p : top->nets())
    {
      if (p.second->is_constant()
          && p.second->constant() == Value::ZERO)
        {
          const0 = p.second;
          break;
        }
    }
  
  // will prune
  if (!const0)
    {
      const0 = top->add_net("$false");
      const0->set_is_constant(true);
      const0->set_constant(Value::ZERO);
    }
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
  else if (models.is_ioX(inst))
    {
      if (conn->name() == "INPUT_CLK"
          || conn->name() == "OUTPUT_CLK")
        return gc_clk;
    }
  else if (models.is_gb(inst)
           || models.is_warmboot(inst)
           || models.is_pllX(inst))
    ;
  else if(models.is_mac16(inst))
    {
      if(conn->name() == "CLK")
        return gc_clk;
      if(conn->name() == "CE")
        return gc_cen;
      if(conn->name() == "IRSTTOP" ||
         conn->name() == "IRSTBOT" ||
         conn->name() == "ORSTTOP" ||
         conn->name() == "ORSTBOT")
        return gc_sr;
    }
  else if(models.is_hfosc(inst))
    ;
  else if(models.is_lfosc(inst))
    ;
  else if(models.is_spram(inst))
   {
      if(conn->name() == "CLOCK")
        return gc_clk;
   }
  else if(models.is_rgba_drv(inst))
    ;
  else if(models.is_i2c(inst) || models.is_spi(inst))
   {
      if(conn->name() == "SBCLKI")
        return gc_clk;
   }
  else if(models.is_ledda_ip(inst))
   {
      if(conn->name() == "LEDDCLK")
        return gc_clk;
   }
  else
    {
      assert(models.is_ramX(inst));
      if (conn->name() == "WCLK"
          || conn->name() == "WCLKN"
          || conn->name() == "RCLK"
          || conn->name() == "RCLKN")
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

bool
Promoter::routable(int gc, Port *p)
{
  return (bool)((port_gc(p, true) & gc) == gc);
}

void
Promoter::pll_pass_through(Instance *inst, int cell, const char *p_name)
{
  Port *p = inst->find_port(p_name);
  Net *n = p->connection();
  if (!n)
    return;
  
  Net *t = top->add_net(n);
  p->connect(t);
  
  Instance *pass_inst = top->add_instance(models.lc);
  pass_inst->find_port("I0")->connect(t);
  pass_inst->find_port("I1")->connect(const0);
  pass_inst->find_port("I2")->connect(const0);
  pass_inst->find_port("I3")->connect(const0);
  pass_inst->set_param("LUT_INIT", BitVector(2, 2));
  pass_inst->find_port("O")->connect(n);
  
  const auto &p2 = chipdb->cell_mfvs.at(cell).at(p_name);
  int pass_cell = chipdb->loc_cell(Location(p2.first, 0));
  
  extend(ds.placement, pass_inst, pass_cell);
}

void
Promoter::make_routable(Net *n, int gc)
{
  Net *internal = nullptr;
  for (auto i = n->connections().begin();
       i != n->connections().end();)
    {
      Port *p = *i;
      ++i;
      
      if (!p->is_input())
        continue;
      if (routable(gc, p))
        continue;
      
      if (!internal)
        {
          internal = top->add_net(n);
          
          Instance *pass_inst = top->add_instance(models.lc);
          pass_inst->find_port("I0")->connect(n);
          pass_inst->find_port("I1")->connect(const0);
          pass_inst->find_port("I2")->connect(const0);
          pass_inst->find_port("I3")->connect(const0);
          pass_inst->set_param("LUT_INIT", BitVector(2, 2));
          pass_inst->find_port("O")->connect(internal);
        }
      p->connect(internal);
    }
}

void
Promoter::promote(bool do_promote)
{
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
  
  std::vector<std::pair<Instance *, int>> plls;
  for (const auto &p : ds.placement)
    {
      Instance *inst = p.first;
      int c = p.second;
      
      if (models.is_gb_io(inst))
        {
          Port *out = inst->find_port("GLOBAL_BUFFER_OUTPUT");
          if (out->connected())
            {
              auto loc = chipdb->cell_location[c];
              if (chipdb->loc_pin_glb_num.find(loc) == chipdb->loc_pin_glb_num.end())
                fatal(fmt("Not able to use pin " << ds.package.loc_pin.at(loc) << " for global buffer output"));
              int g = chipdb->loc_pin_glb_num.at(loc);
              for (uint8_t gc : global_classes)
                {
                  if (gc & (1 << g))
                    ++gc_used[gc];
                }
              make_routable(out->connection(), 1 << g);
            }
        }
      else if (models.is_hfosc(inst))
       {
         Port *out = inst->find_port("CLKHF");
         if (out->connected() && !inst->is_attr_set("ROUTE_THROUGH_FABRIC"))
           {
             int driven_glb = chipdb->get_oscillator_glb(c, "CLKHF");
             for (uint8_t gc : global_classes)
               {
                 if (gc & (1 << driven_glb))
                   ++gc_used[gc];
               }
             make_routable(out->connection(), 1 << driven_glb);
           }
       }
      else if (models.is_lfosc(inst))
       {
          Port *out = inst->find_port("CLKLF");
          if (out->connected() && !inst->is_attr_set("ROUTE_THROUGH_FABRIC"))
            {
              int driven_glb = chipdb->get_oscillator_glb(c, "CLKLF");
              
              for (uint8_t gc : global_classes)
                {
                  if (gc & (1 << driven_glb))
                    ++gc_used[gc];
                }
              make_routable(out->connection(), 1 << driven_glb);
            }
        }
      else if (models.is_pllX(inst))
        {
          plls.push_back(std::make_pair(inst, c));
          
          Port *a = inst->find_port("PLLOUTGLOBAL");
          if (!a)
            a = inst->find_port("PLLOUTGLOBALA");
          assert(a);
          if (a->connected())
            {
              const auto &p2 = chipdb->cell_mfvs.at(c).at("PLLOUT_A");
              Location loc(p2.first, std::stoi(p2.second));
              int g = chipdb->loc_pin_glb_num.at(loc);
              for (uint8_t gc : global_classes)
                {
                  if (gc & (1 << g))
                    ++gc_used[gc];
                }
              make_routable(a->connection(), 1 << g);
            }
          
          Port *b = inst->find_port("PLLOUTGLOBALB");
          if (b && b->connected())
            {
              const auto &p2 = chipdb->cell_mfvs.at(c).at("PLLOUT_B");
              Location loc(p2.first, std::stoi(p2.second));
              int g = chipdb->loc_pin_glb_num.at(loc);
              for (uint8_t gc : global_classes)
                {
                  if (gc & (1 << g))
                    ++gc_used[gc];
                }
              make_routable(b->connection(), 1 << g);
            }
        }
    }
  
  for (const auto &p : plls)
    {
      Instance *inst = p.first;
      int c = p.second;
      pll_pass_through(inst, c, "LOCK");
      pll_pass_through(inst, c, "SDO");
    }
  
  std::set<Net *, IdLess> boundary_nets = top->boundary_nets(d);
  
  std::set<std::pair<int, int>, std::greater<std::pair<int, int>>> promote_q;
  std::map<int, uint8_t> net_gc;
  std::map<int, Port *> net_driver;
  for (int i = 1; i < n_nets; ++i) // skip 0, nullptr
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
          && ((models.is_gbX(cast<Instance>(driver->node()))
               && driver->name() == "GLOBAL_BUFFER_OUTPUT")
              || (models.is_pllX(cast<Instance>(driver->node()))
                  && (driver->name() == "PLLOUTGLOBAL"
                      || driver->name() == "PLLOUTGLOBALA"
                      || driver->name() == "PLLOUTGLOBALB"))
              || (models.is_hfosc(cast<Instance>(driver->node())) 
                    && driver->name() == "CLKHF" && 
                        !cast<Instance>(driver->node())->is_attr_set("ROUTE_THROUGH_FABRIC"))
              || (models.is_lfosc(cast<Instance>(driver->node())) 
                    && driver->name() == "CLKLF" && 
                        !cast<Instance>(driver->node())->is_attr_set("ROUTE_THROUGH_FABRIC"))))
        {
          Instance *gb_inst = cast<Instance>(driver->node());
          
          uint8_t gc = max_gc ? max_gc : gc_clk;
          
          ++n_global;
          ++gc_global[gc];
          
          if (models.is_gbX(gb_inst) || 
              models.is_hfosc(gb_inst) ||
              models.is_lfosc(gb_inst))
            {
              if (driver->connected())
                make_routable(driver->connection(), gc);
              
              extend(gb_inst_gc, gb_inst, gc);
            }
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
  
  d->prune();
}

void
promote_globals(DesignState &ds, bool do_promote)
{
  Promoter promoter(ds);
  promoter.promote(do_promote);
}
