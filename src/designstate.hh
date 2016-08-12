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

#ifndef PNR_DESIGNSTATE_HH
#define PNR_DESIGNSTATE_HH

#include "netlist.hh"
#include "chipdb.hh"
#include "pcf.hh"
#include "carry.hh"
#include "configuration.hh"

class DesignState
{
public:
  const ChipDB *chipdb;
  const Package &package;
  Design *d;
  Models models;
  Model *top;
  Constraints constraints;
  CarryChains chains;
  std::set<Instance *, IdLess> locked;
  std::map<Instance *, int, IdLess> placement;
  std::map<Instance *, uint8_t, IdLess> gb_inst_gc;
  std::vector<Net *> cnet_net;
  Configuration conf;
  
public:
  DesignState(const ChipDB *chipdb_, const Package &package_, Design *d_);
  
  bool is_dual_pll(Instance *inst) const;
  std::vector<int> pll_out_io_cells(Instance *inst, int cell) const;
};

#endif
