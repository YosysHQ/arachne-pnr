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
#include "util.hh"
#include "line_parser.hh"

#include <cassert>
#include <cstring>
#include <iostream>
#include <fstream>

std::ostream &
operator<<(std::ostream &s, const CBit &cbit)
{
  return s << cbit.tile << " B" << cbit.row << "[" << cbit.col << "]";
}

bool
CBit::operator==(const CBit &rhs) const
{
  return tile == rhs.tile
    && row == rhs.row
    && col == rhs.col;
}

bool
CBit::operator<(const CBit &rhs) const
{
  if (tile < rhs.tile)
    return true;
  if (tile > rhs.tile)
    return false;

  if (row < rhs.row)
    return true;
  if (row > rhs.row)
    return false;
  
  return col < rhs.col;
}

std::ostream &
operator<<(std::ostream &s, const CBitVal &cv)
{
  for (const auto &p : cv.cbit_val)
    {
      if (p.second)
        s << "1";
      else
        s << "0";
    }
  for (const auto &p : cv.cbit_val)
    s << " " << p.first;
  return s;
}

std::set<CBit>
CBitVal::cbits() const
{
  return keys(cbit_val);
}

std::string
tile_type_name(TileType t)
{
  assert(t != TileType::EMPTY);
  switch(t)
    {
    case TileType::IO:
      return "io_tile";
    case TileType::LOGIC:
      return "logic_tile";
    case TileType::RAMB:
      return "ramb_tile";
    case TileType::RAMT:
      return "ramt_tile";
    case TileType::DSP0:
      return "dsp0_tile";
    case TileType::DSP1:
      return "dsp1_tile";
    case TileType::DSP2:
      return "dsp2_tile";
    case TileType::DSP3:
      return "dsp3_tile";
    case TileType::IPCON:
      return "ipcon_tile";
    case TileType::EMPTY:
      abort();
    }    
  return std::string();
}

obstream &operator<<(obstream &obs, const Switch &sw)
{
  obs << sw.bidir
      << sw.tile
      << sw.out
      << sw.cbits.size();
  for (const CBit &cbit : sw.cbits)
    {
      assert(cbit.tile == sw.tile);
      obs << cbit.row << cbit.col;
    }
  obs << sw.in_val;
  return obs;
}

ibstream &operator>>(ibstream &ibs, Switch &sw)
{
  size_t n_cbits;
  ibs >> sw.bidir
      >> sw.tile
      >> sw.out
      >> n_cbits;
  sw.cbits.resize(n_cbits);
  for (size_t i = 0; i < n_cbits; ++i)
    {
      int row, col;
      ibs >> row >> col;
      sw.cbits[i] = CBit(sw.tile, row, col);
    }
  ibs >> sw.in_val;
  return ibs;
}

ChipDB::ChipDB()
  : width(0), height(0), n_tiles(0), n_nets(0), n_global_nets(8),
    n_cells(0),
    cell_type_cells(n_cell_types),
    bank_cells(4)
{
}

int
ChipDB::add_cell(CellType type, const Location &loc)
{
  int cell = ++n_cells;
  cell_type.push_back(type);
  cell_location.push_back(loc);
  cell_type_cells[cell_type_idx(type)].push_back(cell);
  return cell;
}

int
ChipDB::tile_bank(int t) const
{
  assert(tile_type[t] == TileType::IO);
  int x = tile_x(t),
    y = tile_y(t);
  if (x == 0)
    return 3;
  else if (y == 0)
    return 2;
  else if (x == width - 1)
    return 1;
  else
    {
      assert(y == height - 1);
      return 0;
    }
}

