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

#ifndef PNR_LOCATION_HH
#define PNR_LOCATION_HH

#include "bstream.hh"

#include <ostream>

class Location
{
  friend std::ostream &operator<<(std::ostream &s, const Location &loc);
  friend obstream &operator<<(obstream &obs, const Location &loc);
  friend ibstream &operator>>(ibstream &ibs, Location &loc);
  template<typename T> friend struct std::hash;
  
  // pos is 0-7 for logic tiles, 0-4 (io_1, io_2, gb, pll) for io tiles
  int m_tile, m_pos;
  
public:
  int tile() const { return m_tile; }
  int pos() const { return m_pos; }
  
  Location()
    : m_tile(0), m_pos(0)
  {}
  Location(int tile_, int pos_)
    : m_tile(tile_), m_pos(pos_)
  {}
  
  bool operator==(const Location &loc2) const
  {
    return (m_tile == loc2.m_tile
            && m_pos == loc2.m_pos);
  }
  bool operator!=(const Location &loc2) const { return !operator==(loc2); }
  
  bool operator<(const Location &loc2) const
  {
    if (m_tile < loc2.m_tile)
      return true;
    if (m_tile > loc2.m_tile)
      return false;
    
    return m_pos < loc2.m_pos;
  }
};

inline obstream &operator<<(obstream &obs, const Location &loc)
{
  return obs << loc.m_tile << loc.m_pos;
}

inline ibstream &operator>>(ibstream &ibs, Location &loc)
{
  return ibs >> loc.m_tile >> loc.m_pos;
}

namespace std {

template<>
struct hash<Location>
{
public:
  size_t operator()(const Location &loc) const
  {
    std::hash<int> hasher;
    size_t h = hasher(loc.m_tile);
    return hash_combine(h, hasher(loc.m_pos));
  }
};

}

#endif
