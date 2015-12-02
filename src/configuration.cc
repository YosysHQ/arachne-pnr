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

#include "chipdb.hh"
#include "configuration.hh"
#include "util.hh"
#include "netlist.hh"
#include "designstate.hh"

#include <cassert>
#include <iostream>

Configuration::Configuration()
{
}

void
Configuration::set_cbit(const CBit &value_cbit, bool value)
{
  assert(!contains_key(cbits, value_cbit)
         || cbits.at(value_cbit) == value);
  // *logs << value_cbit << " = " << value << "\n";
  cbits[value_cbit] = value;
}

void
Configuration::set_cbits(const std::vector<CBit> &value_cbits,
                         unsigned value)
{
  for (unsigned i = 0; i < value_cbits.size(); ++i)
    set_cbit(value_cbits[i], (bool)(value & (1 << i)));
}

void
Configuration::set_extra_cbit(const std::tuple<int, int, int> &t)
{
  extend(extra_cbits, t);
}

void
Configuration::write_txt(std::ostream &s,
                         const ChipDB *chipdb,
                         Design *d,
                         const std::map<Instance *, int, IdLess> &placement,
                         const std::vector<Net *> &cnet_net)
{
  s << ".comment " << version_str << "\n";
  
  s << ".device " << chipdb->device << "\n";
  for (int t = 0; t < chipdb->n_tiles; ++t)
    {
      TileType ty = chipdb->tile_type[t];
      if (ty == TileType::EMPTY)
        continue;

      int  x = chipdb->tile_x(t),
        y = chipdb->tile_y(t);
      s << "." << tile_type_name(ty) << " " << x << " " << y << "\n";
      
      int bw, bh;
      std::tie(bw, bh) = chipdb->tile_cbits_block_size.at(ty);
      
      for (int r = 0; r < bh; r ++)
        {
          for (int c = 0; c < bw; c ++)
            {
              auto i = cbits.find(CBit(t, r, c));
              if (i != cbits.end())
                {
                  if (i->second)
                    s << "1";
                  else
                    s << "0";
                }
              else
                s << "0";
            }
          s << "\n";
        }
    }
  
  for (const auto &t : extra_cbits)
    {
      s << ".extra_bit " << std::get<0>(t)
        << " " << std::get<1>(t)
        << " " << std::get<2>(t) << "\n";
    }
  
  Models models(d);
  for (const auto &p : placement)
    {
      if (models.is_ramX(p.first))
        {
          int cell = p.second;
          const Location &loc = chipdb->cell_location[cell];
          
          int t = loc.tile();
          assert(chipdb->tile_type[t] == TileType::RAMT);
          
          int x = chipdb->tile_x(t),
            y = chipdb->tile_y(t);
          
          s << ".ram_data " << x << " " << (y-1) << "\n";
          for (int i = 0; i < 16; ++i)
            {
              BitVector init_i = p.first->get_param(fmt("INIT_" << hexdigit(i, 'A'))).as_bits();
              init_i.resize(256);
              for (int j = 63; j >= 0; --j)
                {
                  int v = (((int)init_i[j*4 + 3] << 3)
                           | ((int)init_i[j*4 + 2] << 2)
                           | ((int)init_i[j*4 + 1] << 1)
                           | ((int)init_i[j*4 + 0]));
                  s << hexdigit(v);
                }
              s << "\n";
            }
        }
    }
  
  for (int i = 0; i < chipdb->n_nets; ++i)
    {
      Net *n = cnet_net[i];
      if (n)
        s << ".sym " << i << " " << n->name() << "\n";
    }
}

class Configurator
{
  const ChipDB *chipdb;
  const Package &package;
  Design *d;
  const Models &models;
  const std::map<Instance *, int, IdLess> &placement;
  Configuration &conf;

  void configure_io(const Location &loc,
		    bool enable_input,
		    bool pullup);
  
public:
  Configurator(DesignState &ds)
    : chipdb(ds.chipdb), 
      package(ds.package),
      d(ds.d), 
      models(ds.models), 
      placement(ds.placement), 
      conf(ds.conf)
  {}
  
  void configure_placement();
};