void
ChipDB::dump(std::ostream &s) const
{
  s << ".device " << device << "\n\n";
  
  for (const auto &p : packages)
    {
      s << ".pins " << p.first << "\n";
      for (const auto &p2 : p.second.pin_loc)
        {
          int t = p2.second.tile();
          s << p2.first
            << " " << tile_x(t)
            << " " << tile_y(t)
            << " " << p2.second.pos() << "\n";
        }
      s << "\n";
    }
  
  s << ".colbuf\n";
  for (const auto &p : tile_colbuf_tile)
    s << tile_x(p.second) << " " << tile_y(p.second) << " "
      << tile_x(p.first) << " " << tile_y(p.first) << "\n";
  s << "\n";
  
  for (int i = 0; i < width; i ++)
    for (int j = 0; j < height; j ++)
      {
        int t = tile(i, j);
        switch(tile_type[t])
          {
          case TileType::EMPTY:
            break;
          case TileType::IO:
            s << ".io_tile " << i << " " << j << "\n";
            break;
          case TileType::LOGIC:
            s << ".logic_tile " << i << " " << j << "\n";
            break;
          case TileType::RAMB:
            s << ".ramb_tile " << i << " " << j << "\n";
            break;
          case TileType::RAMT:
            s << ".ramt_tile " << i << " " << j << "\n";
            break;
          case TileType::DSP0:
            s << ".dsp0_tile " << i << " " << j << "\n";
            break;
          case TileType::DSP1:
            s << ".dsp1_tile " << i << " " << j << "\n";
            break;
          case TileType::DSP2:
            s << ".dsp2_tile " << i << " " << j << "\n";
            break;
          case TileType::DSP3:
            s << ".dsp3_tile " << i << " " << j << "\n";
            break;
          case TileType::IPCON:
            s << ".ipcon_tile " << i << " " << j << "\n";
            break;
          }
        
        for (const auto &p : tile_nonrouting_cbits.at(tile_type[t]))
          {
            s << p.first;
            for (const auto &cbit : p.second)
              s << " " << cbit;
            s << "\n";
          }
        s << "\n";
      }
  
  std::vector<std::vector<std::pair<int, std::string>>> net_tile_names(n_nets);
  for (int i = 0; i < n_tiles; ++i)
    for (const auto &p : tile_nets[i])
      net_tile_names[p.second].push_back(std::make_pair(i, p.first));
  
  for (int i = 0; i < n_nets; ++i)
    {
      s << ".net " << i << "\n";
      for (const auto &p : net_tile_names[i])
        s << tile_x(p.first) << " " << tile_y(p.first) << " " << p.second << "\n";
      s << "\n";
    }
  
  for (unsigned i = 0; i < switches.size(); ++i)
    {
      const Switch &sw = switches[i];
      
      s << (sw.bidir ? ".routing" : ".buffer")
        << " " << tile_x(sw.tile) << " " << tile_y(sw.tile) << " " << sw.out;
      for (const CBit &cb : sw.cbits)
        s << " B" << cb.row << "[" << cb.col << "]";
      s << "\n";
      
      for (const auto &p : sw.in_val)
        {
          for (int j = 0; j < (int)sw.cbits.size(); ++j)
            {
              if (p.second & (1 << j))
                s << "1";
              else
                s << "0";
            }     
          s << " " << p.first << "\n";
        }
      s << "\n";
    }
}

void
ChipDB::set_device(const std::string &d,
                   int w, int h,
                   int n_nets_)
{
  device = d;
  width = w;
  height = h;
  n_tiles = width * height;
  n_nets = n_nets_;
  
  tile_type.resize(n_tiles, TileType::EMPTY);
  tile_nets.resize(n_tiles);
  
  net_tile_name.resize(n_nets);
  out_switches.resize(n_nets);
  in_switches.resize(n_nets);
}

class ChipDBParser : public LineParser
{
  ChipDB *chipdb;
  
  CBit parse_cbit(int tile, const std::string &s);
  
  void parse_cmd_device();
  void parse_cmd_pins();
  void parse_cmd_gbufpin();
  void parse_cmd_tile();
  void parse_cmd_tile_bits();
  void parse_cmd_net();
  void parse_cmd_buffer_routing();
  void parse_cmd_colbuf();
  void parse_cmd_gbufin();
  void parse_cmd_iolatch();
  void parse_cmd_ieren();
  void parse_cmd_extra_bits();
  void parse_cmd_extra_cell();
  
public:
  ChipDBParser(const std::string &f, std::istream &s_)
    : LineParser(f, s_), chipdb(nullptr)
  {}
  
