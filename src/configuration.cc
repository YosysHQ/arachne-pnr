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

#include "configuration.hh"
#include "chipdb.hh"
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
