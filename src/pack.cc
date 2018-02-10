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
#include "chipdb.hh"
#include "carry.hh"
#include "designstate.hh"

#include <cstring>

class Packer
{
  const ChipDB *chipdb;
  const Package &package;
  Design *d;
  Models &models;
  Model *top;
  CarryChains &chains;
  
  int n_dff_pass_through,
    n_carry_pass_through;
  
  Net *const0;
  Net *const1;
  
  std::set<Instance *, IdLess> ready;
  
  void lc_from_dff(Instance *lc_inst, Instance *dff_inst);
  void lc_from_lut(Instance *lc_inst, Instance *lut_inst);
  void pass_through_lc(Instance *lc_inst, Port *in);
  void carry_pass_through_lc(Instance *lc_inst, Port *cout);
  void lc_from_carry(Instance *lc_inst, Instance *carry_inst);
  
  Port *driver(Net *n);
  Instance *find_carry_lc(Instance *c);

  void pack_dffs();
  void pack_luts();
  void pack_carries_from(Instance *f);
  void pack_carries();
  
  int count_extra_cells(CellType ct);
  
public:
  Packer(DesignState &ds);
  
  void pack();
};

Packer::Packer(DesignState &ds)
  : chipdb(ds.chipdb), 
    package(ds.package),
    d(ds.d), 
    models(ds.models),
    top(ds.top),
    chains(ds.chains),
    n_dff_pass_through(0), 
    n_carry_pass_through(0),
    const0(nullptr), 
    const1(nullptr)
{
  for (const auto &p : top->nets())
    {
      if (p.second->is_constant())
        {
          if (p.second->constant() == Value::ONE)
            const1 = p.second;
          else
            {
              assert(p.second->constant() == Value::ZERO);
              const0 = p.second;
            }
        }
      if (const0 && const1)
        break;
    }
  
  // will prune
  if (!const0)
    {
      const0 = top->add_net("$false");
      const0->set_is_constant(true);
      const0->set_constant(Value::ZERO);
    }
  if (!const1)
    {
      const1 = top->add_net("$true");
      const1->set_is_constant(true);
      const1->set_constant(Value::ONE);
    }
}

void
Packer::lc_from_dff(Instance *lc_inst,
                    Instance *dff_inst)
{
  const std::string &dff_name = dff_inst->instance_of()->name();
  const char *suffix = &dff_name[6];
  
  bool neg_clk = false;
  if (*suffix == 'N')
    {
      neg_clk = true;
      ++suffix;
    }
  
  bool cen = false;
  if (*suffix == 'E')
    {
      cen = true;
      ++suffix;
    }
  
  bool async_sr = false;
  bool set_noreset = false;
  bool sr = false;
  if (!strcmp(suffix, "S"))
    {
      set_noreset = true;
      async_sr = true;
      sr = true;
    }
  else if (!strcmp(suffix, "SS"))
    {
      set_noreset = true;
      sr = true;
    }
  else if (!strcmp(suffix, "R"))
    {
      async_sr = true;
      sr = true;
    }
  else if (!strcmp(suffix, "SR"))
    {
      sr = true;
    }
  else
    assert(*suffix == '\0');
  
  lc_inst->find_port("O")->connect(dff_inst->find_port("Q")->connection());
  lc_inst->find_port("CLK")->connect(dff_inst->find_port("C")->connection());
  
  if (neg_clk)
    lc_inst->set_param("NEG_CLK", BitVector(1, 1));
  
  if (cen)
    lc_inst->find_port("CEN")->connect(dff_inst->find_port("E")->connection());
  else
    lc_inst->find_port("CEN")->connect(const1);
  
  if (sr)
    {
      if (set_noreset)
        {
          lc_inst->find_port("SR")->connect(dff_inst->find_port("S")->connection());
          lc_inst->set_param("SET_NORESET", BitVector(1, 1));
        }
      else
        {
          lc_inst->find_port("SR")->connect(dff_inst->find_port("R")->connection());
        }
      
      if (async_sr)
        lc_inst->set_param("ASYNC_SR", BitVector(1, 1));
    }
  else
    {
      lc_inst->find_port("SR")->connect(const0);
    }
  
  lc_inst->set_param("DFF_ENABLE", BitVector(1, 1));
  
  lc_inst->merge_attrs(dff_inst);
}

