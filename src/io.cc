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
#include "io.hh"
#include "casting.hh"

#include <cassert>

void
instantiate_io(Design *d)
{
  Model *top = d->top();
  Model *io_model = d->find_model("SB_IO");
  
  for (auto i : top->ports())
    {
      Port *p = i.second;
      
      Port *q = p->connection_other_port();
      if (q
	  && isa<Instance>(q->node())
	  && cast<Instance>(q->node())->instance_of() == io_model
	  && q->name() == "PACKAGE_PIN")
	continue;
      
      assert(!p->is_bidir());
      
#ifndef NDEBUG
      bool matched = false;
#endif
      Net *n = p->connection();
      if (n
	  && n->name() == p->name())
	{
#ifndef NDEBUG
	  matched = true;
#endif
	  top->rename_net(n, n->name());
	}
      
      Net *t = top->add_net(p->name());
      p->connect(t);
      assert(!matched || t->name() == p->name());
      
      Instance *io_inst = top->add_instance(io_model);
      io_inst->find_port("PACKAGE_PIN")->connect(t);
      if (p->direction() == Direction::IN)
	{
	  if (t)
	    io_inst->find_port("D_IN_0")->connect(n);
	  io_inst->set_param("PIN_TYPE", BitVector(6, 1)); // 000001
	}
      else
	{
	  assert(p->direction() == Direction::OUT);
	  if (t)
	    io_inst->find_port("D_OUT_0")->connect(n);
	  io_inst->set_param("PIN_TYPE", BitVector(6, 0x18)); // 011000
	}
    }
}
