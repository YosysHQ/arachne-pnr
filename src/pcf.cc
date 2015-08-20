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

#include <cstring>
#include <istream>
#include <fstream>

class Design;
class Model;
class Constraints;

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
