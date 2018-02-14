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

#include "io.hh"
#include "netlist.hh"
#include "casting.hh"

#include <cassert>

void
instantiate_io(Design *d)
{
  Models models(d);
  
  Model *top = d->top();
  Model *io_model = d->find_model("SB_IO");
  Model *tbuf_model = d->find_model("$_TBUF_");

  // Validate all $_TBUF_ outputs
  for (Instance *inst : top->instances())
    {
      if (!models.is_tbuf(inst))
        continue;

      // Look for the $_TBUF_ output (Y)
      Port *p = inst->find_port("Y");
      Net *n = p->connection();
      if (!n)
         fatal("Unconnected $_TBUF_ output");
      // Its output must be connected to only one output port
      // Additionally it could be connected to one or more inputs if the port is inout
      unsigned found = 0;
      for (Port *j : n->connections())
         {
          if (j != p &&
              isa<Model>(j->node()) &&
              (j->direction() == Direction::OUT || j->direction() == Direction::INOUT))
             found++;
         }
      if (!found)
         fatal("$_TBUF_ gate must drive top-level output or inout port");
      if (found != 1)
         fatal("$_TBUF_ gate must drive only one top-level output or inout port");
    }

  // Replace top-level ports using SB_IOs
  for (auto i : top->ports())
    {
      Port *p = i.second;
      Port *q = p->connection_other_port();
      if (q
          && isa<Instance>(q->node())
          && ((models.is_ioX(cast<Instance>(q->node()))
               && q->name() == "PACKAGE_PIN")
              || (models.is_pllX(cast<Instance>(q->node()))
                  && q->name() == "PACKAGEPIN") 
              ||   (models.is_rgba_drv(cast<Instance>(q->node()))
                      && (q->name() == "RGB0" || q->name() == "RGB1" || q->name() == "RGB2")
                  )))
        { // Already connected to an SB_IO or a PLL
        continue;
        }

      // Now we need a net to connect the top-level port to the SB_IO output.
      // The name of this net will be the name of the top-level port.

      // If the net currently used (to connect $_TBUF_'s Y output to the top-level port)
      // is already using this name, we rename the net to NAME$n (n is a number to avoid
      // collisions).
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

      // Connect the new net to the top-level port (disconnecting it from $_TBUF_)
      Net *t = top->add_net(p->name());
      assert(t);
      p->connect(t);
      assert(!matched || t->name() == p->name());

      // Now we create an SB_IO ...
      Instance *io_inst = top->add_instance(io_model);
      // and connect its PACKAGE_PIN output to the new net
      io_inst->find_port("PACKAGE_PIN")->connect(t);
      // The rest of the connection depends on the top-level port direction
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
            // If the top-level port is connected to more than one internal port
            // the connection_other_port will return NULL and we must figure out
            // what's the $_TBUF_ output (Y)
            if (!q && n && n->connections().size() > 1)
              {
               for (auto j : n->connections())
                  {
                   if (j != p && j->name() == "Y" && isa<Instance>(j->node()) &&
                       cast<Instance>(j->node())->instance_of() == tbuf_model)
                     {
                      q = j; // $_TBUF_'s Y output
                      break;
                     }
                  }
              }
            // Analyze the internal port connected to this top-level port
            if (q
                && isa<Instance>(q->node())
                && cast<Instance>(q->node())->instance_of() == tbuf_model
                && q->name() == "Y")
              { // Connected to a $_TBUF_'s Y output
                Instance *tbuf = cast<Instance>(q->node());

                // Connect the $_TBUF_ nets to the SB_IO port
                io_inst->find_port("D_OUT_0")->connect(tbuf->find_port("A")->connection());
                io_inst->find_port("D_IN_0")->connect(tbuf->find_port("Y")->connection());
                io_inst->find_port("OUTPUT_ENABLE")->connect(tbuf->find_port("E")->connection());
                
                io_inst->set_param("PIN_TYPE", BitVector(6, 0x29)); // 101001

                // Remove the $_TBUF_
                tbuf->find_port("A")->disconnect();
                tbuf->find_port("E")->disconnect();
                tbuf->find_port("Y")->disconnect();
                tbuf->remove();
                delete tbuf;
              }
            else
              {
                // This top-level port is an output, not connected to a $_TBUF_
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