  ChipDB *parse();
};

CBit
ChipDBParser::parse_cbit(int t, const std::string &s_)
{
  std::size_t lbr = s_.find('['),
    rbr = s_.find(']');
  
  if (s_[0] != 'B'
      || lbr == std::string::npos
      || rbr == std::string::npos)
    fatal("invalid cbit spec");
  
  std::string rows(&s_[1], &s_[lbr]),
    cols(&s_[lbr + 1], &s_[rbr]);
  
  int r = std::stoi(rows),
    c = std::stoi(cols);
  
  return CBit(t, r, c);
}

void
ChipDBParser::parse_cmd_device()
{
  if (words.size() != 5)
    fatal("wrong number of arguments");
  
  chipdb->set_device(words[1],
                     std::stoi(words[2]),
                     std::stoi(words[3]),
                     std::stoi(words[4]));
  
  // next command
  read_line();
}

void
ChipDBParser::parse_cmd_pins()
{
  if (words.size() != 2)
    fatal("wrong number of arguments");
  
  const std::string &package_name = words[1];
  Package &package = chipdb->packages[package_name];
  
  package.name = package_name;
  
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        return;
      
      if (words.size() != 4)
        fatal("invalid .pins entry");
      
      const std::string &pin = words[0];
      int x = std::stoi(words[1]),
        y = std::stoi(words[2]),
        pos = std::stoi(words[3]);
      int t = chipdb->tile(x, y);
      Location loc(t, pos);
      extend(package.pin_loc, pin, loc);
      extend(package.loc_pin, loc, pin);
    }
}

void
ChipDBParser::parse_cmd_gbufpin()
{
  if (words.size() != 1)
    fatal("wrong number of arguments");
              
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        return;
      
      if (words.size() != 4)
        fatal("invalid .gbufpin entry");
      
      int x = std::stoi(words[0]),
        y = std::stoi(words[1]),
        pos = std::stoi(words[2]),
        glb_num = std::stoi(words[3]);
      int t = chipdb->tile(x, y);
      Location loc(t, pos);
      extend(chipdb->loc_pin_glb_num, loc, glb_num);
      
      chipdb->add_cell(CellType::GB, Location(t, 2));
    }
}

void
ChipDBParser::parse_cmd_tile()
{
  if (words.size() != 3)
    fatal("wrong number of arguments");
  
  int x = std::stoi(words[1]),
    y = std::stoi(words[2]);
  if (x < 0 || x >= chipdb->width)
    fatal("tile x out of range");
  if (y < 0 || y >= chipdb->height)
    fatal("tile y out of range");
  
  int t = chipdb->tile(x, y);
  
  const std::string &cmd = words[0];
  if (cmd == ".io_tile")
    {
      chipdb->tile_type[t] = TileType::IO;
      for (int p = 0; p < 2; ++p)
        chipdb->add_cell(CellType::IO, Location(t, p));
    }
  else if (cmd == ".logic_tile")
    {
      chipdb->tile_type[t] = TileType::LOGIC;
                  
      for (int p = 0; p < 8; ++p)
        chipdb->add_cell(CellType::LOGIC, Location(t, p));
    }
  else if (cmd == ".ramb_tile")
    chipdb->tile_type[t] = TileType::RAMB;
  else if (cmd == ".ramt_tile")
    {
      chipdb->tile_type[t] = TileType::RAMT;
                  
      chipdb->add_cell(CellType::RAM, Location(t, 0));
    }
  else if (cmd == ".dsp0_tile")
    //could add a cell here, but do it using extra_cell because the CBITs differ depending on
    //location, and extra_cell is a better way of specifying this
    chipdb->tile_type[t] = TileType::DSP0; 
  else if (cmd == ".dsp1_tile")
    chipdb->tile_type[t] = TileType::DSP1; 
  else if (cmd == ".dsp2_tile")
    chipdb->tile_type[t] = TileType::DSP2; 
  else if (cmd == ".dsp3_tile")
    chipdb->tile_type[t] = TileType::DSP3; 
  else
    {
      assert(cmd == ".ipcon_tile");
      chipdb->tile_type[t] = TileType::IPCON; 
    }
  
  // next command
  read_line();
}