void
Packer::lc_from_lut(Instance *lc_inst,
                    Instance *lut_inst)
{
  lc_inst->find_port("I0")->connect(lut_inst->find_port("I0")->connection());
  lc_inst->find_port("I1")->connect(lut_inst->find_port("I1")->connection());
  lc_inst->find_port("I2")->connect(lut_inst->find_port("I2")->connection());
  lc_inst->find_port("I3")->connect(lut_inst->find_port("I3")->connection());
  
  if (lut_inst->self_has_param("LUT_INIT"))
    lc_inst->set_param("LUT_INIT", lut_inst->self_get_param("LUT_INIT"));
  
  lc_inst->merge_attrs(lut_inst);
}

void
Packer::pass_through_lc(Instance *lc_inst, Port *in)
{
  lc_inst->find_port("I0")->connect(in->connection());
  lc_inst->find_port("I1")->connect(const0);
  lc_inst->find_port("I2")->connect(const0);
  lc_inst->find_port("I3")->connect(const0);
  
  lc_inst->set_param("LUT_INIT", BitVector(2, 2));
  
  ++n_dff_pass_through;
}

void
Packer::carry_pass_through_lc(Instance *lc_inst, Port *cout)
{
  Net *n = cout->connection();
  Net *t = top->add_net(n);
  
  cout->connect(t);
  
  lc_inst->find_port("I3")->connect(t);
  lc_inst->find_port("O")->connect(n);
  lc_inst->set_param("LUT_INIT", BitVector(16, 0xff00)); // 1111111100000000

  ++n_carry_pass_through;
}

void
Packer::lc_from_carry(Instance *lc_inst,
                      Instance *carry_inst)
{
  assert((lc_inst->find_port("I1")->connection()
          == carry_inst->find_port("I0")->connection())
         && (lc_inst->find_port("I2")->connection()
             == carry_inst->find_port("I1")->connection()));
  
  lc_inst->find_port("CIN")->connect(carry_inst->find_port("CI")->connection());
  lc_inst->find_port("COUT")->connect(carry_inst->find_port("CO")->connection());
  
  lc_inst->set_param("CARRY_ENABLE", BitVector(1, 1));
}

void
Packer::pack_dffs()
{
  const auto &instances = top->instances();
  for (auto i = instances.begin(); i != instances.end();)
    {
      Instance *inst = *i;
      ++i;
      
      if (models.is_dff(inst))
        {
          Instance *lc_inst = top->add_instance(models.lc);
          
          Port *d_port = inst->find_port("D");
          
          Port *d_driver = d_port->connection_other_port();
          
          Instance *lut_inst = nullptr;
          if (d_driver)
            {
              Instance *d_driver_inst = dyn_cast<Instance>(d_driver->node());
              if (d_driver_inst
                  && models.is_lut4(d_driver_inst)
                  && d_driver->name() == "O")
                lut_inst = d_driver_inst;
            }
          
          lc_from_dff(lc_inst, inst);
          
          if (lut_inst)
            lc_from_lut(lc_inst, lut_inst);
          else
            pass_through_lc(lc_inst, d_port);
          
          inst->remove();
          delete inst;
          
          if (lut_inst)
            {
              if (i != instances.end()
                  && *i == lut_inst)
                ++i;
              
              lut_inst->remove();
              delete lut_inst;
            }
        }
    }
}

void
Packer::pack_luts()
{
  const auto &instances = top->instances();
  for (auto i = instances.begin(); i != instances.end();)
    {
      Instance *inst = *i;
      ++i;
      if (models.is_lut4(inst))
        {
          Instance *lc_inst = top->add_instance(models.lc);
          
          lc_from_lut(lc_inst, inst);
          
          lc_inst->find_port("O")->connect(inst->find_port("O")->connection());
          
          inst->remove();
          delete inst;
        }
    }      
}

Port *
Packer::driver(Net *n)
{
  if (!n)
    return nullptr;
  for (Port *p : n->connections())
    {
      if (p->is_output()
          || p->is_bidir())
        return p;
    }
  return nullptr;
}

Instance *
Packer::find_carry_lc(Instance *c)
{
  Port *ci = c->find_port("CI");
  Net *ci_conn = ci->connection();

  /* FIXME if two connections (CO -> CI), could return a LUT that
     matches I1/I2 */
  if (!ci_conn
      || ci_conn->is_constant()
      || ci_conn->connections().size() != 3)
    return nullptr;
  
  // driver is previous COUT
  
  Net *i0_conn = c->find_port("I0")->connection(),
    *i1_conn = c->find_port("I1")->connection();
  
  for (Port *p : ci_conn->connections())
    {
      if (Instance *p_inst = dyn_cast<Instance>(p->node()))
        {
          if (models.is_lc(p_inst)
              && p->name() == "I3"
              && i0_conn == p_inst->find_port("I1")->connection()
              && i1_conn == p_inst->find_port("I2")->connection())
            return p_inst;
        }
    }
  
  return nullptr;
}