void
Configurator::configure_io(const Location &loc,
			   bool enable_input,
			   bool pullup)
{
  const auto &func_cbits = chipdb->tile_nonrouting_cbits.at(TileType::IO);
  const CBit &ie_0 = func_cbits.at("IoCtrl.IE_0")[0],
    &ie_1 = func_cbits.at("IoCtrl.IE_1")[0],
    &ren_0 = func_cbits.at("IoCtrl.REN_0")[0],
    &ren_1 = func_cbits.at("IoCtrl.REN_1")[0];
  
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

void 
Configurator::configure_placement()
{
  BasedVector<Instance *, 1> cell_gate(chipdb->n_cells, 0);
  for (const auto &p : placement)
    cell_gate[p.second] = p.first;
  
  std::vector<int> ramt_tiles;
  for (int i = 0; i < chipdb->n_tiles; ++i)
    {
      switch(chipdb->tile_type[i])
        {
        case TileType::RAMT:
          ramt_tiles.push_back(i);
          break;
        default:
          break;
        }
    }
  
  for (const auto &p : placement)
    {
      Instance *inst = p.first;
      int cell = p.second;
      
      const Location &loc = chipdb->cell_location[cell];
      
      if (models.is_warmboot(inst))
        continue;
      
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
      else if (models.is_ioX(inst))
        {
          assert(contains(package.loc_pin, loc));
          
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
          
          if (models.is_gb_io(inst)
              && inst->find_port("GLOBAL_BUFFER_OUTPUT")->connected())
            {
              int glb = chipdb->loc_pin_glb_num.at(loc);
              
              const auto &ecb = chipdb->extra_bits.at(fmt("padin_glb_netwk." << glb));
              conf.set_extra_cbit(ecb);
            }
        }
      else if (models.is_gb(inst))
        ;
      else if (models.is_ramX(inst))
        {
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
      else
        {
          assert(models.is_pllX(inst));
          
          bool found = false;
          Location io_loc;
          for (const auto &p2 : chipdb->loc_pin_glb_num)
            {
              if (p2.first.tile() == t)
                {
                  assert(!found);
                  io_loc = p2.first;
                  found = true;
                }
            }
          assert(found);
          
          const CBit &cbit_pt0 = func_cbits.at(fmt("IOB_" << io_loc.pos() << ".PINTYPE_0"))[0],
            &cbit_pt1 = func_cbits.at(fmt("IOB_" << io_loc.pos() << ".PINTYPE_1"))[0];
          
          conf.set_cbit(CBit(t, 
                             cbit_pt0.row, 
                             cbit_pt0.col),
                        true);
          conf.set_cbit(CBit(t, 
                             cbit_pt1.row, 
                             cbit_pt1.col),
                        false);
          
          CBit delay_adjmode_fb_cb = chipdb->extra_cell_cbit(cell, "DELAY_ADJMODE_FB");
          
          std::string delay_adjmode_fb = inst->get_param("DELAY_ADJUSTMENT_MODE_FEEDBACK").as_string();
          if (delay_adjmode_fb == "FIXED")
            conf.set_cbit(delay_adjmode_fb_cb, false);
          else if (delay_adjmode_fb == "DYNAMIC")
            conf.set_cbit(delay_adjmode_fb_cb, true);
          else
            fatal(fmt("unknown DELAY_ADJUSTMENT_MODE_FEEDBACK value: " << delay_adjmode_fb));
          
          CBit delay_adjmode_rel_cb = chipdb->extra_cell_cbit(cell, "DELAY_ADJMODE_REL");
          
          std::string delay_adjmode_rel = inst->get_param("DELAY_ADJUSTMENT_MODE_RELATIVE").as_string();
          if (delay_adjmode_rel == "FIXED")
            conf.set_cbit(delay_adjmode_rel_cb, false);
          else if (delay_adjmode_rel == "DYNAMIC")
            conf.set_cbit(delay_adjmode_rel_cb, true);
          else
            fatal(fmt("unknown DELAY_ADJUSTMENT_MODE_RELATIVE value: " << delay_adjmode_rel));
          
          BitVector divf = inst->get_param("DIVF").as_bits();
          divf.resize(7);
          for (int i = 0; i < (int)divf.size(); ++i)
            {
              CBit divf_i_cb = chipdb->extra_cell_cbit(cell, fmt("DIVF_" << i));
              conf.set_cbit(divf_i_cb, divf[i]);
            }
          
          BitVector divq = inst->get_param("DIVQ").as_bits();
          divq.resize(3);
          for (int i = 0; i < (int)divq.size(); ++i)
            {
              CBit divq_i_cb = chipdb->extra_cell_cbit(cell, fmt("DIVQ_" << i));
              conf.set_cbit(divq_i_cb, divq[i]);
            }
          
          BitVector divr = inst->get_param("DIVR").as_bits();
          divr.resize(4);
          for (int i = 0; i < (int)divr.size(); ++i)
            {
              CBit divr_i_cb = chipdb->extra_cell_cbit(cell, fmt("DIVR_" << i));
              conf.set_cbit(divr_i_cb, divr[i]);
            }
          
          BitVector fda_feedback = inst->get_param("FDA_FEEDBACK").as_bits();
          fda_feedback.resize(4);
          for (int i = 0; i < (int)fda_feedback.size(); ++i)
            {
              CBit fda_feedback_i_cb = chipdb->extra_cell_cbit(cell, fmt("FDA_FEEDBACK_" << i));
              conf.set_cbit(fda_feedback_i_cb, fda_feedback[i]);
            }
          
          BitVector fda_relative = inst->get_param("FDA_RELATIVE").as_bits();
          fda_relative.resize(4);
          for (int i = 0; i < (int)fda_relative.size(); ++i)
            {
              CBit fda_relative_i_cb = chipdb->extra_cell_cbit(cell, fmt("FDA_RELATIVE_" << i));
              conf.set_cbit(fda_relative_i_cb, fda_relative[i]);
            }
          
          std::string feedback_path_str = inst->get_param("FEEDBACK_PATH").as_string();
          
          int feedback_path_value = 0;
          if (feedback_path_str == "DELAY")
            feedback_path_value = 0;
          else if (feedback_path_str == "SIMPLE")
            feedback_path_value = 1;
          else if (feedback_path_str == "PHASE_AND_DELAY")
            feedback_path_value = 2;
          else
            {
              assert(feedback_path_str == "EXTERNAL");
              feedback_path_value = 6;
            }
          
          BitVector feedback_path(3, feedback_path_value);
          for (int i = 0; i < (int)feedback_path.size(); ++i)
            {
              CBit feedback_path_i_cb = chipdb->extra_cell_cbit(cell, fmt("FEEDBACK_PATH_" << i));
              conf.set_cbit(feedback_path_i_cb, feedback_path[i]);
            }
          
          BitVector filter_range = inst->get_param("FILTER_RANGE").as_bits();
          filter_range.resize(3);
          for (int i = 0; i < (int)filter_range.size(); ++i)
            {
              CBit filter_range_i_cb = chipdb->extra_cell_cbit(cell, fmt("FILTER_RANGE_" << i));
              conf.set_cbit(filter_range_i_cb, filter_range[i]);
            }
          
          std::string pllout_select_porta_str;
          if (inst->instance_of()->name() == "SB_PLL40_PAD"
              || inst->instance_of()->name() == "SB_PLL40_CORE")
            pllout_select_porta_str = inst->get_param("PLLOUT_SELECT").as_string();
          else
            pllout_select_porta_str = inst->get_param("PLLOUT_SELECT_PORTA").as_string();
          
          int pllout_select_porta_value = 0;
          if (pllout_select_porta_str == "GENCLK")
            pllout_select_porta_value = 0;
          else if (pllout_select_porta_str == "GENCLK_HALF")
            pllout_select_porta_value = 1;
          else if (pllout_select_porta_str == "SHIFTREG_90deg")
            pllout_select_porta_value = 2;
          else
            {
              assert(pllout_select_porta_str == "SHIFTREG_0deg");
              pllout_select_porta_value = 3;
            }
          
          BitVector pllout_select_porta(2, pllout_select_porta_value);
          for (int i = 0; i < (int)pllout_select_porta.size(); ++i)
            {
              CBit pllout_select_porta_i_cb = chipdb->extra_cell_cbit(cell, fmt("PLLOUT_SELECT_A_" << i));
              conf.set_cbit(pllout_select_porta_i_cb, pllout_select_porta[i]);
            }
          
          int pllout_select_portb_value = 0;
          if (inst->instance_of()->name() == "SB_PLL40_2_PAD"
              || inst->instance_of()->name() == "SB_PLL40_2F_PAD"
              || inst->instance_of()->name() == "SB_PLL40_2F_CORE")
            {
              std::string pllout_select_portb_str = inst->get_param("PLLOUT_SELECT_PORTB").as_string();
              
              if (pllout_select_portb_str == "GENCLK")
                pllout_select_portb_value = 0;
              else if (pllout_select_portb_str == "GENCLK_HALF")
                pllout_select_portb_value = 1;
              else if (pllout_select_portb_str == "SHIFTREG_90deg")
                pllout_select_portb_value = 2;
              else
                {
                  assert(pllout_select_portb_str == "SHIFTREG_0deg");
                  pllout_select_portb_value = 3;
                }
            }
          
          BitVector pllout_select_portb(2, pllout_select_portb_value);
          for (int i = 0; i < (int)pllout_select_portb.size(); ++i)
            {
              CBit pllout_select_portb_i_cb = chipdb->extra_cell_cbit(cell, fmt("PLLOUT_SELECT_B_" << i));
              conf.set_cbit(pllout_select_portb_i_cb, pllout_select_portb[i]);
            }
          
          int pll_type_value = 0;
          if (inst->instance_of()->name() == "SB_PLL40_PAD")
            pll_type_value = 2;
          else if (inst->instance_of()->name() == "SB_PLL40_2_PAD")
            pll_type_value = 4;
          else if (inst->instance_of()->name() == "SB_PLL40_2F_PAD")
            pll_type_value = 6;
          else if (inst->instance_of()->name() == "SB_PLL40_CORE")
            pll_type_value = 3;
          else 
            {
              assert(inst->instance_of()->name() == "SB_PLL40_2F_CORE");
              pll_type_value = 7;
            }
          BitVector pll_type(3, pll_type_value);
          for (int i = 0; i < (int)pll_type.size(); ++i)
            {
              CBit pll_type_i_cb = chipdb->extra_cell_cbit(cell, fmt("PLLTYPE_" << i));
              conf.set_cbit(pll_type_i_cb, pll_type[i]);
            }
          
          BitVector shiftreg_div_mode = inst->get_param("SHIFTREG_DIV_MODE").as_bits();
          CBit shiftreg_div_mode_cb = chipdb->extra_cell_cbit(cell, "SHIFTREG_DIV_MODE");
          conf.set_cbit(shiftreg_div_mode_cb, shiftreg_div_mode[0]);
          
          Port *a = inst->find_port("PLLOUTGLOBAL");
          if (!a)
            a = inst->find_port("PLLOUTGLOBALA");
          assert(a);
          if (a->connected())
            {
              const auto &p2 = chipdb->cell_mfvs.at(cell).at("PLLOUT_A");
              Location glb_loc(p2.first, std::stoi(p2.second));
              int glb = chipdb->loc_pin_glb_num.at(glb_loc);
              
              const auto &ecb = chipdb->extra_bits.at(fmt("padin_glb_netwk." << glb));
              conf.set_extra_cbit(ecb);
            }
          
          Port *b = inst->find_port("PLLOUTGLOBALB");
          if (b && b->connected())
            {
              const auto &p2 = chipdb->cell_mfvs.at(cell).at("PLLOUT_B");
              Location glb_loc(p2.first, std::stoi(p2.second));
              int glb = chipdb->loc_pin_glb_num.at(glb_loc);
              
              const auto &ecb = chipdb->extra_bits.at(fmt("padin_glb_netwk." << glb));
              conf.set_extra_cbit(ecb);
            }
        }
    }
  
  // set IoCtrl configuration bits
  {
    const auto &func_cbits = chipdb->tile_nonrouting_cbits.at(TileType::IO);
    const CBit &lvds = func_cbits.at("IoCtrl.LVDS")[0];
    
    std::map<Location, int> loc_pll;
    int pll_idx = cell_type_idx(CellType::PLL);
    for (int cell : chipdb->cell_type_cells[pll_idx])
      {
        const auto &p2a = chipdb->cell_mfvs.at(cell).at("PLLOUT_A");
        Location a_loc(p2a.first, std::stoi(p2a.second));
        extend(loc_pll, a_loc, cell);
        
        const auto &p2b = chipdb->cell_mfvs.at(cell).at("PLLOUT_B");
        Location b_loc(p2b.first, std::stoi(p2b.second));
        extend(loc_pll, b_loc, cell);
      }
    
    for (const auto &p : package.pin_loc)
      {
        // unused io
        bool enable_input = false;
        bool pullup = true;  // default pullup
        
        const Location &loc = p.second;
        int pll_cell = lookup_or_default(loc_pll, loc, 0);
        if (pll_cell)
          {
            // FIXME only enable if inputs present
            enable_input = true;
            pullup = false;
          }
        else
          {
            int cell = chipdb->loc_cell(loc);
            Instance *inst2 = cell_gate[cell];
            if (inst2)
              {
                if (inst2->find_port("D_IN_0")->connected()
                    || inst2->find_port("D_IN_1")->connected()
                    || (models.is_gb_io(inst2)
                        && inst2->find_port("GLOBAL_BUFFER_OUTPUT")->connected()))
                  enable_input = true;
                pullup = inst2->get_param("PULLUP").get_bit(0);
                conf.set_cbit(CBit(loc.tile(),
                                   lvds.row,
                                   lvds.col),
                              inst2->get_param("IO_STANDARD").as_string() == "SB_LVDS_INPUT");
              }
          }
        
        const Location &ieren_loc = chipdb->ieren.at(loc);
        configure_io(ieren_loc, enable_input, pullup);
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
            
            configure_io(loc, enable_input, pullup);
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
        Instance *inst2 = cell_gate[cell];
        assert(!inst2 || models.is_ramX(inst2));
        conf.set_cbit(CBit(chipdb->ramt_ramb_tile(loc.tile()), // PowerUp on ramb tile
                           powerup.row,
                           powerup.col),
                      // active low
                      (chipdb->device == "1k"
                       ? !inst2
                       : (bool)inst2));
      }
  }
}

void
configure_placement(DesignState &ds)
{
  Configurator configurator(ds);
  configurator.configure_placement();
}