void
ChipDBParser::parse_cmd_tile_bits()
{
  if (words.size() != 3)
    fatal("wrong number of arguments");
  
  TileType ty;
  const std::string &cmd = words[0];
  if (cmd == ".io_tile_bits")
    ty = TileType::IO;
  else if (cmd == ".logic_tile_bits")
    ty = TileType::LOGIC;
  else if (cmd == ".ramb_tile_bits")
    ty = TileType::RAMB;
  else if (cmd == ".ramt_tile_bits")
    ty = TileType::RAMT;  
  else if (cmd == ".dsp0_tile_bits")
    ty = TileType::DSP0;
  else if (cmd == ".dsp1_tile_bits")
    ty = TileType::DSP1;
  else if (cmd == ".dsp2_tile_bits")
    ty = TileType::DSP2; 
  else if (cmd == ".dsp3_tile_bits")
    ty = TileType::DSP3;     
  else
    {
      assert(cmd == ".ipcon_tile_bits");
      ty = TileType::IPCON;
    }
  
  int n_columns = std::stoi(words[1]),
    n_rows = std::stoi(words[2]);
  
  extend(chipdb->tile_cbits_block_size,
         ty,
         std::make_pair(n_columns, n_rows));
  
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        return;
      
      if (words.size() < 2)
        fatal("invalid tile entry");
      
      const std::string &func = words[0];
      
      std::vector<CBit> cbits(words.size() - 1);
      for (unsigned i = 1; i < words.size(); ++i)
        cbits[i - 1] = parse_cbit(0, words[i]);
      
      extend(chipdb->tile_nonrouting_cbits[ty], func, cbits);
    }

}

void
ChipDBParser::parse_cmd_net()
{
  if (words.size() != 2)
    fatal("wrong number of arguments");
  
  int n = std::stoi(words[1]);
  if (n < 0)
    fatal("invalid net index");
  
  bool first = true;
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        return;
      
      if (words.size() != 3)
        fatal("invalid .net entry");
      
      int x = std::stoi(words[0]),
        y = std::stoi(words[1]);
      if (x < 0 || x >= chipdb->width)
        fatal("tile x out of range");
      if (y < 0 || y >= chipdb->height)
        fatal("tile y out of range");
      int t = chipdb->tile(x, y);
      
      if (first)
        {
          chipdb->net_tile_name[n] = std::make_pair(t, words[2]);
          first = false;
        }
      extend(chipdb->tile_nets[t], words[2], n);
    }
}

void
ChipDBParser::parse_cmd_buffer_routing()
{
  if (words.size() < 5)
    fatal("too few arguments");
  
  bool bidir = words[0] == ".routing";
  
  int x = std::stoi(words[1]),
    y = std::stoi(words[2]);
  if (x < 0 || x >= chipdb->width)
    fatal("tile x out of range");
  if (y < 0 || y >= chipdb->height)
    fatal("tile y out of range");
  int t = chipdb->tile(x, y);
  
  int n = std::stoi(words[3]);
  if (n < 0)
    fatal("invalid net index");
  
  std::vector<CBit> cbits(words.size() - 4);
  for (unsigned i = 4; i < words.size(); i ++)
    cbits[i - 4] = parse_cbit(t, words[i]);
  
  std::map<int, unsigned> in_val;             
  
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        {
          chipdb->switches.push_back(Switch(bidir,
                                            t,
                                            n,
                                            in_val,
                                            cbits));
          return;
        }
      
      const std::string &sval = words[0];
      
      if (words.size() != 2
          || sval.size() != cbits.size())
        fatal("invalid .buffer/.routing entry");
      
      int n2 = std::stoi(words[1]);
      
      unsigned val = 0;
      for (unsigned i = 0; i < sval.size(); i ++)
        {
          if (sval[i] == '1')
            val |= (1 << i);
          else
            {
              if (sval[i] != '0')
                fatal("invalid binary string");
            }
        }
      
      extend(in_val, n2, val);
    }
}

