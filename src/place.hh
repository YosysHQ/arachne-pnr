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

#ifndef PNR_PLACE_HH
#define PNR_PLACE_HH

#include "util.hh"
#include <ostream>

class Design;
class Instance;
class Configuration;
class ChipDB;
class Package;
class Constraints;
class CarryChains;
class IdLess;
class DesignState;

extern void place(random_generator &rg, DesignState &ds);
extern void place_set(random_generator &rg, DesignState &ds, 
		      const std::map<Instance *, int, IdLess> &new_placement);

#endif
