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
#include "chipdb.hh"
#include "line_parser.hh"

#include <cassert>
#include <cstring>
#include <iostream>
#include <fstream>

std::ostream &
operator<<(std::ostream &s, const CBit &cbit)
{
  return s << cbit.x << " " << cbit.y << " B" << cbit.row << "[" << cbit.col << "]";
}

bool
CBit::operator==(const CBit &rhs) const
{
  return x == rhs.x
    && y == rhs.y
    && row == rhs.row
    && col == rhs.col;
}

bool
CBit::operator<(const CBit &rhs) const
{
  if (x < rhs.x)
    return true;
  if (x > rhs.x)
    return false;
  
  if (y < rhs.y)
    return true;
  if (y > rhs.y)
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

hashset<CBit>
CBitVal::cbits() const
{
  return unordered_keys(cbit_val);
}

std::string
tile_type_name(TileType t)
{
  assert(t != TileType::NO_TILE);
  switch(t)
    {
    case TileType::IO_TILE:
      return "io_tile";
    case TileType::LOGIC_TILE:
      return "logic_tile";
    case TileType::RAMB_TILE:
      return "ramb_tile";
    case TileType::RAMT_TILE:
      return "ramt_tile";
    case TileType::NO_TILE:
      abort();
    }    
  return std::string();
}

ChipDB::ChipDB()
  : width(0), height(0), n_tiles(0), n_nets(0), n_global_nets(8),
    bank_tiles(4)
{
}

int
ChipDB::tile_bank(int t) const
{
  assert(tile_type[t] == TileType::IO_TILE);
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
	s << p2.first
	  << " " << p2.second.x()
	  << " " << p2.second.y() << " "
	  << " " << p2.second.pos() << "\n";
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
	  case TileType::NO_TILE:
	    break;
	  case TileType::IO_TILE:
	    s << ".io_tile " << i << " " << j << "\n";
	    break;
	  case TileType::LOGIC_TILE:
	    s << ".logic_tile " << i << " " << j << "\n";
	    break;
	  case TileType::RAMB_TILE:
	    s << ".ramb_tile " << i << " " << j << "\n";
	    break;
	  case TileType::RAMT_TILE:
	    s << ".ramt_tile " << i << " " << j << "\n";
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
	  std::copy(p.second.begin(),
		    p.second.end(),
		    std::ostream_iterator<bool>(s));
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
  
  tile_type.resize(n_tiles, TileType::NO_TILE);
  tile_nets.resize(n_tiles);
  
  net_tile_name.resize(n_nets);
  out_switches.resize(n_nets);
  in_switches.resize(n_nets);
  
  for (int t = 0; t < n_tiles; ++t)
    {
      if (tile_type[t] != TileType::IO_TILE)
	continue;
      
      int b = tile_bank(t);
      bank_tiles[b].push_back(t);
    }
}

class ChipDBParser : public LineParser
{
  CBit parse_cbit(int x, int y, const std::string &s);
  
public:
  ChipDBParser(const std::string &f, std::istream &s_)
    : LineParser(f, s_)
  {}
  
  ChipDB *parse();
};

CBit
ChipDBParser::parse_cbit(int x, int y, const std::string &s_)
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
  
  return CBit(x, y, r, c);
}

