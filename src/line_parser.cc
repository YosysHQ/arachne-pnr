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

#include "line_parser.hh"
#include "util.hh"

std::ostream &
operator<<(std::ostream &s, const LexicalPosition &lp)
{
  if (lp.internal)
    s << "<internal>";
  else
    s << lp.file << ":" << lp.line;
  return s;
}

void
LexicalPosition::fatal(const std::string &msg) const
{
  std::cerr << *this << ": fatal error: " << msg << "\n";
  exit(EXIT_FAILURE);
}

void
LexicalPosition::warning(const std::string &msg) const
{
  std::cerr << *this << ": warning: " << msg << "\n";
}

void
LineParser::split_line()
{
  words.clear();
  
  std::string t;
  bool instr = false,
    quote = false;
  
  for (char ch : line)
    {
      if (instr)
        {
          t.push_back(ch);
          if (quote)
            quote = false;
          else if (ch == '\\')
            quote = true;
          else if (ch == '"')
            {
              words.push_back(unescape(t));
              t.clear();
              instr = false;
            }
        }
      else if (isspace(ch))
        {
          if (!t.empty())
            {
              words.push_back(t);
              t.clear();
            }
        }
      else
        {
          t.push_back(ch);
          if (ch == '"')
            instr = true;
        }
    }
  if (instr)
    fatal("unterminated string constant");
  else if (!t.empty())
    {
      words.push_back(t);
      t.clear();
    }
}

void
LineParser::read_line()
{
  words.clear();
  do {
    line.clear();
    if (s.eof())
      return;
    
    lp.next_line();
    std::getline(s, line);
    
  L:
    std::size_t p = line.find('#');
    if (p != std::string::npos)
      line.resize(p);
    else if (!line.empty()
             && line.back() == '\\')
      {
        if (s.eof())
          fatal("unexpected backslash before eof");
        
        // drop backslash
        line.pop_back();
        
        std::string line2;
        lp.next_line();
        std::getline(s, line2);
        
        line.append(line2);
        goto L;
      }
    
    split_line();
  } while (words.empty());
}
