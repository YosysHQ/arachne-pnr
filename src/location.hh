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

#include "util.hh"
#include <ostream>

class Location
{
  friend std::ostream &operator<<(std::ostream &s, const Location &loc);
  template<typename T> friend struct std::hash;
  
  // pos is 0-7 for logic tiles, 0-1 for io tiles
  int m_x, m_y, m_pos;
  
public:
  int x() const { return m_x; }
  int y() const { return m_y; }
  int pos() const { return m_pos; }
  
  Location()
    : m_x(0), m_y(0), m_pos(0)
  {}
  Location(int x_, int y_, int pos_)
    : m_x(x_), m_y(y_), m_pos(pos_)
  {}
  
  bool operator==(const Location &loc2) const
  {
    return (m_x == loc2.m_x
	    && m_y == loc2.m_y
	    && m_pos == loc2.m_pos);
  }
  bool operator!=(const Location &loc2) const { return !operator==(loc2); }
  
  bool operator<(const Location &loc2) const
  {
    if (m_x < loc2.m_x)
      return true;
    if (m_x > loc2.m_x)
      return false;
    
    if (m_y < loc2.m_y)
      return true;
    if (m_y > loc2.m_y)
      return false;
    
    return m_pos < loc2.m_pos;
  }
};

namespace std {

template<>
struct hash<Location>
{
public:
  size_t operator()(const Location &loc) const
  {
    hash<int> hasher;
    size_t h = hasher(loc.m_x);
    h = hash_combine(h, hasher(loc.m_y));
    return hash_combine(h, hasher(loc.m_pos));
  }
};

}

#endif
