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
#include "netlist.hh"
#include "pcf.hh"
#include "line_parser.hh"
#include "designstate.hh"
#include "casting.hh"

#include <cstring>
#include <istream>
#include <fstream>

class PCFParser : public LineParser
{
  const Package &package;
  Design *d;
  Model *top;
  Constraints &constraints;
  
public:
  PCFParser(const std::string &f, std::istream &s_, const Package &p, Design *d_, Constraints &c)
    : LineParser(f, s_),
      package(p),
      d(d_),
      top(d->top()),
      constraints(c)
  {}
  
  void parse();
};

void
PCFParser::parse()
{
  std::map<std::string, Location> net_pin_loc;
  
  for (;;)
    {
      if (eof())
	break;
      
      read_line();
      if (words.empty())
	continue;
      
      const std::string &cmd = words[0];
      if (cmd == "set_io")
	{
	  bool err_no_port = true;
	  
	  const char *net_name = nullptr,
	    *pin_name = nullptr;
	  for (int i = 1; i < (int)words.size(); ++i)
	    {	  
	      if (words[i][0] == '-')
		{
		  if (words[1] == "--warn-no-port")
		    err_no_port = false;
		  else
		    fatal(fmt("unknown option `" << words[1] << "'"));
		}
	      else
		{
		  if (net_name == nullptr)
		    net_name = words[i].c_str();
		  else if (pin_name == nullptr)
		    pin_name = words[i].c_str();
		  else
		    fatal("set_io: too many arguments");
		}
	    }
	  
	  if (!net_name || !pin_name)
	    fatal("set_io: too few arguments");
	  
	  Port *p = top->find_port(net_name);
	  if (!p)
	    {
	      if (err_no_port)
		fatal(fmt("no port `" << net_name << "' in top-level module `"
			  << top->name() << "'"));
	      else
		{
		  warning(fmt("no port `" << net_name << "' in top-level module `"
			      << top->name() << "', constraint ignored."));
		  continue;
		}
	    }
	  
	  auto i = package.pin_loc.find(pin_name);
	  if (i == package.pin_loc.end())
	    fatal(fmt("unknown pin `" << pin_name << "' on package `"
		      << package.name << "'"));
	  
	  const Location &loc = i->second;
	  
	  auto j = net_pin_loc.find(net_name);
	  if (j != net_pin_loc.end())
	    fatal(fmt("duplicate pin constraints for net `" << net_name << "'"));
	  
	  extend(net_pin_loc, net_name, loc);
	}
      else
	fatal(fmt("unknown command `" << cmd << "'"));
    }
  
  constraints.net_pin_loc = net_pin_loc;
}

void
read_pcf(const std::string &filename,
	 const Package &package,
	 Design *d,
	 Constraints &constraints)
{
  std::string expanded = expand_filename(filename);
  std::ifstream fs(expanded);
  if (fs.fail())
    fatal(fmt("read_pcf: failed to open `" << expanded << "': "
	      << strerror(errno)));
  PCFParser parser(filename, fs, package, d, constraints);
  return parser.parse();
}

class ConstraintsPlacer
{
  DesignState &ds;
  const ChipDB *chipdb;
  const Models &models;
  Model *top;
  const Constraints &constraints;
  
  BasedVector<Instance *, 1> cell_gate;
  
  Instance *top_port_io_gate(const std::string &net_name);
  
public:
  ConstraintsPlacer(DesignState &ds_);
  
  void place();
};

Instance *
ConstraintsPlacer::top_port_io_gate(const std::string &net_name)
{
  Port *p = top->find_port(net_name);
  assert(p);
  
  Port *p2 = p->connection_other_port();
  
  Instance *inst = cast<Instance>(p2->node());
  assert(models.is_ioX(inst)
	 || models.is_pllX(inst));
  
  return inst;
}

ConstraintsPlacer::ConstraintsPlacer(DesignState &ds_)
  : ds(ds_),
    chipdb(ds.chipdb),
    models(ds.models),
    top(ds.top),
    constraints(ds.constraints),
    cell_gate(chipdb->n_cells, nullptr)
{
  assert(ds.placement.empty());
}

void
ConstraintsPlacer::place()
{
  std::vector<Net *> bank_latch(4, nullptr);
  for (const auto &p : constraints.net_pin_loc)
    {
      Instance *inst = top_port_io_gate(p.first);
      
      // FIXME handle pll, gb_io
      
      const Location &loc = p.second;
      int t = loc.tile();
      int b = chipdb->tile_bank(t);
      
      int c = 0;
      if (models.is_ioX(inst))
	{
	  Net *latch = inst->find_port("LATCH_INPUT_VALUE")->connection();
	  if (latch)
	    {
	      if (bank_latch[b])
		{
		  if (bank_latch[b] != latch)
		    fatal(fmt("pcf error: multiple LATCH_INPUT_VALUE drivers in bank " << b));
		}
	      else
		bank_latch[b] = latch;
	    }
	  
	  if (inst->get_param("IO_STANDARD").as_string() == "SB_LVDS_INPUT"
	      && b != 3)
	    fatal(fmt("pcf error: LVDS port `" << p.first << "' not in bank 3\n"));
	  
	  Location loc_other(t,
			     loc.pos() ? 0 : 1);
	  int cell_other = chipdb->loc_cell(loc_other);
	  if (cell_other)
	    {
	      Instance *inst_other = cell_gate[cell_other];
	      if (inst_other)
		{
		  if (inst->get_param("NEG_TRIGGER").get_bit(0)
		      != inst_other->get_param("NEG_TRIGGER").get_bit(0))
		    {
		      int x = chipdb->tile_x(t),
			y = chipdb->tile_y(t);
		      fatal(fmt("pcf error: incompatible NEG_TRIGGER parameters in PIO at (" 
				<< x << ", " << y << ")"));
		    }
		}
	    }
      
	  c = chipdb->loc_cell(loc);
	}
      else
	{
	  assert(models.is_pllX(inst));
	  
	  Location pll_loc(loc.tile(), 3);
	  
	  c = chipdb->loc_cell(pll_loc);
	  if (!c)
	    fatal(fmt("bad constraint on `"
		      << p.first << "': no PLL at pin "
		      << ds.package.loc_pin.at(loc)));
	  // FIXME check for conflicting IO
	}
      
      cell_gate[c] = inst;
      extend(ds.placement, inst, c);
    }
  
  for (Instance *inst : top->instances())
    {
      if (contains(ds.placement, inst))
	continue;
      
      if (models.is_gb_io(inst))
	fatal("physical constraint required for GB_IO");
      else if (models.is_pllX(inst))
	{
	  if (inst->has_attr("location"))
	    {
	      const auto &a = inst->get_attr("location");
	      int x, y;
	      // FIXME check retval
	      sscanf(a.as_string().c_str(), "%d %d", &x, &y);
	      
	      Location pll_loc(chipdb->tile(x, y), 3);
	      int c = chipdb->loc_cell(pll_loc);
	      if (!c)
		fatal(fmt("bad location attribute: no PLL at `"
			  << x << " " << y << "'"));
	      
	      // FIXME check for conflicting IO
	      
	      cell_gate[c] = inst;
	      extend(ds.placement, inst, c);
	    }
	  else 
	    {
	      // FIXME place randomly
	      if (inst->find_port("PACKAGEPIN"))
		fatal("physical constraint required for PAD PLL");
	      else
		fatal("location attribute required for PAD PLL");
	    }
	}
    }
}

void
place_constraints(DesignState &ds)
{
  ConstraintsPlacer placer(ds);
  placer.place();
}