void
Packer::pack_carries_from(Instance *f)
{
  unsigned max_chain_length = (chipdb->height - 2)*8;
  
  std::vector<Instance *> chain;
  
  Net *global_clk = nullptr,
    *global_cen = nullptr,
    *global_sr = nullptr;
  for (Instance *c = f; c;)
    {
      Port *out = c->find_port("CO");
      Net *out_conn = out->connection();
      if (out_conn
          && chain.size() == max_chain_length - 1)
        {
          // break chain
          Instance *out_lc_inst = top->add_instance(models.lc);
          
          carry_pass_through_lc(out_lc_inst, chain.back()->find_port("COUT"));
          chain.push_back(out_lc_inst);
          
          chains.chains.push_back(chain);
          chain.clear();
        }
      
      Port *in = c->find_port("CI");
      Net *in_conn = in->connection();
      
      if (chain.size() % 8 == 0)
        {
          global_clk = nullptr;
          global_cen = nullptr;
          global_sr = nullptr;
        }
      
      if (chain.empty()
          && in_conn
          && !in_conn->is_constant())
        {
          // carry in
          Instance *in_lc_inst = top->add_instance(models.lc);
          
          Net *t = top->add_net(in_conn);
          
          in_lc_inst->find_port("COUT")->connect(t);
          in_lc_inst->find_port("I0")->connect(const0);
          in_lc_inst->find_port("I1")->connect(in_conn);
          in_lc_inst->find_port("I2")->connect(const0);
          in_lc_inst->find_port("I3")->connect(const0);
          in_lc_inst->find_port("CIN")->connect(const1);
          
          in_lc_inst->set_param("CARRY_ENABLE", BitVector(1, 1));
          
          chain.push_back(in_lc_inst);
          
          in->connect(t);
          in_conn = t;
          
          ++n_carry_pass_through;
        }
      
      Instance *lc_inst = find_carry_lc(c);
      
      if (lc_inst)
        {
          Net *clk = lc_inst->find_port("CLK")->connection(),
            *cen = lc_inst->find_port("CEN")->connection(),
            *sr = lc_inst->find_port("SR")->connection();
          
          if ((global_clk
               && global_clk != clk)
              || (global_cen
                  && global_cen != cen)
              || (global_sr
                  && global_sr != sr))
            {
              lc_inst = nullptr;
              goto L;
            }
          if (!global_clk)
            global_clk = clk;
          if (!global_cen)
            global_cen = cen;
          if (!global_sr)
            global_sr = sr;
        }
      
      if (!lc_inst)
        {
        L:
          lc_inst = top->add_instance(models.lc);
          
          lc_inst->find_port("I1")->connect(c->find_port("I0")->connection());
          lc_inst->find_port("I2")->connect(c->find_port("I1")->connection());
          
          if (!in_conn
              || in_conn->is_constant()
              || in_conn->connections().size() == 2)
            {
              // could try to pack lut here
            }
          else
            {
              Port *p = chain.back()->find_port("COUT");
              assert(p && p->connection() == in_conn);
              carry_pass_through_lc(lc_inst, p);
              
              c->find_port("CI")->connect(p->connection());
            }
        }
      
      lc_from_carry(lc_inst, c);
      chain.push_back(lc_inst);
      
      Instance *next_c = nullptr;
      if (out_conn)
        {
          for (Port *p : out_conn->connections())
            {
              if (Instance *inst = dyn_cast<Instance>(p->node()))
                {
                  if (models.is_carry(inst)
                      && p->name() == "CI")
                    {
                      if (next_c)
                        extend(ready, inst);
                      else
                        next_c = inst;
                    }
                }
            }
        }
      
      c->remove();
      delete c;
      
      if (!next_c
          && out_conn)
        {
          assert(chain.size() < max_chain_length);
          
          Port *p = chain.back()->find_port("COUT");
          assert(p && p->connection() == out_conn);
          
          Instance *lc2_inst = nullptr;
          
          // COUT might drive a LC I3
          if (out_conn->connections().size() == 2)
            {
              Port *consumer = p->connection_other_port();
              if (consumer->name() == "I3"
                  && isa<Instance>(consumer->node()))
                {
                  Instance *inst = cast<Instance>(consumer->node());
                  if (models.is_lc(inst))
                    lc2_inst = inst;
                }
            }
          bool break_chain = false;
          
          if (lc2_inst)
            {
              Net *clk = lc2_inst->find_port("CLK")->connection(),
                *cen = lc2_inst->find_port("CEN")->connection(),
                *sr = lc2_inst->find_port("SR")->connection();
              
                if ((global_clk
                     && global_clk != clk)
                    || (global_cen
                        && global_cen != cen)
                    || (global_sr
                        && global_sr != sr))
                  {
                    break_chain = true;
                  }
                  
              if (!global_clk)
                global_clk = clk;
              if (!global_cen)
                global_cen = cen;
              if (!global_sr)
                global_sr = sr;
            }
        
          if (!lc2_inst)
            {
              lc2_inst = top->add_instance(models.lc);
              carry_pass_through_lc(lc2_inst, p);
            }
          
          if(break_chain)
            {
              // break chain
              
              Instance *out_lc_inst = top->add_instance(models.lc);
              
              carry_pass_through_lc(out_lc_inst, chain.back()->find_port("COUT"));
              chain.push_back(out_lc_inst);
              
              chains.chains.push_back(chain);
              chain.clear();
              
              chain.push_back(lc2_inst);
            }
          else
            {
              chain.push_back(lc2_inst);
            }
        }
      c = next_c;
    }
  
  chains.chains.push_back(chain);
  chain.clear();
}

