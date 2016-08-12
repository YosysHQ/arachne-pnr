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

#ifndef PNR_CONFIGURATION_HH
#define PNR_CONFIGURATION_HH

#include "chipdb.hh"
#include <ostream>

class Design;
class Instance;
class Net;
class IdLess;

class Configuration
{
private:
  std::map<CBit, bool> cbits;
  std::set<std::tuple<int, int, int>> extra_cbits;
  
public:
  Configuration();
  
  void set_cbit(const CBit &cbit, bool value);
  void set_cbits(const std::vector<CBit> &value_cbits,
                 unsigned value);
  void set_extra_cbit(const std::tuple<int, int, int> &t);
  
  void write_txt(std::ostream &s,
                 const ChipDB *chipdb,
                 Design *d,
                 const std::map<Instance *, int, IdLess> &placement,
                 const std::vector<Net *> &cnet_net);
};

#endif