ChipDB *
ChipDBParser::parse()
{
  ChipDB *chipdb = new ChipDB;
  
  for (;;)
    {
      if (eof())
	return chipdb;
      
      read_line();
      if (words.empty())
	continue;
      
      if (line[0] == '.')
	{
	L:
	  std::string cmd = words[0];
	  if (cmd == ".device")
	    {
	      if (words.size() != 5)
		fatal("wrong number of arguments");
	      
	      chipdb->set_device(words[1],
				 std::stoi(words[2]),
				 std::stoi(words[3]),
				 std::stoi(words[4]));
	    }
	  else if (cmd == ".pins")
	    {
	      if (words.size() != 2)
		fatal("wrong number of arguments");
	      
	      const std::string &package_name = words[1];
	      Package &package = chipdb->packages[package_name];
	      
	      package.name = package_name;
	      
	      for (;;)
		{
		  if (eof())
		    return chipdb;
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    goto L;
		  
		  if (words.size() != 4)
		    fatal("invalid .pins entry");
		  
		  const std::string &pin = words[0];
		  int x = std::stoi(words[1]),
		    y = std::stoi(words[2]),
		    pos = std::stoi(words[3]);
		  Location loc(x, y, pos);
		  extend(package.pin_loc, pin, loc);
		  extend(package.loc_pin, loc, pin);
		}
	    }
	  else if (cmd == ".gbufpin")
	    {
	      if (words.size() != 1)
		fatal("wrong number of arguments");
	      
	      for (;;)
		{
		  if (eof())
		    return chipdb;
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    goto L;
		  
		  if (words.size() != 4)
		    fatal("invalid .gbufpin entry");
		  
		  int x = std::stoi(words[0]),
		    y = std::stoi(words[1]),
		    pos = std::stoi(words[2]),
		    glb_num = std::stoi(words[3]);
		  Location loc(x, y, pos);
		  extend(chipdb->loc_pin_glb_num, loc, glb_num);
		}
	      
	    }
	  else if (cmd == ".io_tile"
		   || cmd == ".logic_tile"
		   || cmd == ".ramb_tile"
		   || cmd == ".ramt_tile")
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
	      
	      if (cmd == ".io_tile")
		chipdb->tile_type[t] = TileType::IO_TILE;
	      else if (cmd == ".logic_tile")
		chipdb->tile_type[t] = TileType::LOGIC_TILE;
	      else if (cmd == ".ramb_tile")
		chipdb->tile_type[t] = TileType::RAMB_TILE;
	      else
		{
		  assert(cmd == ".ramt_tile");
		  chipdb->tile_type[t] = TileType::RAMT_TILE;
		}
	    }
	  else if (cmd == ".io_tile_bits"
		   || cmd == ".logic_tile_bits"
		   || cmd == ".ramb_tile_bits"
		   || cmd == ".ramt_tile_bits")
	    {
	      if (words.size() != 3)
		fatal("wrong number of arguments");
	      
	      TileType ty;
	      if (cmd == ".io_tile_bits")
		ty = TileType::IO_TILE;
	      else if (cmd == ".logic_tile_bits")
		ty = TileType::LOGIC_TILE;
	      else if (cmd == ".ramb_tile_bits")
		ty = TileType::RAMB_TILE;
	      else
		{
		  assert(cmd == ".ramt_tile_bits");
		  ty = TileType::RAMT_TILE;
		}
	      
	      int n_columns = std::stoi(words[1]),
		n_rows = std::stoi(words[2]);
	      
	      extend(chipdb->tile_cbits_block_size,
		     ty,
		     std::make_pair(n_columns, n_rows));
	      
	      for (;;)
		{
		  if (eof())
		    return chipdb;
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    goto L;
		  
		  if (words.size() < 2)
		    fatal("invalid tile entry");
		  
		  const std::string &func = words[0];
		  
		  std::vector<CBit> cbits(words.size() - 1);
		  for (unsigned i = 1; i < words.size(); ++i)
		    cbits[i - 1] = parse_cbit(0, 0, words[i]);
		  
		  extend(chipdb->tile_nonrouting_cbits[ty], func, cbits);
		}
	    }
	  else if (cmd == ".net")
	    {
	      if (words.size() != 2)
		fatal("wrong number of arguments");
	      
	      int n = std::stoi(words[1]);
	      if (n < 0)
		fatal("invalid net index");
	      
	      bool first = true;
	      for (;;)
		{
		  if (eof())
		    return chipdb;
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    goto L;
		  
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
	  else if (cmd == ".buffer"
		   || cmd == ".routing")
	    {
	      if (words.size() < 5)
		fatal("too few arguments");
	      
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
		cbits[i - 4] = parse_cbit(x, y, words[i]);
	      
	      hashmap<int, std::vector<bool>> in_val;	      
	      
	      for (;;)
		{
		  if (eof())
		    return chipdb;
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    {
		      // FIXME
		      int s2 = chipdb->switches.size();
		      chipdb->switches.push_back(Switch(cmd == ".routing",
							t,
							n,
							in_val,
							cbits));
		      
		      extend(chipdb->out_switches[n], s2);
		      for (const auto &p : in_val)
			extend(chipdb->in_switches[p.first], s2);
		      
		      goto L;
		    }
		  
		  const std::string &sval = words[0];
		  
		  if (words.size() != 2
		      || sval.size() != cbits.size())
		    fatal("invalid .buffer/.routing entry");
		  
		  int n2 = std::stoi(words[1]);
		  
		  std::vector<bool> val;
		  for (unsigned i = 0; i < sval.size(); i ++)
		    {
		      if (sval[i] == '1')
			val.push_back(true);
		      else
			{
			  if (sval[i] != '0')
			    fatal("invalid binary string");
			  
			  val.push_back(false);
			}
		    }
		  assert(val.size() == cbits.size());
		  
		  extend(in_val, n2, val);
		}
	    }
	  else if (cmd == ".colbuf")
	    {
	      for (;;)
		{
		  if (eof())
		    return chipdb;
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    goto L;
		  
		  if (words.size() != 4)
		    fatal("invalid .colbuf entry");
		  
		  int src_x = std::stoi(words[0]);
		  int src_y = std::stoi(words[1]);
		  int dst_x = std::stoi(words[2]);
		  int dst_y = std::stoi(words[3]);
		  
		  chipdb->tile_colbuf_tile[chipdb->tile(dst_x, dst_y)] = chipdb->tile(src_x, src_y);
		}
	    }
	  else if (cmd == ".gbufin")
	    {
	      for (;;)
		{
		  if (eof())
		    return chipdb;
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    goto L;
		  
		  if (words.size() != 3)
		    fatal("invalid .gbufin entry");
		  
		  int g = std::stoi(words[2]);
		  assert(g < chipdb->n_global_nets);
		  
		  extend(chipdb->gbufin,
			 std::make_pair(std::stoi(words[0]), std::stoi(words[1])),
			 g);
		}
	    }
	  else if (cmd == ".iolatch")
	    {
	      for (;;)
		{
		  if (eof())
		    return chipdb;
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    goto L;
		  
		  if (words.size() != 2)
		    fatal("invalid .iolatch entry");
		  
		  int x = std::stoi(words[0]),
		    y = std::stoi(words[1]);
		  chipdb->iolatch.push_back(chipdb->tile(x, y));
		}
	    }
	  else if (cmd == ".ieren")
	    {
	      for (;;)
		{
		  if (eof())
		    return chipdb;
	      
		  read_line();
		  if (words.empty())
		    continue;
	      
		  if (line[0] == '.')
		    goto L;
	      
		  if (words.size() != 6)
		    fatal("invalid .ieren entry");
	      
		  Location pio(std::stoi(words[0]),
			       std::stoi(words[1]),
			       std::stoi(words[2])),
		    ieren(std::stoi(words[3]),
			  std::stoi(words[4]),
			  std::stoi(words[5]));
		  extend(chipdb->ieren, pio, ieren);
		}
	    }
	  else if (cmd == ".extra_bits")
	    {
	      for (;;)
		{
		  if (eof())
		    return chipdb;
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    goto L;
		  
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
	  else if (cmd == ".extra_cell")
	    {
	      if (words.size() != 4)
		fatal("wrong number of arguments to .extra_cell");
	      
	      int t = chipdb->tile(std::stoi(words[1]),
				   std::stoi(words[2]));
	      chipdb->extra_cell_tile.push_back(t);
	      chipdb->extra_cell_name.push_back(words[3]);
	      
	      std::map<std::string, std::pair<int, std::string>> mfvs;
	      for (;;)
		{
		  if (eof())
		    {
		      chipdb->extra_cell_mfvs.push_back(mfvs);
		      return chipdb;
		    }
		  
		  read_line();
		  if (words.empty())
		    continue;
		  
		  if (line[0] == '.')
		    {
		      chipdb->extra_cell_mfvs.push_back(mfvs);
		      goto L;
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
	  else
	    fatal(fmt("unknown directive '" << cmd << "'"));
	}
      else
	fatal(fmt("expected directive, got '" << words[0] << "'"));
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

ChipDB *
read_chipdb(const std::string &filename)
{
  std::string expanded = expand_filename(filename);
  std::ifstream fs(expanded);
  if (fs.fail())
    fatal(fmt("read_chipdb: failed to open `" << expanded << "': "
	      << strerror(errno)));
  ChipDBParser parser(filename, fs);
  return parser.parse();
}
