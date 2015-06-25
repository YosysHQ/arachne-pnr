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
  cbits[value_cbit] = value;
}

void
Configuration::set_cbits(const std::vector<CBit> &value_cbits,
			 const std::vector<bool> &value)
{
  assert(value_cbits.size() == value.size());
  for (unsigned i = 0; i < value_cbits.size(); i ++)
    set_cbit(value_cbits[i], value[i]);
}

void
Configuration::write_txt(std::ostream &s,
			 const ChipDB *chipdb,
			 Design *d,
			 const hashmap<Instance *, Location> &placement,
			 const std::vector<Net *> &cnet_net)
{
  s << ".device " << chipdb->device << "\n";
  for (int x = 0; x < chipdb->width; x ++)
    for (int y = 0; y < chipdb->height; y ++)
      {
	int t = chipdb->tile(x, y);
	TileType ty = chipdb->tile_type[t];
	
	if (ty == TileType::NO_TILE)
	  continue;
	
	s << "." << tile_type_name(ty) << " " << x << " " << y << "\n";
	
	int bw, bh;
	std::tie(bw, bh) = chipdb->tile_cbits_block_size.at(ty);
	
	for (int r = 0; r < bh; r ++)
	  {
	    for (int c = 0; c < bw; c ++)
	      {
		auto i = cbits.find(CBit(x, y, r, c));
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
  
  Models models(d);
  for (const auto &p : placement)
    {
      if (models.is_ramX(p.first))
	{
	  assert(chipdb->tile_type[chipdb->tile(p.second.x(), p.second.y())]
		 == TileType::RAMT_TILE);
	  
	  s << ".ram_data " << p.second.x() << " " << (p.second.y()-1) << "\n";
	  for (int i = 0; i < 16; ++i)
	    {
	      BitVector init_i = p.first->get_param(fmt("INIT_" << hexdigit(i, 'A'))).as_bits();
	      init_i.resize(256);
	      for (int j = 63; j >= 0; --j)
		{
		  int x = (((int)init_i[j*4 + 3] << 3)
			   | ((int)init_i[j*4 + 2] << 2)
			   | ((int)init_i[j*4 + 1] << 1)
			   | ((int)init_i[j*4 + 0]));
		  s << hexdigit(x);
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