void
ChipDBParser::parse_cmd_colbuf()
{
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        return;
      
      if (words.size() != 4)
        fatal("invalid .colbuf entry");
      
      int src_x = std::stoi(words[0]);
      int src_y = std::stoi(words[1]);
      int dst_x = std::stoi(words[2]);
      int dst_y = std::stoi(words[3]);
      
      chipdb->tile_colbuf_tile[chipdb->tile(dst_x, dst_y)]
        = chipdb->tile(src_x, src_y);
    }
}

void
ChipDBParser::parse_cmd_gbufin()
{
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        return;
      
      if (words.size() != 3)
        fatal("invalid .gbufin entry");
      
      int g = std::stoi(words[2]);
      assert(g < chipdb->n_global_nets);
      
      extend(chipdb->gbufin,
             std::make_pair(std::stoi(words[0]), std::stoi(words[1])),
             g);
    }
}

void
ChipDBParser::parse_cmd_iolatch()
{
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        return;
      
      if (words.size() != 2)
        fatal("invalid .iolatch entry");
      
      int x = std::stoi(words[0]),
        y = std::stoi(words[1]);
      chipdb->iolatch.push_back(chipdb->tile(x, y));
    }
}

void
ChipDBParser::parse_cmd_ieren()
{
    for (;;)
      {
        read_line();
        if (eof()
            || line[0] == '.')
          return;
        
        if (words.size() != 6)
          fatal("invalid .ieren entry");
        
        int pio_t = chipdb->tile(std::stoi(words[0]),
                                 std::stoi(words[1])),
          ieren_t = chipdb->tile(std::stoi(words[3]),
                                 std::stoi(words[4]));
        
        Location pio(pio_t, std::stoi(words[2])),
          ieren(ieren_t, std::stoi(words[5]));
        extend(chipdb->ieren, pio, ieren);
      }
}

void
ChipDBParser::parse_cmd_extra_bits()
{
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        return;
      
      if (words.size() != 4)
        fatal("invalid .extra_bits entry");
      
      int bank_num = std::stoi(words[1]),
        addr_x = std::stoi(words[2]),
        addr_y = std::stoi(words[3]);
      
      extend(chipdb->extra_bits,
             words[0],
             std::make_tuple(bank_num, addr_x, addr_y));
    }
}

