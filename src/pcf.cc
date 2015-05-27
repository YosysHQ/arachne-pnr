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
#include "netlist.hh"
#include "pcf.hh"
#include "line_parser.hh"

#include <istream>
#include <fstream>

class Design;
class Model;
class Constraints;

class PCFParser : public LineParser
{
  Design *d;
  Model *top;
  Constraints &constraints;
  
public:
  PCFParser(const std::string &f, std::istream &s, Design *d_, Constraints &c)
    : LineParser(f, s),
      d(d_),
      top(d->top()),
      constraints(c)
  {}
  
  void parse();
};

void
PCFParser::parse()
{
  std::unordered_map<std::string, int> net_pin;
  
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
	  if (words.size() != 3)
	    fatal("set_io: wrong number of arguments");
	  
	  const std::string &net_name = words[1];
	  Port *p = top->find_port(net_name);
	  if (!p)
	    fatal(fmt("no port `" << net_name << "' in top-level module `" << top->name() << "'"));
	  
	  int pin = std::stoi(words[2]);
	  auto i = net_pin.find(net_name);
	  if (i == net_pin.end())
	    net_pin[net_name] = pin;
	  else
	    fatal(fmt("duplicate pin constraints for net `" << net_name << "'"));
	}
      else
	fatal(fmt("unknown command `" << cmd << "'"));
    }
  
  constraints.net_pin = net_pin;
}

void
read_pcf(const std::string &filename,
	 Design *d,
	 Constraints &constraints)
{
  std::string expanded = expand_filename(filename);
  std::ifstream fs(expanded);
  if (fs.fail())
    fatal(fmt("read_pcf: failed to open `" << expanded << "': "
	      << strerror(errno)));
  PCFParser parser(filename, fs, d, constraints);
  return parser.parse();
}