void
Packer::pack_carries()
{
  const auto &instances = top->instances();
  
  for (Instance *inst : instances)
    {
      if (models.is_carry(inst))
        {
          Port *in = inst->find_port("CI");
          Net *in_conn = in->connection();
          Port *p = driver(in_conn);
          if (!p
              || !isa<Instance>(p->node())
              || !models.is_carry(cast<Instance>(p->node())))
            extend(ready, inst);
        }
    }
  
  while (!ready.empty())
    {
      Instance *inst = front(ready);
      ready.erase(inst);
      pack_carries_from(inst);
    }
  
  std::set<Instance *, IdLess> done;
  for (const auto &ch : chains.chains)
    for (Instance *inst : ch)
      extend(done, inst);
  
  for (Instance *inst : instances)
    {
      if (!models.is_carry(inst))
        continue;
      
      if (!contains(done, inst))
        fatal("carry chain loop");
    }
}

int
Packer::count_extra_cells(CellType ct)
{
  int n = 0;
  for (auto cell : chipdb->cell_type_cells[cell_type_idx(ct)]) {
    if (!contains(chipdb->cell_locked_pkgs.at(cell), package.name))
      n++;
  }
  return n;
}


void
Packer::pack()
{
  pack_dffs();
  pack_luts();
  pack_carries();
  
  d->prune();
  // d->dump();
  
  int n_ramt_tiles = 0;
  for (int i = 0; i < chipdb->n_tiles; ++i)
    {
      if (chipdb->tile_type[i] == TileType::RAMT)
        ++n_ramt_tiles;
    }
  
  int n_io = 0,
    n_lc = 0,
    n_lc_carry = 0,
    n_lc_dff = 0,
    n_lc_carry_dff = 0,
    n_gb = 0,
    n_gb_io = 0,
    n_bram = 0,
    n_pll = 0,
    n_mac16 = 0,
    n_spram = 0,
    n_lfosc = 0,
    n_hfosc = 0,
    n_rgba_drv = 0,
    n_ledda_ip = 0,
    n_i2c = 0,
    n_spi = 0,
    n_io_i3c = 0,
    n_io_od = 0,
    n_warmboot = 0;
  for (Instance *inst : top->instances())
    {
      if (models.is_lc(inst))
        {
          ++n_lc;
          if (inst->get_param("DFF_ENABLE").get_bit(0))
            {
              if (inst->get_param("CARRY_ENABLE").get_bit(0))
                ++n_lc_carry_dff;
              else
                ++n_lc_dff;
            }
          else
            {
              if (inst->get_param("CARRY_ENABLE").get_bit(0))
                ++n_lc_carry;
            }
        }
      else if (models.is_io(inst))
        ++n_io;
      else if (models.is_gb(inst))
        ++n_gb;
      else if (models.is_warmboot(inst))
        ++n_warmboot;
      else if (models.is_gb_io(inst))
        {
          ++n_io;
          ++n_gb_io;
        }
      else if (models.is_pllX(inst))
        ++n_pll;
      else if (models.is_mac16(inst))
        ++n_mac16;
      else if (models.is_spram(inst))
        ++n_spram;
      else if (models.is_hfosc(inst))
        ++n_hfosc;
      else if (models.is_lfosc(inst))
        ++n_lfosc;        
      else if (models.is_rgba_drv(inst))
        ++n_rgba_drv;
      else if (models.is_ledda_ip(inst))
        ++n_ledda_ip;
      else if (models.is_spi(inst))
        ++n_spi;
      else if (models.is_i2c(inst))
        ++n_i2c;
      else if (models.is_io_i3c(inst))
        ++n_io_i3c;
      else if (models.is_io_od(inst))
        ++n_io_od;
      else
        { 
          assert(models.is_ramX(inst));
          ++n_bram;
        }
    }
  
  int n_logic_tiles = 0;
  for (int i = 0; i < chipdb->n_tiles; ++i)
    {
      if (chipdb->tile_type[i] == TileType::LOGIC)
        ++n_logic_tiles;
    }
  
  int n_warmboot_cells = 0;
  for (int i = 0; i < chipdb->n_cells; ++i)
    {
      if (chipdb->cell_type[i+1] == CellType::WARMBOOT)
        ++n_warmboot_cells;
    }
  if(chipdb->device == "5k") {
    *logs << "\nAfter packing:\n"
          << "IOs          " << n_io << " / " << package.pin_loc.size() << "\n"
          << "  IO_I3Cs    " << n_io_i3c << " / " << count_extra_cells(CellType::IO_I3C) << "\n"
          << "  IO_ODs     " << n_io_od << " / " << (3 * count_extra_cells(CellType::RGBA_DRV)) << "\n"
          << "GBs          " << n_gb << " / " << chipdb->n_global_nets << "\n"
          << "  GB_IOs     " << n_gb_io << " / " << chipdb->n_global_nets << "\n"
          << "LCs          " << n_lc << " / " << n_logic_tiles*8 << "\n"
          << "  DFF        " << n_lc_dff << "\n"
          << "  CARRY      " << n_lc_carry << "\n"
          << "  CARRY, DFF " << n_lc_carry_dff << "\n"
          << "  DFF PASS   " << n_dff_pass_through << "\n"
          << "  CARRY PASS " << n_carry_pass_through << "\n"
          << "BRAMs        " << n_bram << " / " << n_ramt_tiles << "\n"
          << "WARMBOOTs    " << n_warmboot << " / " << n_warmboot_cells << "\n"
          << "PLLs         " << n_pll << " / " 
          << count_extra_cells(CellType::PLL) << "\n"
          << "MAC16s       " << n_mac16 << " / " 
          << count_extra_cells(CellType::MAC16) << "\n"
          << "SPRAM256KAs  " << n_spram << " / " 
          << count_extra_cells(CellType::SPRAM) << "\n"
          << "HFOSCs       " << n_hfosc << " / " 
          << count_extra_cells(CellType::HFOSC) << "\n"
          << "LFOSCs       " << n_lfosc << " / " 
          << count_extra_cells(CellType::LFOSC) << "\n"
          << "RGBA_DRVs    " << n_rgba_drv << " / " 
          << count_extra_cells(CellType::RGBA_DRV) << "\n"
          << "LEDDA_IPs    " << n_ledda_ip << " / " 
          << count_extra_cells(CellType::LEDDA_IP) << "\n"
          << "I2Cs         " << n_i2c << " / " 
          << count_extra_cells(CellType::I2C_IP) << "\n"
          << "SPIs         " << n_spi << " / " 
          << count_extra_cells(CellType::SPI_IP) << "\n\n";
  } else {
    *logs << "\nAfter packing:\n"
          << "IOs          " << n_io << " / " << package.pin_loc.size() << "\n"
          << "GBs          " << n_gb << " / " << chipdb->n_global_nets << "\n"
          << "  GB_IOs     " << n_gb_io << " / " << chipdb->n_global_nets << "\n"
          << "LCs          " << n_lc << " / " << n_logic_tiles*8 << "\n"
          << "  DFF        " << n_lc_dff << "\n"
          << "  CARRY      " << n_lc_carry << "\n"
          << "  CARRY, DFF " << n_lc_carry_dff << "\n"
          << "  DFF PASS   " << n_dff_pass_through << "\n"
          << "  CARRY PASS " << n_carry_pass_through << "\n"
          << "BRAMs        " << n_bram << " / " << n_ramt_tiles << "\n"
          << "WARMBOOTs    " << n_warmboot << " / " << n_warmboot_cells << "\n"
          << "PLLs         " << n_pll << " / " 
          << count_extra_cells(CellType::PLL) << "\n\n";
  }
  
}

void
pack(DesignState &ds)
{
  Packer packer(ds);
  packer.pack();
}
