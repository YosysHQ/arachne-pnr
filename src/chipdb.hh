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

#ifndef PNR_CHIPDB_HH
#define PNR_CHIPDB_HH

#include "location.hh"
#include "util.hh"

#include <ostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cassert>

class CBit
{
public:
  friend std::ostream &operator<<(std::ostream &s, const CBit &cbit);
  template<typename T> friend struct std::hash;
  
  int x;  // tile
  int y;
  int row;
  int col;
  
public:
  CBit()
    : x(0), y(0), row(0), col(0)
  {}
  CBit(int x_, int y_, int r, int c)
    : x(x_), y(y_), row(r), col(c)
  {}
  
  bool operator==(const CBit &rhs) const;
  bool operator!=(const CBit &rhs) const { return !operator==(rhs); }
  
  bool operator<(const CBit &rhs) const;
};

namespace std {

template<>
struct hash<CBit>
{
public:
  size_t operator() (const CBit &cbit) const
  {
    hash<int> hasher;
    size_t h = hasher(cbit.x);
    h = hash_combine(h, hasher(cbit.y));
    h = hash_combine(h, hasher(cbit.row));
    return hash_combine(h, hasher(cbit.col));
  }
};

}

class CBitVal
{
public:
  friend std::ostream &operator<<(std::ostream &s, const CBitVal &cbits);
  
  std::unordered_map<CBit, bool> cbit_val;
  
public:
  CBitVal() {}
  CBitVal(const std::unordered_map<CBit, bool> &cbv)
    : cbit_val(cbv)
  {}
  
  std::unordered_set<CBit> cbits() const;
};

class Switch
{
public:
  bool bidir; // routing
  int tile;
  int out;
  std::unordered_map<int, std::vector<bool>> in_val;
  std::vector<CBit> cbits;
  
public:
  Switch() {}
  Switch(bool bi,
	 int t,
	 int o,
	 const std::unordered_map<int, std::vector<bool>> &iv,
	 const std::vector<CBit> &cb)
    : bidir(bi),
      tile(t),
      out(o),
      in_val(iv),
      cbits(cb)
  {}
};

enum class TileType {
  NO_TILE, IO_TILE, LOGIC_TILE, RAMB_TILE, RAMT_TILE,
};

namespace std {

template<>
struct hash<TileType>
{
public:
  size_t operator() (TileType x) const
  {
    using underlying_t = typename underlying_type<TileType>::type;
    hash<underlying_t> hasher;
    return hasher(static_cast<underlying_t>(x));
  }
};

}

extern std::string tile_type_name(TileType t);

class Package
{
public:
  std::string name;
  
  std::unordered_map<std::string, Location> pin_loc;
  std::unordered_map<Location, std::string> loc_pin;
};

class ChipDB
{
public:
  std::string device;
  
  int width;
  int height;
  
  int n_tiles;
  int n_nets;
  int n_global_nets;
  
  std::unordered_map<std::string, Package> packages;
  
  std::unordered_map<Location, int> loc_pin_glb_num;
  
  std::vector<std::vector<int>> bank_tiles;
  
  std::vector<int> iolatch;  // tiles
  std::unordered_map<Location, Location> ieren;
  std::unordered_map<std::string, std::tuple<int, int, int>> extra_bits;
  
  std::unordered_map<std::pair<int, int>, int> gbufin;
  
  std::unordered_map<int, int> tile_colbuf_tile;
  
  std::vector<TileType> tile_type;
  std::vector<std::pair<int, std::string>> net_tile_name;
  std::vector<std::unordered_map<std::string, int>> tile_nets;
  std::unordered_map<TileType,
		     std::unordered_map<std::string, std::vector<CBit>>> tile_nonrouting_cbits;
  
  std::vector<int> extra_cell_tile;
  std::vector<std::string> extra_cell_name;
  std::vector<std::map<std::string, std::pair<int, std::string>>>
    extra_cell_mfvs;
  
  // buffers and routing
  std::vector<Switch> switches;
  
  std::vector<std::set<int>> out_switches;
  std::vector<std::set<int>> in_switches;
  
  std::unordered_map<TileType, std::pair<int, int>> tile_cbits_block_size;
  
  bool is_global_net(int i) const { return i < n_global_nets; }
  int find_switch(int in, int out) const;
  
  int tile(int x, int y) const
  {
    assert(x >= 0 && x < width);
    assert(y >= 0 && y < height);
  
    return x + width*y;
  }
  
  int tile_x(int t) const
  {
    assert(t >= 0 && t <= n_tiles);
    return t % width;
  }

  int tile_y(int t) const
  {
    assert(t >= 0 && t <= n_tiles);
    return t / width;
  }
  
  int tile_bank(int t) const;
  
  void set_device(const std::string &d, int w, int h, int n_nets_);
  
public:
  ChipDB();
  
  void dump(std::ostream &s) const;
};

extern ChipDB *read_chipdb(const std::string &filename);

#endif
