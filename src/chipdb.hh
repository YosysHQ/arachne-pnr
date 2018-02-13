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
#include "hashmap.hh"
#include "bstream.hh"
#include "vector.hh"

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
  friend obstream &operator<<(obstream &obs, const CBit &cbit);
  friend ibstream &operator>>(ibstream &ibs, CBit &cbit);
  template<typename T> friend struct std::hash;
  
  int tile;
  int row;
  int col;
  
public:
  CBit()
    : tile(0), row(0), col(0)
  {}
  CBit(int tile_, int r, int c)
    : tile(tile_), row(r), col(c)
  {}
  
  CBit with_tile(int new_t) const { return CBit(new_t, row, col); }
  
  bool operator==(const CBit &rhs) const;
  bool operator!=(const CBit &rhs) const { return !operator==(rhs); }
  
  bool operator<(const CBit &rhs) const;
};

inline obstream &operator<<(obstream &obs, const CBit &cbit)
{
  return obs << cbit.tile << cbit.row << cbit.col;
}

inline ibstream &operator>>(ibstream &ibs, CBit &cbit)
{
  return ibs >> cbit.tile >> cbit.row >> cbit.col;
}

namespace std {

template<>
struct hash<CBit>
{
public:
  size_t operator() (const CBit &cbit) const
  {
    std::hash<int> hasher;
    size_t h = hasher(cbit.tile);
    h = hash_combine(h, hasher(cbit.row));
    return hash_combine(h, hasher(cbit.col));
  }
};

}

class CBitVal
{
public:
  friend std::ostream &operator<<(std::ostream &s, const CBitVal &cbits);
  
  std::map<CBit, bool> cbit_val;
  
public:
  CBitVal() {}
  CBitVal(const std::map<CBit, bool> &cbv)
    : cbit_val(cbv)
  {}
  
  std::set<CBit> cbits() const;
};

class Switch
{
public:
  friend obstream &operator<<(obstream &obs, const Switch &sw);
  friend ibstream &operator>>(ibstream &ibs, Switch &sw);
  
  bool bidir; // routing
  int tile;
  int out;
  std::map<int, unsigned> in_val;
  std::vector<CBit> cbits;
  
public:
  Switch() {}
  Switch(bool bi,
         int t,
         int o,
         const std::map<int, unsigned> &iv,
         const std::vector<CBit> &cb)
    : bidir(bi),
      tile(t),
      out(o),
      in_val(iv),
      cbits(cb)
  {}
};

obstream &operator<<(obstream &obs, const Switch &sw);
ibstream &operator>>(ibstream &ibs, Switch &sw);

enum class TileType : int {
  EMPTY, IO, LOGIC, RAMB, RAMT, DSP0, DSP1, DSP2, DSP3, IPCON
};

enum class CellType : int {
  LOGIC, IO, GB, RAM, WARMBOOT, PLL, MAC16, SPRAM, LFOSC, HFOSC, RGBA_DRV, LEDDA_IP, I2C_IP, SPI_IP, IO_I3C
};

std::string cell_type_name(CellType ct);

inline obstream &operator<<(obstream &obs, TileType t)
{
  return obs << static_cast<int>(t);
}

inline ibstream &operator>>(ibstream &ibs, TileType &t)
{
  int x;
  ibs >> x;
  t = static_cast<TileType>(x);
  return ibs;
}

constexpr int cell_type_idx(CellType type)
{
  return static_cast<int>(type);
}

static const int n_cell_types = cell_type_idx(CellType::IO_I3C) + 1;

inline obstream &operator<<(obstream &obs, CellType t)
{
  return obs << static_cast<int>(t);
}

inline ibstream &operator>>(ibstream &ibs, CellType &t)
{
  int x;
  ibs >> x;
  t = static_cast<CellType>(x);
  return ibs;
}

namespace std {

template<>
struct hash<TileType>
{
public:
  size_t operator() (TileType x) const
  {
    std::hash<int> hasher;
    return hasher(static_cast<int>(x));
  }
};

}

std::string tile_type_name(TileType t);

class Package
{
  friend obstream &operator<<(obstream &obs, const Package &pkg);
  friend ibstream &operator>>(ibstream &ibs, Package &pkg);
  
public:
  std::string name;
  
  std::map<std::string, Location> pin_loc;
  std::map<Location, std::string> loc_pin;
};

inline obstream &operator<<(obstream &obs, const Package &pkg)
{
  return obs << pkg.name << pkg.pin_loc;
}

inline ibstream &operator>>(ibstream &ibs, Package &pkg)
{
  ibs >> pkg.name >> pkg.pin_loc;
  for (const auto &p : pkg.pin_loc)
    extend(pkg.loc_pin, p.second, p.first);
  return ibs;
}

class ChipDB
{
public:
  std::string device;
  
  int width;
  int height;
  
  int n_tiles;
  int n_nets;
  int n_global_nets;
  std::map<int, int> net_global;
  
  std::map<std::string, Package> packages;
  
  std::map<Location, int> loc_pin_glb_num;
  
  std::vector<int> iolatch;  // tiles
  std::map<Location, Location> ieren;
  std::map<std::string, std::tuple<int, int, int>> extra_bits;
  
  std::map<std::pair<int, int>, int> gbufin;
  
  std::map<int, int> tile_colbuf_tile;
  
  std::vector<TileType> tile_type;
  std::vector<std::pair<int, std::string>> net_tile_name;
  std::vector<std::map<std::string, int>> tile_nets;
  
  std::map<TileType,
          std::map<std::string, std::vector<CBit>>>
    tile_nonrouting_cbits;
  
  CBit extra_cell_cbit(int ec, const std::string &name, bool is_ip = false) const;
  std::string extra_cell_netname(int ec, const std::string &name) const;
  int get_oscillator_glb(int cell, const std::string &net) const;
  
  int n_cells;
  BasedVector<CellType, 1> cell_type;
  BasedVector<Location, 1> cell_location;
  std::map<int, std::map<std::string, std::pair<int, std::string>>>
    cell_mfvs;
  std::map<int, std::set<std::string>> cell_locked_pkgs;
  
  std::vector<std::vector<int>> tile_pos_cell;
  int loc_cell(const Location &loc) const 
  {
    return tile_pos_cell[loc.tile()][loc.pos()];
  }
  
  std::vector<std::vector<int>> cell_type_cells;
  
  std::vector<std::vector<int>> bank_cells;
  
  // buffers and routing
  std::vector<Switch> switches;
  
  std::vector<std::set<int>> out_switches;
  std::vector<std::set<int>> in_switches;
  
  std::map<TileType, std::pair<int, int>> tile_cbits_block_size;
  
  int add_cell(CellType type, const Location &loc);
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
  int ramt_ramb_tile(int ramt_t) const
  {
    assert(tile_type[ramt_t] == TileType::RAMT);
    
    int ramb_t = ramt_t - width;
    assert(ramb_t == tile(tile_x(ramt_t),
                          tile_y(ramt_t)-1));
    assert(tile_type[ramb_t] == TileType::RAMB);
    return ramb_t;
  }
  
  void set_device(const std::string &d, int w, int h, int n_nets_);
  void finalize();
  
public:
  ChipDB();
  
  void dump(std::ostream &s) const;
  void bwrite(obstream &obs) const;
  void bread(ibstream &ibs);
};

ChipDB *read_chipdb(const std::string &filename);

#endif
