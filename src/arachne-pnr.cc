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

#include "netlist.hh"
#include "chipdb.hh"
#include "blif.hh"
#include "pack.hh"
#include "io.hh"
#include "place.hh"
#include "route.hh"
#include "configuration.hh"
#include "pcf.hh"
#include "casting.hh"
#include "global.hh"
#include "carry.hh"
#include "constant.hh"

#include <iostream>
#include <fstream>
#include <cstring>

const char *program_name;

class null_streambuf : public std::streambuf
{
public:
  int overflow(int c) override { return c; }  
};

void
usage()
{
  std::cout 
    << "Usage:\n"
    << "\n"
    << "  " << program_name << " [options] [input-file]\n"
    << "\n"
    << "Place and route netlist.  Input file is in BLIF format.  Output is\n"
    << "(text) bitstream.\n"
    << "\n"
    << "    -h, --help\n"
    << "        Print this usage message.\n"
    << "\n"
    << "    -q, --quiet\n"
    << "        Run quite.  Don't output progress messages.\n"
    << "\n"
    << "    -d <device>, --device <device>\n"
    << "        Target device <device>.  Supported devices:\n"
    << "          1k - Lattice Semiconductor iCE40LP/HX1K\n"
    << "          8k - Lattice Semiconductor iCE40LP/HX8K\n"
    << "        Default: 1k\n"
    << "\n"
    << "    -c <file>, --chipdb <chipdb-file>\n"
    << "        Read chip database from <chipdb-file>.\n"
    << "        Default: /usr/local/share/icebox/chipdb-<device>.txt\n"
    << "\n"
    << "    -l, --no-promote-globals\n"
    << "        Don't promote nets to globals.\n"
    << "\n"
    << "    -B <file>, --post-pack-blif <file>\n"
    << "        Write post-pack netlist to <file> as BLIF.\n"
    << "    -V <file>, --post-pack-verilog <file>\n"
    << "        Write post-pack netlist to <file> as Verilog.\n"
    << "\n"
    << "    --post-place-blif <file>\n"
    << "        Write post-place netlist to <file> as BLIF.\n"
    << "\n"
    << "    --route-only\n"
    << "        Input must include placement.\n"
    << "\n"
    << "    -p <pcf-file>, --pcf-file <pcf-file>\n"
    << "        Read physical constraints from <pcf-file>.\n"
    << "\n"
    << "    -P <package>, --package <package>\n"
    << "        Target package <package>.\n"
    << "        Default: tq144 for 1k, ct256 for 8k\n"
    << "\n"
    << "    -s <int>, --seed <int>\n"
    << "        Set seed for random generator to <int>.\n"
    << "\n"
    << "    -w <pcf-file>, --write-pcf <pcf-file>\n"
    << "        Write pin assignments to <pcf-file> after placement.\n"
    << "\n"
    << "    -o <output-file>, --output-file <output-file>\n"
    << "        Write output to <output-file>.\n";
}

struct null_ostream : public std::ostream
{
  null_ostream() : std::ostream(0) {}
};

