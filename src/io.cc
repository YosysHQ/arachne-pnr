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
  Models models(d);
  
  Model *top = d->top();
  Model *io_model = d->find_model("SB_IO");
  Model *tbuf_model = d->find_model("$_TBUF_");
  
  for (Instance *inst : top->instances())
    {
      if (!models.is_tbuf(inst))
        continue;
      
      Port *p = inst->find_port("Y");
      Port *q = p->connection_other_port();
      if (!q
          || !isa<Model>(q->node())
          || (q->direction() != Direction::OUT
              && q->direction() != Direction::INOUT))
        fatal("$_TBUF_ gate must drive top-level output or inout port");
    }
  
  for (auto i : top->ports())
    {
      Port *p = i.second;
      
      Port *q = p->connection_other_port();
      if (q
          && isa<Instance>(q->node())
          && ((models.is_ioX(cast<Instance>(q->node()))
               && q->name() == "PACKAGE_PIN")
              || (models.is_pllX(cast<Instance>(q->node()))
                  && q->name() == "PACKAGEPIN")))
        continue;
      
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
      assert(t);
      p->connect(t);
      assert(!matched || t->name() == p->name());
      
      Instance *io_inst = top->add_instance(io_model);
      io_inst->find_port("PACKAGE_PIN")->connect(t);
      switch(p->direction())
        {
        case Direction::IN:
          {
            io_inst->find_port("D_IN_0")->connect(n);
            io_inst->set_param("PIN_TYPE", BitVector(6, 1)); // 000001
          }
          break;
          
        case Direction::OUT:
        case Direction::INOUT:
          {
            if (q
                && isa<Instance>(q->node())
                && cast<Instance>(q->node())->instance_of() == tbuf_model
                && q->name() == "Y")
              {
                Instance *tbuf = cast<Instance>(q->node());
                
                io_inst->find_port("D_OUT_0")->connect(tbuf->find_port("A")->connection());
                io_inst->find_port("D_IN_0")->connect(tbuf->find_port("Y")->connection());
                io_inst->find_port("OUTPUT_ENABLE")->connect(tbuf->find_port("E")->connection());
                
                io_inst->set_param("PIN_TYPE", BitVector(6, 0x29)); // 101001
                
                tbuf->find_port("A")->disconnect();
                tbuf->find_port("E")->disconnect();
                tbuf->find_port("Y")->disconnect();
                tbuf->remove();
                delete tbuf;
              }
            else
              {
                if (p->direction() == Direction::INOUT)
                  fatal(fmt("bidirectional port `" << p->name()
                            << "' must be driven by tri-state buffer"));
                
                io_inst->find_port("D_OUT_0")->connect(n);
                io_inst->set_param("PIN_TYPE", BitVector(6, 0x19)); // 011001
              }
          }
          break;
          
        default: abort();
        }
    }
  
  d->prune();
}
