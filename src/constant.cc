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

#include "constant.hh"
#include "netlist.hh"
#include "chipdb.hh"

#include <cassert>

void
realize_constants(const ChipDB *chipdb, Design *d)
{
  Models models(d);
  Model *top = d->top();
  
  Net *const0 = nullptr;
  for (const auto &p : top->nets())
    {
      if (p.second->is_constant()
          && p.second->constant() == Value::ZERO)
        {
          const0 = p.second;
          break;
        }
    }
  
  Net *actual_const0 = nullptr,
    *actual_const1 = nullptr;
  
  for (Instance *inst : top->instances())
    {
      for (const auto &p : inst->ports())
        {
          if ((models.is_io(inst)
               && p.second->name() == "PACKAGE_PIN")
              || (models.is_lc(inst)
                  && p.second->name() == "CIN"))
            continue;
          
          Net *n = p.second->connection();
          if (n
              && n->is_constant()
              && n->constant() != p.second->undriven())
            {
              Value v = n->constant();
              
              Net *new_n = nullptr;
              if (v == Value::ZERO)
                {
                  if (!actual_const0)
                    {
                      actual_const0 = top->add_net("$false");
                      
                      Instance *lc_inst = top->add_instance(models.lc);
                      
                      assert(const0);
                      lc_inst->find_port("I0")->connect(const0);
                      lc_inst->find_port("I1")->connect(const0);
                      lc_inst->find_port("I2")->connect(const0);
                      lc_inst->find_port("I3")->connect(const0);
                      lc_inst->find_port("O")->connect(actual_const0);
                      
                      lc_inst->set_param("LUT_INIT", BitVector(1, 0));
                    }
                  new_n = actual_const0;
                }
              else
                {
                  assert(v == Value::ONE);
                  if (!actual_const1)
                    {
                      actual_const1 = top->add_net("$true");
                      
                      Instance *lc_inst = top->add_instance(models.lc);
                      
                      if (!const0)
                        {
                          const0 = top->add_net("$false");
                          const0->set_is_constant(true);
                          const0->set_constant(Value::ZERO);
                        }
                      
                      lc_inst->find_port("I0")->connect(const0);
                      lc_inst->find_port("I1")->connect(const0);
                      lc_inst->find_port("I2")->connect(const0);
                      lc_inst->find_port("I3")->connect(const0);
                      lc_inst->find_port("O")->connect(actual_const1);
                      
                      lc_inst->set_param("LUT_INIT", BitVector(16, 1));
                    }
                  new_n = actual_const1;
                }
              
              p.second->connect(new_n);
              
              if (n->connections().empty())
                {
                  top->remove_net(n);
                  delete n;
                }
            }
        }
    }
  
  if (actual_const0)
    {
      if (actual_const1)
        *logs << "  realized 0, 1\n";
      else
        *logs << "  realized 0\n";
    }
  else if (actual_const1)
        *logs << "  realized 1\n";
}