int
main(int argc, const char **argv)
{
  program_name = argv[0];
  
  bool help = false,
    quiet = false,
    do_promote_globals = true,
    route_only = false;
  std::string device = "1k";
  const char *chipdb_file = nullptr,
    *input_file = nullptr,
    *package_name_cp = nullptr,
    *pcf_file = nullptr,
    *post_place_pcf = nullptr,
    *pack_blif = nullptr,
    *pack_verilog = nullptr,
    *place_blif = nullptr,
    *output_file = nullptr,
    *seed_str = nullptr;
  
  for (int i = 1; i < argc; ++i)
    {
      if (argv[i][0] == '-')
	{
	  if (!strcmp(argv[i], "-h")
	      || !strcmp(argv[i], "--help"))
	    help = true;
	  else if (!strcmp(argv[i], "-q")
	      || !strcmp(argv[i], "--quiet"))
	    quiet = true;
	  else if (!strcmp(argv[i], "-d")
	      || !strcmp(argv[i], "--device"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      device = argv[i];
	    }
	  else if (!strcmp(argv[i], "-c")
		   || !strcmp(argv[i], "--chipdb"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      chipdb_file = argv[i];
	    }
	  else if (!strcmp(argv[i], "-l")
		   || !strcmp(argv[i], "--no-promote-globals"))
	    do_promote_globals = false;
	  else if (!strcmp(argv[i], "-B")
	      || !strcmp(argv[i], "--post-pack-blif"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      pack_blif = argv[i];
	    }
	  else if (!strcmp(argv[i], "-V")
	      || !strcmp(argv[i], "--post-pack-verilog"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      pack_verilog = argv[i];
	    }
	  else if (!strcmp(argv[i], "--post-place-blif"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      place_blif = argv[i];
	    }
	  else if (!strcmp(argv[i], "--route-only"))
	    route_only = true;
	  else if (!strcmp(argv[i], "-p")
		   || !strcmp(argv[i], "--pcf-file"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      pcf_file = argv[i];
	    }
	  else if (!strcmp(argv[i], "-P")
		   || !strcmp(argv[i], "--package"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      package_name_cp = argv[i];
	    }
	  else if (!strcmp(argv[i], "-w")
		   || !strcmp(argv[i], "--write-pcf"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      post_place_pcf = argv[i];
	    }
	  else if (!strcmp(argv[i], "-s")
		   || !strcmp(argv[i], "--seed"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      seed_str = argv[i];
	    }
	  else if (!strcmp(argv[i], "-o")
		   || !strcmp(argv[i], "--output-file"))
	    {
	      if (i + 1 >= argc)
		fatal(fmt(argv[i] << ": expected argument"));
	      
	      ++i;
	      output_file = argv[i];
	    }
	  else
	    fatal(fmt("unknown option `" << argv[i] << "'"));
	}
      else
	{
	  if (input_file)
	    fatal("too many command-line arguments");
	  else
	    input_file = argv[i];
	}
    }
  
  if (help)
    {
      usage();
      exit(EXIT_SUCCESS);
    }
  
  if (device != "1k"
      && device != "8k")
    fatal(fmt("unknown device: " << device));
  
  std::string package_name;
  if (package_name_cp)
    package_name = package_name_cp;
  else if (device == "1k")
    package_name = "tq144";
  else
    {
      assert(device == "8k");
      package_name = "ct256";
    }
  
  std::ostream *null_ostream = nullptr;
  if (quiet)
    logs = null_ostream = new std::ostream(new null_streambuf);
  else
    logs = &std::cerr;
  
  // FIXME: use random_device by default
  unsigned seed = 0;
  if (seed_str)
    {
      // FIXME catch exception
      seed = std::stoi(seed_str);
      if (!seed)
	fatal("zero seed\n");
    }
  else
    {
      std::random_device rd;
      do {
	seed = rd();
      } while (seed == 0);
    }
  *logs << "seed: " << seed << "\n";
  
  random_generator rg(seed);
  
  *logs << "device: " << device << "\n";
  std::string chipdb_file_s;
  if (chipdb_file)
    chipdb_file_s = chipdb_file;
  else
    chipdb_file_s = (std::string("/usr/local/share/icebox/chipdb-")
		     + device 
		     + ".txt");
  *logs << "read_chipdb " << chipdb_file_s << "...\n";
  const ChipDB *chipdb = read_chipdb(chipdb_file_s);
  
  *logs << "  supported packages: ";
  bool first = true;
  for (const auto &p : chipdb->packages)
    {
      if (first)
	first = false;
      else
	*logs << ", ";
      *logs << p.first;
    }
  *logs << "\n";
  
  // chipdb->dump(std::cout);
  
  auto package_i = chipdb->packages.find(package_name);
  if (package_i == chipdb->packages.end())
    fatal(fmt("unknown package `" << package_name << "'"));
  const Package &package = package_i->second;
  
  Design *d;
  if (input_file)
    {
      *logs << "read_blif " << input_file << "...\n";
      d = read_blif(input_file);
    }
  else
    {
      *logs << "read_blif <stdin>...\n";
      d = read_blif("<stdin>", std::cin);
    }
  // d->dump();
  
  *logs << "prune...\n";
  d->prune();
#ifndef NDEBUG
  d->check();
#endif
  // d->dump();
  
  {
    Models models(d);
    Configuration conf;
    
    std::unordered_map<Instance *, Location, HashId> placement;
    if (route_only)
      {
	Model *top = d->top();
	for (Instance *inst : top->instances())
	  {
	    const std::string &loc_attr = inst->get_attr("loc").as_string();
	    int t, pos;
	    sscanf(loc_attr.c_str(), "%d,%d", &t, &pos);
	    Location loc(chipdb->tile_x(t),
			 chipdb->tile_y(t),
			 pos);
	    extend(placement, inst, loc);
	  }
      }
    else
      {
	Constraints constraints;
	if (pcf_file)
	  {
	    *logs << "read_pcf " << pcf_file << "...\n";
	    read_pcf(pcf_file, d, constraints);
	  }
	
	*logs << "instantiate_io...\n";
	instantiate_io(d);
#ifndef NDEBUG
	d->check();
#endif
	// d->dump();
    
	CarryChains chains;
    
	*logs << "pack...\n";
	pack(chipdb, package, d, chains);
#ifndef NDEBUG
	d->check();
#endif
	// d->dump();
    
	if (pack_blif)
	  {
	    *logs << "write_blif " << pack_blif << "\n";
	    std::string expanded = expand_filename(pack_blif);
	    std::ofstream fs(expanded);
	    if (fs.fail())
	      fatal(fmt("write_blif: failed to open `" << expanded << "': "
			<< strerror(errno)));
	    d->write_blif(fs);
	  }
	if (pack_verilog)
	  {
	    *logs << "write_verilog " << pack_verilog << "\n";
	    std::string expanded = expand_filename(pack_verilog);
	    std::ofstream fs(expanded);
	    if (fs.fail())
	      fatal(fmt("write_verilog: failed to open `" << expanded << "': "
			<< strerror(errno)));
	    d->write_verilog(fs);
	  }
    
	*logs << "promote_globals...\n";
	std::unordered_map<Instance *, uint8_t, HashId> gb_inst_gc
	  = promote_globals(chipdb, d, do_promote_globals);
#ifndef NDEBUG
	d->check();
#endif
	// d->dump();
    
	*logs << "realize_constants...\n";
	realize_constants(chipdb, d);
#ifndef NDEBUG
	d->check();
#endif
    
	*logs << "place...\n";
	// d->dump();
	placement = place(rg, chipdb, package, d,
			  chains, constraints, gb_inst_gc,
			  conf);
#ifndef NDEBUG
	d->check();
#endif
    
	if (post_place_pcf)
	  {
	    *logs << "write_pcf " << post_place_pcf << "...\n";
	    std::string expanded = expand_filename(post_place_pcf);
	    std::ofstream fs(expanded);
	    if (fs.fail())
	      fatal(fmt("write_pcf: failed to open `" << expanded << "': "
			<< strerror(errno)));
	    for (const auto &p : placement)
	      {
		if (models.is_io(p.first))
		  {
		    std::string pin = package.loc_pin.at(p.second);
		    Port *top_port = (p.first
				      ->find_port("PACKAGE_PIN")
				      ->connection_other_port());
		    assert(isa<Model>(top_port->node())
			   && cast<Model>(top_port->node()) == d->top());
		
		    fs << "set_io " << top_port->name() << " " << pin << "\n";
		  }
	      }
	  }
    
	if (place_blif)
	  {
	    for (const auto &p : placement)
	      {
		int t = chipdb->tile(p.second.x(),
				     p.second.y());
		int pos = p.second.pos();
		p.first->set_attr("loc",
				  fmt(t << "," << pos));
	      }
	
	    *logs << "write_blif " << place_blif << "\n";
	    std::string expanded = expand_filename(place_blif);
	    std::ofstream fs(expanded);
	    if (fs.fail())
	      fatal(fmt("write_blif: failed to open `" << expanded << "': "
			<< strerror(errno)));
	    d->write_blif(fs);
	  }
      }
    
    *logs << "route...\n";
    std::vector<Net *> cnet_net = route(chipdb, d, conf, placement);
#ifndef NDEBUG
    d->check();
#endif
    
    if (output_file)
      {
	*logs << "write_txt " << output_file << "...\n";
	std::string expanded = expand_filename(output_file);
	std::ofstream fs(expanded);
	if (fs.fail())
	  fatal(fmt("write_txt: failed to open `" << expanded << "': "
		    << strerror(errno)));
	conf.write_txt(fs, chipdb, d, placement, cnet_net);
      }
    else
      {
	*logs << "write_txt <stdout>...\n";
	conf.write_txt(std::cout, chipdb, d, placement, cnet_net);
      }
  }
  
  delete d;
  delete chipdb;
  
  logs = nullptr;
  if (null_ostream)
    {
      delete null_ostream;
      null_ostream = nullptr;
    }
  
  return 0;
}