void
ChipDBParser::parse_cmd_extra_cell()
{
  if (words.size() < 4)
    fatal("wrong number of arguments to .extra_cell");
  
  
              
  const std::string &cell_type = words[(words.size() >= 5) ? 4 : 3];
  int x = std::stoi(words[1]),
    y = std::stoi(words[2]);
  int z = 0;
  if(words.size() >= 5)
    z = std::stoi(words[3]);
  int t = chipdb->tile(x, y);
  
  int c = 0;
  if (cell_type == "WARMBOOT")
    c = chipdb->add_cell(CellType::WARMBOOT, Location(t, 0));
  else if (cell_type == "PLL")
    c = chipdb->add_cell(CellType::PLL, Location(t, 3));
  else if (cell_type == "MAC16")
    c = chipdb->add_cell(CellType::MAC16, Location(t, z));
  else if (cell_type == "SPRAM") 
    c = chipdb->add_cell(CellType::SPRAM, Location(t, z));
  else if (cell_type == "LFOSC") 
    c = chipdb->add_cell(CellType::LFOSC, Location(t, z));
  else if (cell_type == "HFOSC") 
    c = chipdb->add_cell(CellType::HFOSC, Location(t, z));
  else if (cell_type == "RGBA_DRV") 
    c = chipdb->add_cell(CellType::RGBA_DRV, Location(t, z));
  else if (cell_type == "LEDDA_IP") 
    c = chipdb->add_cell(CellType::LEDDA_IP, Location(t, z));
  else if (cell_type == "I2C") 
    c = chipdb->add_cell(CellType::I2C_IP, Location(t, z));
  else if (cell_type == "SPI") 
    c = chipdb->add_cell(CellType::SPI_IP, Location(t, z));
  else if (cell_type == "IO_I3C")
    c = chipdb->add_cell(CellType::IO_I3C, Location(t, z));
  else
    fatal(fmt("unknown extra cell type `" << cell_type << "'"));
  
  std::map<std::string, std::pair<int, std::string>> mfvs;
  std::set<std::string> locked_pkgs;
  for (;;)
    {
      read_line();
      if (eof()
          || line[0] == '.')
        {
          extend(chipdb->cell_mfvs, c, mfvs);
          extend(chipdb->cell_locked_pkgs, c, locked_pkgs);
          return;
        }

      if (words.size() > 0 && words[0] == "LOCKED") {
        for (size_t i = 1; i < words.size(); i++)
          extend(locked_pkgs, words[i]);
        continue;
      }

      
      if (words.size() != 4)
        fatal("invalid .extra_cell entry");
      
      int mfv_t = chipdb->tile(std::stoi(words[1]),
                               std::stoi(words[2]));
      extend(mfvs, words[0],
             std::make_pair(mfv_t,
                            words[3]));
    }
}

ChipDB *
ChipDBParser::parse()
{
  chipdb = new ChipDB;
  
  read_line();
  for (;;)
    {
      if (eof())
        break;
      if (line[0] != '.')
        fatal(fmt("expected command, got '" << words[0] << "'"));
      
      const std::string &cmd = words[0];
      if (cmd == ".device")
        parse_cmd_device();
      else if (cmd == ".pins")
        parse_cmd_pins();
      else if (cmd == ".gbufpin")
        parse_cmd_gbufpin();
      else if (cmd == ".io_tile"
               || cmd == ".logic_tile"
               || cmd == ".ramb_tile"
               || cmd == ".ramt_tile"
               || cmd == ".dsp0_tile"
               || cmd == ".dsp1_tile"
               || cmd == ".dsp2_tile"
               || cmd == ".dsp3_tile"
               || cmd == ".ipcon_tile")
        parse_cmd_tile();
      else if (cmd == ".io_tile_bits"
               || cmd == ".logic_tile_bits"
               || cmd == ".ramb_tile_bits"
               || cmd == ".ramt_tile_bits"
               || cmd == ".dsp0_tile_bits"
               || cmd == ".dsp1_tile_bits"
               || cmd == ".dsp2_tile_bits"
               || cmd == ".dsp3_tile_bits"
               || cmd == ".ipcon_tile_bits")
        parse_cmd_tile_bits();
      else if (cmd == ".net")
        parse_cmd_net();
      else if (cmd == ".buffer"
               || cmd == ".routing")
        parse_cmd_buffer_routing();
      else if (cmd == ".colbuf")
        parse_cmd_colbuf();
      else if (cmd == ".gbufin")
        parse_cmd_gbufin();
      else if (cmd == ".iolatch")
        parse_cmd_iolatch();
      else if (cmd == ".ieren")
        parse_cmd_ieren();
      else if (cmd == ".extra_bits")
        parse_cmd_extra_bits();
      else if (cmd == ".extra_cell")
        parse_cmd_extra_cell();
      else
        fatal(fmt("unknown directive '" << cmd << "'"));
    }
  
  chipdb->finalize();
  return chipdb;
}

