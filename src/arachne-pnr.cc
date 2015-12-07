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

#include "pass.hh"
#include "passlist.hh"
#include "netlist.hh"
#include "chipdb.hh"
#include "configuration.hh"
#include "casting.hh"
#include "carry.hh"
#include "designstate.hh"
#include "util.hh"

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>

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
    << "    -B <file>, --post-pack-blif <file>\n"
    << "        Write post-pack netlist to <file> as BLIF.\n"
    << "\n"
    << "    -c <file>, --chipdb <chipdb-file>\n"
    << "        Read chip database from <chipdb-file>.\n"
    << "        Default: +/share/arachne-pnr/chipdb-<device>.bin\n"
    << "\n"
    << "    -d <device>, --device <device>\n"
    << "        Target device <device>.  Supported devices:\n"
    << "          1k - Lattice Semiconductor iCE40LP/HX1K\n"
    << "          8k - Lattice Semiconductor iCE40LP/HX8K\n"
    << "        Default: 1k\n"
    << "\n"
    << "    -e <passlist-file>\n"
    << "        Execute <passlist-file> instead of the standard workflow.\n"
    << "        Options controlling the standard workflow will be ignored.\n"
    << "\n"
    << "    -h, --help\n"
    << "        Print this usage message and exit.\n"
    << "\n"
    << "    -l, --no-promote-globals\n"
    << "        Don't promote nets to globals.\n"
    << "\n"
    << "    -o <output-file>, --output-file <output-file>\n"
    << "        Write output to <output-file>.\n"
    << "\n"
    << "    -P <package>, --package <package>\n"
    << "        Target package <package>.\n"
    << "        Default: tq144 for 1k, ct256 for 8k\n"
    << "\n"
    << "    -p <pcf-file>, --pcf-file <pcf-file>\n"
    << "        Read physical constraints from <pcf-file>.\n"
    << "\n"
    << "    --post-place-blif <file>\n"
    << "        Write post-place netlist to <file> as BLIF.\n"
    << "\n"
    << "    -q, --quiet\n"
    << "        Run quite.  Don't output progress messages.\n"
    << "\n"
    << "    -r\n"
    << "        Randomize seed.\n"
    << "\n"
    << "    --route-only\n"
    << "        Input must include placement.\n"
    << "\n"
    << "    -s <int>, --seed <int>\n"
    << "        Set seed for random generator to <int>.\n"
    << "        Default: 1\n"
    << "\n"
    << "    -t, --list-passes\n"
    << "        Print list of support passes with usage and exit.\n"
    << "\n"
    << "    -V <file>, --post-pack-verilog <file>\n"
    << "        Write post-pack netlist to <file> as Verilog.\n"
    << "\n"
    << "    -v, --version\n"
    << "        Print version and exit.\n"
    << "\n"
    << "    -w <pcf-file>, --write-pcf <pcf-file>\n"
    << "        Write pin assignments to <pcf-file> after placement.\n"
    << "\n"
    << "    --write-binary-chipdb <file>\n"
    << "        Write binary chipdb to <file>.\n";
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
    route_only = false,
    randomize_seed = false;
  std::string device = "1k";
  const char *chipdb_file = nullptr,
    *input_file = nullptr,
    *do_promote_globals = nullptr,
    *package_name_cp = nullptr,
    *pcf_file = nullptr,
    *post_place_pcf = nullptr,
    *pack_blif = nullptr,
    *pack_verilog = nullptr,
    *place_blif = nullptr,
    *output_file = nullptr,
    *seed_str = nullptr,
    *binary_chipdb = nullptr,
    *passlist_file = nullptr;
  
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
          else if (!strcmp(argv[i], "-e"))
            {
              if (i + 1 >= argc)
                fatal(fmt(argv[i] << ": expected argument"));
              
              ++i;
              passlist_file = argv[i];
            }
          else if (!strcmp(argv[i], "--write-binary-chipdb"))
            {
              if (i + 1 >= argc)
                fatal(fmt(argv[i] << ": expected argument"));
              
              ++i;
              binary_chipdb = argv[i];
            }
          else if (!strcmp(argv[i], "-l")
                   || !strcmp(argv[i], "--no-promote-globals"))
            do_promote_globals = argv[i];
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
          else if (!strcmp(argv[i], "-r"))
            randomize_seed = true;
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
          else if (!strcmp(argv[i], "-t")
                   || !strcmp(argv[i], "--list-passes"))
            {
              std::cout << "Supported passes:\n\n";
              Pass::print_passes();
              exit(EXIT_SUCCESS);
            }
          else if (!strcmp(argv[i], "-o")
                   || !strcmp(argv[i], "--output-file"))
            {
              if (i + 1 >= argc)
                fatal(fmt(argv[i] << ": expected argument"));
              
              ++i;
              output_file = argv[i];
            }
          else if (!strcmp(argv[i], "-v")
                   || !strcmp(argv[i], "--version"))
            {
              std::cout << version_str << "\n";
              exit(EXIT_SUCCESS);
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
  
  // defaults
  if (!input_file)
    input_file = "-";
  if (!output_file)
    output_file = "-";
  
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
  
  unsigned seed = 0;
  if (seed_str)
    {
      std::string seed_s = seed_str;
      
      if (seed_s.empty())
        fatal("invalid empty seed");
      
      for (char ch : seed_s)
        {
          if (ch >= '0'
              && ch <= '9')
            seed = seed * 10 + (unsigned)(ch - '0');
          else
            fatal(fmt("invalid character `" 
                      << ch
                      << "' in unsigned integer literal in seed"));
        }
    }
  else
    seed = 1;
  
  if (randomize_seed)
    {
      std::random_device rd;
      do {
        seed = rd();
      } while (seed == 0);
    }
  
  *logs << "seed: " << seed << "\n";
  if (!seed)
    fatal("zero seed");
  
  random_generator rg(seed);
  
  *logs << "device: " << device << "\n";
  std::string chipdb_file_s;
  if (chipdb_file)
    chipdb_file_s = chipdb_file;
  else
    chipdb_file_s = (std::string("+/share/arachne-pnr/chipdb-")
                     + device 
                     + ".bin");
  *logs << "read_chipdb " << chipdb_file_s << "...\n";
  const ChipDB *chipdb = read_chipdb(chipdb_file_s);
  
  if (binary_chipdb)
    {
      *logs << "write_binary_chipdb " << binary_chipdb << "\n";
      
      std::string expanded = expand_filename(binary_chipdb);
      std::ofstream ofs(expanded);
      if (ofs.fail())
        fatal(fmt("write_binary_chidpb: failed to open `" << expanded << "': "
                  << strerror(errno)));
      obstream obs(ofs);
      chipdb->bwrite(obs);
      
      // clean up
      if (chipdb)
        delete chipdb;
      
      logs = nullptr;
      if (null_ostream)
        {
          delete null_ostream;
          null_ostream = nullptr;
        }
      
      return 0;
    }
  
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
  
#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_INIT();
#endif
  
  /*
  while (__AFL_LOOP(1000)) {
  */
  
  {
    DesignState ds(rg, chipdb, package);
    
    if (passlist_file)
      {
        PassList passlist(passlist_file);
        passlist.run(ds);
      }
    else
      {      
        Pass::run(ds, "read_blif", {input_file});
    
        if (route_only)
          {
            for (Instance *inst : ds.top->instances())
              {
                const std::string &loc_attr = inst->get_attr("loc").as_string();
                int cell;
                if (sscanf(loc_attr.c_str(), "%d", &cell) != 1)
                  fatal("parse error in loc attribute");
                extend(ds.placement, inst, cell);
              }
          }
        else
          {
            if (pcf_file)
              Pass::run(ds, "read_pcf", {pcf_file});
        
            Pass::run(ds, "instantiate_io");
            Pass::run(ds, "pack");
        
            if (pack_blif)
              Pass::run(ds, "write_blif", {pack_blif});
            if (pack_verilog)
              Pass::run(ds, "write_verilog", {pack_verilog});
        
            Pass::run(ds, "place_constraints");
        
            if (do_promote_globals)
              Pass::run(ds, "promote_globals", {do_promote_globals});
            else
              Pass::run(ds, "promote_globals");
        
            Pass::run(ds, "realize_constants");
            Pass::run(ds, "place");
        
            if (post_place_pcf)
              Pass::run(ds, "write_pcf", {post_place_pcf});
        
            if (place_blif)
              {
                for (const auto &p : ds.placement)
                  {
                    // p.first->set_attr("loc", fmt(p.second));
                    const Location &loc = chipdb->cell_location[p.second];
                    int t = loc.tile();
                    int pos = loc.pos();
                    p.first->set_attr("loc",
                                      fmt(chipdb->tile_x(t)
                                          << "," << chipdb->tile_y(t)
                                          << "/" << pos));
                  }
            
                Pass::run(ds, "write_blif", {place_blif});
              }
          }
    
        Pass::run(ds, "route");
    
        Pass::run(ds, "write_conf", {output_file});
      }
  }
  
  /*
  }
  */
  
  logs = nullptr;
  if (null_ostream)
    {
      delete null_ostream;
      null_ostream = nullptr;
    }
  
  return 0;
}
