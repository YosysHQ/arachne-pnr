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
#include "designstate.hh"
#include "util.hh"

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
    << "          384 - Lattice Semiconductor iCE40LP384\n"
    << "           1k - Lattice Semiconductor iCE40LP/HX1K\n"
    << "           5k - Lattice Semiconductor iCE40UP5K\n"
    << "           8k - Lattice Semiconductor iCE40LP/HX8K\n"
    << "        Default: 1k\n"
    << "\n"
    << "    -c <file>, --chipdb <chipdb-file>\n"
    << "        Read chip database from <chipdb-file>.\n"
#ifdef _WIN32
    << "        Default: +/chipdb-<device>.bin\n"
#else
    << "        Default: +/share/arachne-pnr/chipdb-<device>.bin\n"
#endif
    << "\n"
    << "    --write-binary-chipdb <file>\n"
    << "        Write binary chipdb to <file>.\n"
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
    << "        Default: cm49 for 384, tq144 for 1k, ct256 for 8k\n"
    << "\n"
    << "    -r\n"
    << "        Randomize seed.\n"
    << "\n"
    << "    -m <int>, --max-passes <int>\n"
    << "        Maximum number of routing passes.\n"
    << "        Default: 200\n"
    << "\n"
    << "    -s <int>, --seed <int>\n"
    << "        Set seed for random generator to <int>.\n"
    << "        Default: 1\n"
    << "\n"
    << "    -w <pcf-file>, --write-pcf <pcf-file>\n"
    << "        Write pin assignments to <pcf-file> after placement.\n"
    << "\n"
    << "    -o <output-file>, --output-file <output-file>\n"
    << "        Write output to <output-file>.\n"
    << "\n"
    << "    -v, --version\n"
    << "        Print version and exit.\n";
}

struct null_ostream : public std::ostream
{
  null_ostream() : std::ostream(0) {}
};

int
main(int argc, const char **argv)
{
#ifdef __EMSCRIPTEN__
  EM_ASM(
    if (ENVIRONMENT_IS_NODE)
    {
      FS.mkdir('/x');
      FS.mount(NODEFS, { root: '.' }, '/x');
    }
  );
#endif

  program_name = argv[0];

  bool help = false,
    quiet = false,
    do_promote_globals = true,
    route_only = false,
    randomize_seed = false;
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
    *seed_str = nullptr,
    *max_passes_str = nullptr,
    *binary_chipdb = nullptr;

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
          else if (!strcmp(argv[i], "--write-binary-chipdb"))
            {
              if (i + 1 >= argc)
                fatal(fmt(argv[i] << ": expected argument"));

              ++i;
              binary_chipdb = argv[i];
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
          else if (!strcmp(argv[i], "-m")
                   || !strcmp(argv[i], "--max-passes"))
            {
              if (i + 1 >= argc)
                fatal(fmt(argv[i] << ": expected argument"));

              ++i;
              max_passes_str = argv[i];
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

  if (help)
    {
      usage();
      exit(EXIT_SUCCESS);
    }

  if (device != "384"
      && device != "1k"
      && device != "5k"
      && device != "8k")
    fatal(fmt("unknown device: " << device));

  std::string package_name;
  if (package_name_cp)
    package_name = package_name_cp;
  else if (device == "384")
    package_name = "cm49";
  else if (device == "1k")
    package_name = "tq144";
  else if (device == "5k")
    package_name = "sg48";
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

  int max_passes = 0;
  if (max_passes_str)
    {
      std::string max_passes_s = max_passes_str;

      if (max_passes_s.empty())
        fatal("invalid empty max-passes value");

      for (char ch : max_passes_s)
        {
          if (ch >= '0'
              && ch <= '9')
            max_passes = max_passes * 10 + (unsigned)(ch - '0');
          else
            fatal(fmt("invalid character `"
                      << ch
                      << "' in unsigned integer literal in max-passes value"));
        }
    }
  else
    max_passes = 200;

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
#if defined(_WIN32) && defined(MXE_DIR_STRUCTURE)
    chipdb_file_s = (std::string("+/chipdb-")
                     + device
                     + ".bin");
#else
    chipdb_file_s = (std::string("+/share/arachne-pnr/chipdb-")
                     + device
                     + ".bin");
#endif
  *logs << "read_chipdb " << chipdb_file_s << "...\n";
  const ChipDB *chipdb = read_chipdb(chipdb_file_s);

  if (binary_chipdb)
    {
      *logs << "write_binary_chipdb " << binary_chipdb << "\n";

      std::string expanded = expand_filename(binary_chipdb);
      std::ofstream ofs(expanded, std::ofstream::out | std::ofstream::binary);
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
    DesignState ds(chipdb, package, d);

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
          {
            *logs << "read_pcf " << pcf_file << "...\n";
            read_pcf(pcf_file, ds);
          }

        *logs << "instantiate_io...\n";
        instantiate_io(d);
#ifndef NDEBUG
        d->check();
#endif
        // d->dump();

        *logs << "pack...\n";
        pack(ds);
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
            fs << "# " << version_str << "\n";
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
            fs << "/* " << version_str << " */\n";
            d->write_verilog(fs);
          }

        *logs << "place_constraints...\n";
        place_constraints(ds);
#ifndef NDEBUG
        d->check();
#endif

        // d->dump();

        *logs << "promote_globals...\n";
        promote_globals(ds, do_promote_globals);
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
        place(rg, ds);
#ifndef NDEBUG
        d->check();
#endif
        // d->dump();

        if (post_place_pcf)
          {
            *logs << "write_pcf " << post_place_pcf << "...\n";
            std::string expanded = expand_filename(post_place_pcf);
            std::ofstream fs(expanded);
            if (fs.fail())
              fatal(fmt("write_pcf: failed to open `" << expanded << "': "
                        << strerror(errno)));
            fs << "# " << version_str << "\n";
            for (const auto &p : ds.placement)
              {
                if (ds.models.is_io(p.first))
                  {
                    const Location &loc = chipdb->cell_location[p.second];
                    std::string pin = package.loc_pin.at(loc);
                    Port *top_port = (p.first
                                      ->find_port("PACKAGE_PIN")
                                      ->connection_other_port());
                    assert(isa<Model>(top_port->node())
                           && cast<Model>(top_port->node()) == ds.top);

                    fs << "set_io " << top_port->name() << " " << pin << "\n";
                  }
              }
          }

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

            *logs << "write_blif " << place_blif << "\n";
            std::string expanded = expand_filename(place_blif);
            std::ofstream fs(expanded);
            if (fs.fail())
              fatal(fmt("write_blif: failed to open `" << expanded << "': "
                        << strerror(errno)));
            fs << "# " << version_str << "\n";
            d->write_blif(fs);
          }
      }

    // d->dump();

    *logs << "route...\n";
    route(ds, max_passes);
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
        ds.conf.write_txt(fs, chipdb, d, ds.placement, ds.cnet_net);
      }
    else
      {
        *logs << "write_txt <stdout>...\n";
        ds.conf.write_txt(std::cout, chipdb, d, ds.placement, ds.cnet_net);
      }
  }

  if (d)
    delete d;

  /*
  }
  */

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