void
ChipDB::finalize()
{
  int t1c1 = tile(1, 1);
  for (const auto &p : tile_nets[t1c1])
    {
      if (is_prefix("glb_netwk_", p.first))
        {
          int n = std::stoi(&p.first[10]);
          extend(net_global, p.second, n);
        }
    }
  
  for (int i = 1; i <= n_cells; ++i)
    {
      int t = cell_location[i].tile();
      if (tile_type[t] != TileType::IO)
        continue;
      
      int b = tile_bank(t);
      bank_cells[b].push_back(i);
    }
  
  tile_pos_cell.resize(n_tiles);
  for (int i = 0; i < n_tiles; ++i)
    {
      switch(tile_type[i])
        {
        case TileType::LOGIC:
          tile_pos_cell[i].resize(8, 0);
          break;
        case TileType::IO:
          tile_pos_cell[i].resize(4, 0);
          break;
        case TileType::RAMT:
          tile_pos_cell[i].resize(1, 0);
          break;
        case TileType::DSP0:
          tile_pos_cell[i].resize(1, 0);
          break;
        case TileType::IPCON:
          tile_pos_cell[i].resize(1, 0);
          break;
        default:
          break;
        }
    }
  for (int i = 1; i <= n_cells; ++i)
    {
      const Location &loc = cell_location[i];
      // *logs << i << " " << loc << "\n";
      int t = loc.tile();
      int pos = loc.pos();
      if ((int)tile_pos_cell[t].size() <= pos)
        tile_pos_cell[t].resize(pos + 1, 0);
      assert(tile_pos_cell[t][pos] == 0);
      tile_pos_cell[t][pos] = i;
    }
  
  in_switches.resize(n_nets);
  out_switches.resize(n_nets);
  for (size_t s = 0;  s < switches.size(); ++s)
    {
      int out = switches[s].out;
      extend(out_switches[out], s);
      for (const auto &p : switches[s].in_val)
        extend(in_switches[p.first], s);
    }
}

int
ChipDB::find_switch(int in, int out) const
{
  std::vector<int> t;
  std::set_intersection(out_switches[out].begin(),
                        out_switches[out].end(),
                        in_switches[in].begin(),
                        in_switches[in].end(),
                        std::back_insert_iterator<std::vector<int>>(t));
  assert(t.size() == 1);
  int s = t[0];
  assert(switches[s].out == out);
  assert(contains_key(switches[s].in_val, in));
  return s;
}

void
ChipDB::bwrite(obstream &obs) const
{
  std::vector<std::string> net_names;
  std::map<std::string, int> net_name_idx;
  
  std::vector<std::map<int, int>> tile_nets_idx(n_tiles);
  for (int t = 0; t < n_tiles; ++t)
    {
      for (const auto &p : tile_nets[t])
        {
          int ni;
          auto i = net_name_idx.find(p.first);
          if (i == net_name_idx.end())
            {
              ni = net_name_idx.size();
              net_names.push_back(p.first);
              net_name_idx.insert(std::make_pair(p.first, ni));
            }
          else
            ni = i->second;
          extend(tile_nets_idx[t], ni, p.second);
        }
    }

  obs << std::string(version_str)
      << device
      << width
      << height
    // n_tiles = width * height
      << n_nets
    // n_global_nets = 8
      << packages
      << loc_pin_glb_num
      << iolatch
      << ieren
      << extra_bits
      << gbufin
      << tile_colbuf_tile
      << tile_type
    // net_tile_name
      << net_names
      << tile_nets_idx // tile_nets
      << tile_nonrouting_cbits
      << n_cells
      << cell_type
      << cell_location
      << cell_mfvs
      << cell_locked_pkgs
      << cell_type_cells
    // bank_cells
      << switches
    // in_switches, out_switches
      << tile_cbits_block_size;
}

