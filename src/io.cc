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

static void
pll_redirect_global(Model *top,
		    Instance *inst,
		    const char *globalname,
		    const char *name)
{
  Port *outglobal = inst->find_port(globalname);
  if (!outglobal)
    return;
  
  Net *globaln = outglobal->connection();
  outglobal->disconnect();
  if (globaln)
    {
      Port *out = inst->find_port(name);
      Net *n = out->connection();
      if (n)
	{
	  for (auto i = globaln->connections().begin();
	       i != globaln->connections().end();)
	    {
	      Port *p = *i;
	      ++i;
	      
	      p->connect(n);
	    }
	  
	  top->remove_net(globaln);
	  delete globaln;
	}
      else
	out->connect(globaln);
    }
}

void
instantiate_io(Design *d)
{
  Model *top = d->top();
  Models models(d);

  for (Instance *inst : top->instances())
    {
      if (!models.is_pllX(inst))
	continue;
      
      pll_redirect_global(top, inst, "PLLOUTGLOBAL", "PLLOUTCORE");
      pll_redirect_global(top, inst, "PLLOUTGLOBALA", "PLLOUTCOREA");
      pll_redirect_global(top, inst, "PLLOUTGLOBALB", "PLLOUTCOREB");
    }
  
  for (auto i : top->ports())
    {
      Port *p = i.second;
      
      Port *q = p->connection_other_port();
      if (q && q->is_package_pin(models))
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
      
      Instance *io_inst = top->add_instance(models.io);
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
	  io_inst->set_param("PIN_TYPE", BitVector(6, 0x19)); // 011001
	}
    }
}
