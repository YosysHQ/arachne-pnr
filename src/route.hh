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

#ifndef PNR_ROUTE_HH
#define PNR_ROUTE_HH

#include "location.hh"
#include "util.hh"

#include <map>

class Design;
class Instance;
class ChipDB;
class Configuration;

extern std::vector<Net *> route(const ChipDB *chipdb,
				Design *d,
				Configuration &conf,
				const hashmap<Instance *, Location> &placement);

#endif