void
ChipDB::bread(ibstream &ibs)
{
  std::vector<std::string> net_names;
  std::vector<std::map<int, int>> tile_nets_idx;
  std::string dbversion;
  ibs >> dbversion;
  if(dbversion != version_str)
   {
     fatal(fmt("chipdb and arachne-pnr versions do not match (chipdb: "
              << dbversion 
              << ", arachne-pnr: "
              << version_str << ")"));
   }
  ibs >> device
      >> width
      >> height
    // n_tiles = width * height
      >> n_nets
    // n_global_nets = 8
      >> packages
      >> loc_pin_glb_num
      >> iolatch
      >> ieren
      >> extra_bits
      >> gbufin
      >> tile_colbuf_tile
      >> tile_type
    // net_tile_name
      >> net_names
      >> tile_nets_idx // tile_nets
      >> tile_nonrouting_cbits
      >> n_cells
      >> cell_type
      >> cell_location
      >> cell_mfvs
      >> cell_locked_pkgs
      >> cell_type_cells
    // bank_cells
      >> switches
    // in_switches, out_switches
      >> tile_cbits_block_size;
  
  n_tiles = width * height;
  
  tile_nets_idx.resize(n_tiles);
  tile_nets.resize(n_tiles);
  for (int i = 0; i < n_tiles; ++i)
    {
      for (const auto &p : tile_nets_idx[i])
        extend(tile_nets[i], net_names[p.first], p.second);
    }
  
  finalize();
}

ChipDB *
read_chipdb(const std::string &filename)
{
  std::string expanded = expand_filename(filename);
  std::ifstream ifs(expanded, std::ifstream::in | std::ifstream::binary);
  if (ifs.fail())
    fatal(fmt("read_chipdb: failed to open `" << expanded << "': "
              << strerror(errno)));
  ChipDB *chipdb;
  if (is_suffix(expanded, ".bin"))
    {
      chipdb = new ChipDB;
      ibstream ibs(ifs);
      chipdb->bread(ibs);
    }
  else
    {
      ChipDBParser parser(filename, ifs);
      chipdb = parser.parse();
    }
  return chipdb;
}

std::string
cell_type_name(CellType ct)
{
  switch(ct)
    {
    case CellType::LOGIC:  return "LC";
    case CellType::IO:  return "IO";
    case CellType::GB:  return "GB";
    case CellType::RAM:  return "RAM";
    case CellType::WARMBOOT:  return "WARMBOOT";
    case CellType::PLL:  return "PLL";
    case CellType::MAC16:  return "MAC16";
    case CellType::SPRAM:  return "SPRAM";
    case CellType::LFOSC:  return "LFOSC";
    case CellType::HFOSC:  return "HFOSC";
    case CellType::RGBA_DRV:  return "RGBA_DRV";
    case CellType::LEDDA_IP:  return "LEDDA_IP";
    case CellType::I2C_IP:  return "I2C";
    case CellType::SPI_IP:  return "SPI";
    case CellType::IO_I3C:  return "IO_I3C";

    default:  abort();
    }
}

CBit
ChipDB::extra_cell_cbit(int c, const std::string &name, bool is_ip) const
{
  const auto &p = cell_mfvs.at(c).at(name);
  std::string prefix = "PLL.";
  if((tile_type[p.first] == TileType::DSP0) || (tile_type[p.first] == TileType::DSP1) ||
     (tile_type[p.first] == TileType::DSP2) || (tile_type[p.first] == TileType::DSP3) ||
     (tile_type[p.first] == TileType::IPCON) || is_ip)
     prefix = "IpConfig.";
  const auto &cbits = tile_nonrouting_cbits.at(tile_type[p.first]).at(prefix + p.second);
  assert(cbits.size() == 1);
  const CBit &cbit0 = cbits[0];
  return cbit0.with_tile(p.first);
}

std::string
ChipDB::extra_cell_netname(int c, const std::string &name) const
{
  const auto &p = cell_mfvs.at(c).at(name);
  return p.second;
}

int
ChipDB::get_oscillator_glb(int cell, const std::string &net) const
{
  std::string netname = extra_cell_netname(cell, net);
  const std::string prefix = "glb_netwk_";
  assert(netname.substr(0, prefix.length()) == prefix);
  int driven_glb = std::stoi(netname.substr(prefix.length()));
  return driven_glb;
}
