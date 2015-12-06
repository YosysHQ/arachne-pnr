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
#include "pass.hh"
#include "passlist.hh"
#include "line_parser.hh"

class PassListParser : public LineParser
{
public:
  PassListParser(const std::string &f, std::istream &s_)
    : LineParser(f, s_)
  {}
  
  PassList *parse();
};

PassList *
PassListParser::parse()
{
  PassList *pl = new PassList;
  for (;;)
    {
      if (eof())
        break;
      
      read_line();
      if (line.empty())
        continue;
      
      Pass *pass = lookup_or_default(Pass::passes, words[0], nullptr);
      if (!pass)
        fatal(fmt("unknown pass `" << words[0] << "'"));
      
      pl->passes.push_back(pass);
      pl->pass_args.push_back(std::vector<std::string>(words.begin() + 1,
                                                       words.end()));
    }
  return pl;
}

void
PassList::run(DesignState &ds) const
{
  for (size_t i = 0; i < passes.size(); ++i)
    passes[i]->run(ds, pass_args[i]);
}
