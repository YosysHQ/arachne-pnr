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
#include "util.hh"
#include "casting.hh"

#include <cassert>
#include <cstring>
#include <iostream>
#include <iomanip>

static void
write_string_escaped(std::ostream &s, const std::string &str)
{
  s << '"';
  for (char ch : str)
    {
      if (ch == '"'
          || ch == '\\')
        s << '\\' << ch;
      else if (isprint(ch))
        s << ch;
      else if (ch == '\n')
        s << "\n";
      else if (ch == '\t')
        s << "\t";
      else
        s << fmt(std::oct << std::setw(3) << std::setfill('0') << (int)ch);
    }
  s << '"';
}

std::ostream &
operator<<(std::ostream &s, const Const &c)
{
  if (c.m_is_bits)
    {
      for (int i = c.m_bitval.size() - 1; i >= 0; --i)
        s << (c.m_bitval[i] ? '1' : '0');
    }
  else
    write_string_escaped(s, c.m_strval);
  return s;
}

Direction
opposite_direction(Direction d)
{
  switch(d)
    {
    case Direction::IN:
      return Direction::OUT;
    case Direction::OUT:
      return Direction::IN;
    case Direction::INOUT:
      return Direction::INOUT;
    default:
      abort();
      return Direction::IN;
    }
}

int Identified::id_counter = 0;

void
Const::write_verilog(std::ostream &s) const
{
  if (m_is_bits)
    {
      s << m_bitval.size()
        << "'b";
      for (int i = m_bitval.size() - 1; i >= 0; --i)
        s << (m_bitval[i] ? '1' : '0');
    }
  else
    write_string_escaped(s, m_strval);
}

void
Net::replace(Net *new_n)
{
  assert(new_n != this);
  
  for (auto i = m_connections.begin();
       i != m_connections.end();)
    {
      Port *p = *i;
      ++i;
      p->connect(new_n);
    }
  assert(m_connections.empty());
}

void
Port::disconnect()
{
  if (m_connection)
    {
      m_connection->m_connections.erase(this);
      m_connection = nullptr;
    }
}

void
Port::connect(Net *n)
{
  if (m_connection)
    disconnect();
  
  assert(!m_connection);
  m_connection = n;
  if (n)
    n->m_connections.insert(this);
}

Port *
Port::connection_other_port() const
{
  Net *n = connection();
  if (!n
      || n->connections().size() != 2)
    return nullptr;
  
  auto i = n->connections().begin();
  if (*i == this)
    ++i;
  return *i;
}

bool
Port::is_output() const
{
  assert(m_node
         && (isa<Model>(m_node)
             || isa<Instance>(m_node)));
  return (isa<Instance>(m_node)
          ? m_dir == Direction::OUT
          : m_dir == Direction::IN); // model
}

bool
Port::is_input() const
{
  assert(m_node
         && (isa<Model>(m_node)
             || isa<Instance>(m_node)));
  return (isa<Instance>(m_node)
          ? m_dir == Direction::IN
          : m_dir == Direction::OUT); // model
}

Node::~Node()
{
  for (Port *p : m_ordered_ports)
    {
      p->disconnect();
      delete p;
    }
  m_ports.clear();
  m_ordered_ports.clear();
}

Port *
Node::add_port(Port *t)
{
  Port *new_port = new Port(this, t->name(), t->direction(), t->undriven());
  extend(m_ports, new_port->name(), new_port);
  m_ordered_ports.push_back(new_port);
  return new_port;
}

Port *
Node::add_port(const std::string &n, Direction dir)
{
  Port *new_port = new Port(this, n, dir);
  extend(m_ports, new_port->name(), new_port);
  m_ordered_ports.push_back(new_port);
  return new_port;
}

Port *
Node::add_port(const std::string &n, Direction dir, Value u)
{
  Port *new_port = new Port(this, n, dir, u);
  extend(m_ports, new_port->name(), new_port);
  m_ordered_ports.push_back(new_port);
  return new_port;
}

Port *
Node::find_port(const std::string &n)
{
  return lookup_or_default(m_ports, n, nullptr);
}

Instance::Instance(Model *parent_, Model *inst_of)
  : Node(Node::Kind::instance),
    m_parent(parent_),
    m_instance_of(inst_of)
{
  for (Port *p : m_instance_of->m_ordered_ports)
    add_port(p);
}

void
Instance::merge_attrs(const Instance *inst)
{
  auto i = inst->m_attrs.find("src");
  if (i != inst->m_attrs.end())
    {
      auto j = m_attrs.find("src");
      if (j != m_attrs.end())
        j->second = Const(j->second.as_string() + "|" + i->second.as_string());
      else
        m_attrs.insert(*i);
    }
}

bool
Instance::has_param(const std::string &pn) const
{ 
  return (contains_key(m_params, pn)
          || m_instance_of->has_param(pn));
}

const Const &
Instance::get_param(const std::string &pn) const
{
  auto i = m_params.find(pn);
  if (i == m_params.end())
    return m_instance_of->get_param(pn);  // default
  else
    return i->second;
}

void
Instance::remove()
{
  m_parent->m_instances.erase(this);
  for (const auto &p : ports())
    p.second->disconnect();
}

void
Instance::write_blif(std::ostream &s,
                     const std::map<Net *, std::string, IdLess> &net_name) const
{
  s << ".gate " << m_instance_of->name();
  for (Port *p : m_ordered_ports)
    {
      s << " " << p->name() << "=";
      if (p->connected())
        s << net_name.at(p->connection());
    }
  s << "\n";
  
  for (const auto &p : m_attrs)
    s << ".attr " << p.first << " " << p.second << "\n";
  for (const auto &p : m_params)
    s << ".param " << p.first << " " << p.second << "\n";
}

void
Instance::dump() const
{
  *logs << ".gate " << m_instance_of->name();
  for (Port *p : m_ordered_ports)
    {
      *logs << " " << p->name() << "=";
      if (p->connected())
        *logs << p->connection()->name();
    }
  *logs << " # " << this << "\n";
  
  for (const auto &p : m_attrs)
    *logs << ".attr " << p.first << " " << p.second << "\n";
  for (const auto &p : m_params)
    *logs << ".param " << p.first << " " << p.second << "\n";
}

static void
write_verilog_name(std::ostream &s, const std::string &name)
{
  bool quote = false;
  for (char ch : name)
    {
      if (! (isalnum(ch)
             || ch == '_'
             || ch == '$'))
        {
          quote = true;
          break;
        }
    }
  if (quote)
    s << '\\';
  s << name;
  if (quote)
    s << ' ';
}

void
Instance::write_verilog(std::ostream &s,
                        const std::map<Net *, std::string, IdLess> &net_name,
                        const std::string &inst_name) const
{
  if (!m_attrs.empty())
    {
      s << "  (* ";
      bool first = true;
      for (const auto &p : m_attrs)
        {
          if (first)
            first = false;
          else
            s << ", ";
          s << p.first << "=";
          p.second.write_verilog(s);
        }
      s << " *)\n";
    }
  
  s << "  ";
  write_verilog_name(s, m_instance_of->name());
  
  if (!m_params.empty())
    {
      s << " #(";
      bool first = true;
      for (const auto &p : m_params)
        {
          if (first)
            first = false;
          else
            s << ", ";
              
          s << "\n    .";
          write_verilog_name(s, p.first);
          s << "(";
          p.second.write_verilog(s);
          s << ")";
        }
      s << "\n  ) ";
    }
  
  write_verilog_name(s, inst_name);
  s << " (";
  bool first = true;
  for (Port *p : m_ordered_ports)
    {
      Net *conn = p->connection();
      if (conn)
        {
          if (first)
            first = false;
          else
            s << ",";
          s << "\n    .";
          write_verilog_name(s, p->name());
          s << "(";
          write_verilog_name(s, conn->name());
          s << ")";
        }
    }
  s << "\n  );\n";
}

int Model::counter = 0;

Model::Model(Design *d, const std::string &n)
  : Node(Node::Kind::model),
    m_name(n)
{
  if (contains(d->m_models, n)) {
    std::ostringstream s;
    s << "model name \"" << n << "\" conflicts with another defined model";
    fatal(s.str());
  }
  extend(d->m_models, n, this);
}

Model::~Model()
{
  for (Instance *inst : m_instances)
    delete inst;
  m_instances.clear();
  
  // disconnect ports before deleting nets
  for (Port *p : m_ordered_ports)
    p->disconnect();
  
  for (const auto &p : m_nets)
    delete p.second;
  m_nets.clear();
}

Net *
Model::find_net(const std::string &n)
{
  return lookup_or_default(m_nets, n, (Net *)nullptr);
}

Net *
Model::find_or_add_net(const std::string &n)
{
  assert(!n.empty());
  return lookup_or_create(m_nets, n, [&n]() { return new Net(n); });
}

Net *
Model::add_net()
{
 L:
  std::string net_name = fmt("$temp$" << counter);
  ++counter;
  if (contains_key(m_nets, net_name))
    goto L;
  
  Net *new_n = new Net(net_name);
  extend(m_nets, net_name, new_n);
  return new_n;
}

Net *
Model::add_net(const std::string &orig)
{
  int i = 2;
  std::string net_name = orig;
 L:
  if (contains_key(m_nets, net_name))
    {
      net_name = fmt(orig << "$" << i);
      ++i;
      goto L;
    }
  
  Net *new_n = new Net(net_name);
  extend(m_nets, net_name, new_n);
  return new_n;
}

void
Model::remove_net(Net *n)
{
  assert(n->connections().empty());
  m_nets.erase(n->name());
}

Instance *
Model::add_instance(Model *inst_of)
{
  Instance *new_inst = new Instance(this, inst_of);
  m_instances.insert(new_inst);
  return new_inst;
}

std::set<Net *, IdLess>
Model::boundary_nets(const Design *d) const
{
  Models models(d);
  std::set<Net *, IdLess> bnets;
  for (Port *p : m_ordered_ports)
    {
      Net *n = p->connection();
      if (n)
        {
          Port *q = p->connection_other_port();
          if (q
              && isa<Instance>(q->node())
              && ((models.is_ioX(cast<Instance>(q->node()))
                   && q->name() == "PACKAGE_PIN")
                  || (models.is_pllX(cast<Instance>(q->node()))
                      && q->name() == "PACKAGEPIN")
                  || (models.is_rgba_drv(cast<Instance>(q->node()))
                      && (q->name() == "RGB0" || q->name() == "RGB1" || q->name() == "RGB2")
                  )))
            extend(bnets, n);
        }
    }
  return bnets;
}

std::pair<std::vector<Net *>, std::map<Net *, int, IdLess>>
Model::index_nets() const
{
  int n_nets = 0;
  std::vector<Net *> vnets;
  std::map<Net *, int, IdLess> net_idx;
  vnets.push_back(nullptr);
  ++n_nets;
  for (const auto &p : m_nets)
    {
      Net *n = p.second;
      vnets.push_back(n);
      extend(net_idx, n, n_nets);
      ++n_nets;
    }
  return std::make_pair(vnets, net_idx);
}

std::pair<std::vector<Net *>, std::map<Net *, int, IdLess>>
Model::index_internal_nets(const Design *d) const
{
  std::set<Net *, IdLess> bnets = boundary_nets(d);
  
  std::vector<Net *> vnets;
  std::map<Net *, int, IdLess> net_idx;
  
  int n_nets = 0;
  for (const auto &p : m_nets)
    {
      Net *n = p.second;
      if (contains(bnets, n))
        continue;
      
      vnets.push_back(n);
      extend(net_idx, n, n_nets);
      ++n_nets;
    }
  return std::make_pair(vnets, net_idx);
}

std::pair<BasedVector<Instance *, 1>, std::map<Instance *, int, IdLess>>
Model::index_instances() const
{
  BasedVector<Instance *, 1> gates;
  std::map<Instance *, int, IdLess> gate_idx;
  
  int n_gates = 0;
  for (Instance *inst : m_instances)
    {
      ++n_gates;
      gates.push_back(inst);
      extend(gate_idx, inst, n_gates);
    }
  return std::make_pair(gates, gate_idx);
}

void
Model::prune()
{
  for (auto i = m_nets.begin(); i != m_nets.end();)
    {
      Net *n = i->second;
      auto t = i;
      ++i;
      
      int n_distinct = n->connections().size();
      bool driver = false,
        input = false;
      if (n->is_constant())
        {
          driver = true;
          ++n_distinct;
        }
      for (Port *p : n->connections())
        {
          if (p->is_input()
              || p->is_bidir())
            input = true;
          if (p->is_output()
              || p->is_bidir())
            driver = true;
          if (input && driver)
            break;
        }
      
      if (input && driver && n_distinct > 1)
        continue;
      
      // remove n
      for (auto j = n->connections().begin();
           j != n->connections().end();)
        {
          Port *p = *j;
          ++j;
          p->disconnect();
        }
      m_nets.erase(t);
      delete n;
    }
}

void
Model::rename_net(Net *n, const std::string &new_name)
{
  const std::string &old_name = n->name();
  
  int i = 2;
  std::string net_name = new_name;
 L:
  if (contains(m_nets, net_name)
      || net_name == old_name)
    {
      net_name = fmt(new_name << "$" << i);
      ++i;
      goto L;
    }
  
  m_nets.erase(old_name);
  
  n->m_name = net_name;
  extend(m_nets, net_name, n);
}

#ifndef NDEBUG
void
Model::check(const Design *d) const
{
  Models models(d);
  
  for (Port *p : m_ordered_ports)
    {
      if (p->is_bidir())
        {
          Net *n = p->connection();
          if (n)
            {
              Port *q = p->connection_other_port();
              assert (q
                      && isa<Instance>(q->node())
                      && ((models.is_ioX(cast<Instance>(q->node()))
                           && q->name() == "PACKAGE_PIN")
                          || (models.is_pllX(cast<Instance>(q->node()))
                              && q->name() == "PACKAGEPIN")
                          || (models.is_rgba_drv(cast<Instance>(q->node())) 
                            &&  (q->name() == "RGB0" || q->name() == "RGB1" || q->name() == "RGB2"))));
            }
        }
    }
  
  std::set<Net *, IdLess> bnets = boundary_nets(d);
  
  for (const auto &p : m_nets)
    {
      Net *n = p.second;
      assert(p.first == n->name());
      assert(!n->connections().empty());
      
      if (contains(bnets, n))
        continue;
      
      int n_drivers = 0;
      bool input = false;
      if (n->is_constant())
        ++n_drivers;
      for (Port *p2 : n->connections())
        {
          assert(!p2->is_bidir());
          if (p2->is_input())
            input = true;
          if (p2->is_output())
            ++n_drivers;
        }
      
      assert(n_drivers == 1 && input);
    }
}
#endif

std::pair<std::map<Net *, std::string, IdLess>,
          std::set<Net *, IdLess>>
Model::shared_names() const
{
  std::set<std::string> names;
  std::map<Net *, std::string, IdLess> net_name;
  std::set<Net *, IdLess> is_port;
  for (Port *p : m_ordered_ports)
    {
      Net *n = p->connection();
      extend(names, p->name());
      if (n
          && n->name() == p->name())
        {
          extend(net_name, n, p->name());
          extend(is_port, n);
        }
    }
  for (const auto &p : m_nets)
    {
      if (contains(is_port, p.second))
        continue;
      
      int i = 2;
      std::string shared_net_name = p.first;
    L:
      if (contains(names, shared_net_name))
        {
          shared_net_name = fmt(p.first << "$" << i);
          ++i;
          goto L;
        }
      extend(names, shared_net_name);
      extend(net_name, p.second, shared_net_name);
    }
  return std::make_pair(net_name, is_port);
}

void
Model::write_blif(std::ostream &s) const
{
  s << ".model " << m_name << "\n";
  
  s << ".inputs";
  for (Port *p : m_ordered_ports)
    {
      if (p->direction() == Direction::IN
          || p->direction() == Direction::INOUT)
        s << " " << p->name();
    }
  s << "\n";
  
  s << ".outputs";
  for (Port *p : m_ordered_ports)
    {
      if (p->direction() == Direction::OUT
          || p->direction() == Direction::INOUT)
        s << " " << p->name();
    }
  s << "\n";
  
  std::map<Net *, std::string, IdLess> net_name;
  std::set<Net *, IdLess> is_port;
  std::tie(net_name, is_port) = shared_names();
  
  for (const auto &p : net_name)
    {
      if (p.second != p.first->name())
        s << "# " << p.first->name() << " -> " << p.second << "\n";
    }
  
  for (const auto &p : m_nets)
    {
      if (p.second->is_constant())
        {
          s << ".names " << p.first << "\n";
          if (p.second->constant() == Value::ONE)
            s << "1\n";
          else
            assert(p.second->constant() == Value::ZERO);
        }
    }
  
  for (auto i : m_instances)
    i->write_blif(s, net_name);
  
  for (Port *p : m_ordered_ports)
    {
      Net *n = p->connection();
      if (n
          && n->name() != p->name())
        {
          if (p->is_input())
            s << ".names " << net_name.at(n) << " " << p->name() << "\n";
          else
            {
              assert(p->is_output());
              s << ".names " << p->name() << " " << net_name.at(n) << "\n";
            }
          s << "1 1\n";
        }
    }
  
  s << ".end\n";
}

void
Model::write_verilog(std::ostream &s) const
{
  s << "module ";
  write_verilog_name(s, m_name);
  s << "(";
  bool first = true;
  for (Port *p : m_ordered_ports)
    {
      if (first)
        first = false;
      else
        s << ", ";
      switch(p->direction())
        {
        case Direction::IN:
          s << "input ";
          break;
        case Direction::OUT:
          s << "output ";
          break;
        case Direction::INOUT:
          s << "inout ";
          break;
        }
      write_verilog_name(s, p->name());
    }
  s << ");\n";
  
  std::map<Net *, std::string, IdLess> net_name;
  std::set<Net *, IdLess> is_port;
  std::tie(net_name, is_port) = shared_names();
  
  for (const auto &p : net_name)
    {
      if (p.second != p.first->name())
        s << "  // " << p.first->name() << " -> " << p.second << "\n";
    }
  
  for (const auto &p : m_nets)
    {
      if (contains(is_port, p.second))
        continue;
      
      s << "  wire ";
      write_verilog_name(s, net_name.at(p.second));
      if (p.second->is_constant())
        {
          s << " = ";
          if (p.second->constant() == Value::ONE)
            s << "1";
          else
            {
              assert(p.second->constant() == Value::ZERO);
              s << "0";
            }
        }
      s << ";\n";
    }
  
  for (Port *p : m_ordered_ports)
    {
      Net *n = p->connection();
      if (n
          && n->name() != p->name())
        {
          if (p->is_input())
            {
              s << "  assign ";
              write_verilog_name(s, net_name.at(n));
              s << " = " << p->name() << ";\n";
            }
          else
            {
              assert(p->is_output());
              s << "  assign " << p->name() << " = ";
              write_verilog_name(s, net_name.at(n));
              s << ";\n";
            }
        }
      else
        assert(contains(is_port, n));
    }
  
  int k = 0;
  for (Instance *inst : m_instances)
    {
      inst->write_verilog(s, net_name, fmt("$inst" << k));
      ++k;
    }
  
  s << "endmodule\n";
}

void
Design::set_top(Model *t)
{
  assert(m_top == nullptr);
  m_top = t;
}

Design::Design()
  : m_top(nullptr)
{
}

Design::~Design()
{
  for (const auto &p : m_models)
    delete p.second;
  m_models.clear();
}

void
Design::create_standard_models()
{
  Model *lc = new Model(this, "ICESTORM_LC");
  lc->add_port("I0", Direction::IN, Value::ZERO);
  lc->add_port("I1", Direction::IN, Value::ZERO);
  lc->add_port("I2", Direction::IN, Value::ZERO);
  lc->add_port("I3", Direction::IN, Value::ZERO);
  lc->add_port("CIN", Direction::IN, Value::ZERO);
  lc->add_port("CLK", Direction::IN, Value::ZERO);
  lc->add_port("CEN", Direction::IN, Value::ONE);
  lc->add_port("SR", Direction::IN, Value::ZERO);
  lc->add_port("LO", Direction::OUT);
  lc->add_port("O", Direction::OUT);
  lc->add_port("COUT", Direction::OUT);
  
  lc->set_param("LUT_INIT", BitVector(1, 0));
  lc->set_param("NEG_CLK", BitVector(1, 0));
  lc->set_param("CARRY_ENABLE", BitVector(1, 0));
  lc->set_param("DFF_ENABLE", BitVector(1, 0));
  lc->set_param("SET_NORESET", BitVector(1, 0));
  lc->set_param("SET_ASYNC", BitVector(1, 0));
  lc->set_param("ASYNC_SR", BitVector(1, 0));
  
  Model *io = new Model(this, "SB_IO");
  io->add_port("PACKAGE_PIN", Direction::INOUT);
  io->add_port("LATCH_INPUT_VALUE", Direction::IN, Value::ZERO);
  io->add_port("CLOCK_ENABLE", Direction::IN, Value::ONE);
  io->add_port("INPUT_CLK", Direction::IN, Value::ZERO);
  io->add_port("OUTPUT_CLK", Direction::IN, Value::ZERO);
  io->add_port("OUTPUT_ENABLE", Direction::IN, Value::ZERO);
  io->add_port("D_OUT_0", Direction::IN, Value::ZERO);
  io->add_port("D_OUT_1", Direction::IN, Value::ZERO);
  io->add_port("D_IN_0", Direction::OUT, Value::ZERO);
  io->add_port("D_IN_1", Direction::OUT, Value::ZERO);
  
  io->set_param("PIN_TYPE", BitVector(6, 0)); // 000000
  io->set_param("PULLUP", BitVector(1, 0));  // default NO pullup
  io->set_param("NEG_TRIGGER", BitVector(1, 0));
  io->set_param("IO_STANDARD", "SB_LVCMOS");
  
  Model *gb = new Model(this, "SB_GB");
  gb->add_port("USER_SIGNAL_TO_GLOBAL_BUFFER", Direction::IN);
  gb->add_port("GLOBAL_BUFFER_OUTPUT", Direction::OUT);
  
  Model *gb_io = new Model(this, "SB_GB_IO");
  gb_io->add_port("PACKAGE_PIN", Direction::INOUT);
  gb_io->add_port("GLOBAL_BUFFER_OUTPUT", Direction::OUT);
  gb_io->add_port("LATCH_INPUT_VALUE", Direction::IN, Value::ZERO);
  gb_io->add_port("CLOCK_ENABLE", Direction::IN, Value::ONE);
  gb_io->add_port("INPUT_CLK", Direction::IN, Value::ZERO);
  gb_io->add_port("OUTPUT_CLK", Direction::IN, Value::ZERO);
  gb_io->add_port("OUTPUT_ENABLE", Direction::IN, Value::ZERO);
  gb_io->add_port("D_OUT_0", Direction::IN, Value::ZERO);
  gb_io->add_port("D_OUT_1", Direction::IN, Value::ZERO);
  gb_io->add_port("D_IN_0", Direction::OUT, Value::ZERO);
  gb_io->add_port("D_IN_1", Direction::OUT, Value::ZERO);
  
  gb_io->set_param("PIN_TYPE", BitVector(6, 0)); // 000000
  gb_io->set_param("PULLUP", BitVector(1, 0));  // default NO pullup
  gb_io->set_param("NEG_TRIGGER", BitVector(1, 0));
  gb_io->set_param("IO_STANDARD", "SB_LVCMOS");
  
  Model *io_i3c = new Model(this, "SB_IO_I3C");
  io_i3c->add_port("PACKAGE_PIN", Direction::INOUT);
  io_i3c->add_port("LATCH_INPUT_VALUE", Direction::IN, Value::ZERO);
  io_i3c->add_port("CLOCK_ENABLE", Direction::IN, Value::ONE);
  io_i3c->add_port("INPUT_CLK", Direction::IN, Value::ZERO);
  io_i3c->add_port("OUTPUT_CLK", Direction::IN, Value::ZERO);
  io_i3c->add_port("OUTPUT_ENABLE", Direction::IN, Value::ZERO);
  io_i3c->add_port("D_OUT_0", Direction::IN, Value::ZERO);
  io_i3c->add_port("D_OUT_1", Direction::IN, Value::ZERO);
  io_i3c->add_port("D_IN_0", Direction::OUT, Value::ZERO);
  io_i3c->add_port("D_IN_1", Direction::OUT, Value::ZERO);
  
  io_i3c->add_port("PU_ENB", Direction::IN, Value::ZERO);
  io_i3c->add_port("WEAK_PU_ENB", Direction::IN, Value::ZERO);

  
  io_i3c->set_param("PIN_TYPE", BitVector(6, 0)); // 000000
  io_i3c->set_param("PULLUP", BitVector(1, 0));  // default NO pullup
  io_i3c->set_param("WEAK_PULLUP", BitVector(1, 0));  // default NO pullup
  io_i3c->set_param("NEG_TRIGGER", BitVector(1, 0));
  io_i3c->set_param("IO_STANDARD", "SB_LVCMOS");
  
  // The official SB_IO_OD, with inconsistent net naming
  Model *io_od = new Model(this, "SB_IO_OD");
  io_od->add_port("PACKAGEPIN", Direction::INOUT);
  io_od->add_port("LATCHINPUTVALUE", Direction::IN, Value::ZERO);
  io_od->add_port("CLOCKENABLE", Direction::IN, Value::ONE);
  io_od->add_port("INPUTCLK", Direction::IN, Value::ZERO);
  io_od->add_port("OUTPUTCLK", Direction::IN, Value::ZERO);
  io_od->add_port("OUTPUTENABLE", Direction::IN, Value::ZERO);
  io_od->add_port("DOUT0", Direction::IN, Value::ZERO);
  io_od->add_port("DOUT1", Direction::IN, Value::ZERO);
  io_od->add_port("DIN0", Direction::OUT, Value::ZERO);
  io_od->add_port("DIN1", Direction::OUT, Value::ZERO);
  
  io_od->set_param("PIN_TYPE", BitVector(6, 0)); // 000000
  io_od->set_param("PULLUP", BitVector(1, 0));  // default NO pullup
  io_od->set_param("NEG_TRIGGER", BitVector(1, 0));
  io_od->set_param("IO_STANDARD", "SB_LVCMOS");
  
  // As above, but with normalised net naming to minimise code changes throughout
  // arachne
  Model *io_od_a = new Model(this, "SB_IO_OD_A");
  io_od_a->add_port("PACKAGE_PIN", Direction::INOUT);
  io_od_a->add_port("LATCH_INPUT_VALUE", Direction::IN, Value::ZERO);
  io_od_a->add_port("CLOCK_ENABLE", Direction::IN, Value::ONE);
  io_od_a->add_port("INPUT_CLK", Direction::IN, Value::ZERO);
  io_od_a->add_port("OUTPUT_CLK", Direction::IN, Value::ZERO);
  io_od_a->add_port("OUTPUT_ENABLE", Direction::IN, Value::ZERO);
  io_od_a->add_port("D_OUT_0", Direction::IN, Value::ZERO);
  io_od_a->add_port("D_OUT_1", Direction::IN, Value::ZERO);
  io_od_a->add_port("D_IN_0", Direction::OUT, Value::ZERO);
  io_od_a->add_port("D_IN_1", Direction::OUT, Value::ZERO);
  
  io_od_a->set_param("PIN_TYPE", BitVector(6, 0)); // 000000
  io_od_a->set_param("PULLUP", BitVector(1, 0));  // default NO pullup
  io_od_a->set_param("NEG_TRIGGER", BitVector(1, 0));
  io_od_a->set_param("IO_STANDARD", "SB_LVCMOS");
  
  Model *lut = new Model(this, "SB_LUT4");
  lut->add_port("O", Direction::OUT);
  lut->add_port("I0", Direction::IN, Value::ZERO);
  lut->add_port("I1", Direction::IN, Value::ZERO);
  lut->add_port("I2", Direction::IN, Value::ZERO);
  lut->add_port("I3", Direction::IN, Value::ZERO);
  
  lut->set_param("LUT_INIT", BitVector(1, 0));
  
  Model *carry = new Model(this, "SB_CARRY");
  carry->add_port("CO", Direction::OUT);
  carry->add_port("I0", Direction::IN, Value::ZERO);
  carry->add_port("I1", Direction::IN, Value::ZERO);
  carry->add_port("CI", Direction::IN, Value::ZERO);
  
  for (int neg_clk = 0; neg_clk <= 1; ++neg_clk)
    for (int cen = 0; cen <= 1; ++cen)
      for (int sr = 0; sr <= 4; ++sr)
        {
          std::string name = "SB_DFF";
          if (neg_clk)
            name.push_back('N');
          if (cen)
            name.push_back('E');
          switch(sr)
            {
            case 0:  break;
            case 1:
              name.append("SR");
              break;
            case 2:
              name.append("R");
              break;
            case 3:
              name.append("SS");
              break;
            case 4:
              name.append("S");
              break;
            default:
              abort();
            }
        
          Model *dff = new Model(this, name);
          dff->add_port("Q", Direction::OUT);
          dff->add_port("C", Direction::IN, Value::ZERO);
          if (cen)
            dff->add_port("E", Direction::IN, Value::ONE);
          switch(sr)
            {
            case 0:  break;
            case 1:
            case 2:
              dff->add_port("R", Direction::IN, Value::ZERO);
              break;
            case 3:
            case 4:
              dff->add_port("S", Direction::IN, Value::ZERO);
              break;
            default:
              abort();
            }
          dff->add_port("D", Direction::IN, Value::ZERO);
        }
  
  for (int nr = 0; nr <= 1; ++nr)
    for (int nw = 0; nw <= 1; ++nw)
      {
        std::string name = "SB_RAM40_4K";
        
        if (nr)
          name.append("NR");
        if (nw)
          name.append("NW");
        Model *bram = new Model(this, name);
        
        for (int i = 0; i <= 15; ++i)
          bram->add_port(fmt("RDATA[" << i << "]"), Direction::OUT);
        for (int i = 0; i <= 10; ++i)
          bram->add_port(fmt("RADDR[" << i << "]"), Direction::IN, Value::ZERO);
        
        for (int i = 0; i <= 10; ++i)
          bram->add_port(fmt("WADDR[" << i << "]"), Direction::IN, Value::ZERO);
        for (int i = 0; i <= 15; ++i)
          bram->add_port(fmt("MASK[" << i << "]"), Direction::IN, Value::ZERO);
        for (int i = 0; i <= 15; ++i)
          bram->add_port(fmt("WDATA[" << i << "]"), Direction::IN, Value::ZERO);
        
        bram->add_port("RCLKE", Direction::IN, Value::ONE);
        
        if (nr)
          bram->add_port("RCLKN", Direction::IN, Value::ZERO);
        else
          bram->add_port("RCLK", Direction::IN, Value::ZERO);
        bram->add_port("RE", Direction::IN, Value::ZERO);
        
        bram->add_port("WCLKE", Direction::IN, Value::ONE);
        if (nw)
          bram->add_port("WCLKN", Direction::IN, Value::ZERO);
        else
          bram->add_port("WCLK", Direction::IN, Value::ZERO);
        bram->add_port("WE", Direction::IN, Value::ZERO);
        
        for (int i = 0; i <= 15; ++i)
          bram->set_param(fmt("INIT_" << hexdigit(i, 'A')), BitVector(256, 0));
        bram->set_param("READ_MODE", BitVector(2, 0));
        bram->set_param("WRITE_MODE", BitVector(2, 0));
      }

  Model *pll_core = new Model(this, "SB_PLL40_CORE");
  pll_core->add_port("REFERENCECLK", Direction::IN, Value::ZERO);
  pll_core->add_port("RESETB", Direction::IN, Value::ZERO);
  pll_core->add_port("BYPASS", Direction::IN, Value::ZERO);
  pll_core->add_port("EXTFEEDBACK", Direction::IN, Value::ZERO);
  pll_core->add_port("DYNAMICDELAY[0]", Direction::IN, Value::ZERO);
  pll_core->add_port("DYNAMICDELAY[1]", Direction::IN, Value::ZERO);
  pll_core->add_port("DYNAMICDELAY[2]", Direction::IN, Value::ZERO);
  pll_core->add_port("DYNAMICDELAY[3]", Direction::IN, Value::ZERO);
  pll_core->add_port("DYNAMICDELAY[4]", Direction::IN, Value::ZERO);
  pll_core->add_port("DYNAMICDELAY[5]", Direction::IN, Value::ZERO);
  pll_core->add_port("DYNAMICDELAY[6]", Direction::IN, Value::ZERO);
  pll_core->add_port("DYNAMICDELAY[7]", Direction::IN, Value::ZERO);
  pll_core->add_port("LATCHINPUTVALUE", Direction::IN, Value::ZERO);
  pll_core->add_port("SCLK", Direction::IN, Value::ZERO);
  pll_core->add_port("SDI", Direction::IN, Value::ZERO);
  pll_core->add_port("SDO", Direction::IN, Value::ZERO);
  pll_core->add_port("LOCK", Direction::OUT);
  pll_core->add_port("PLLOUTGLOBAL", Direction::OUT);
  pll_core->add_port("PLLOUTCORE", Direction::OUT);
  
  pll_core->set_param("FEEDBACK_PATH", "SIMPLE");
  pll_core->set_param("DELAY_ADJUSTMENT_MODE_FEEDBACK", "FIXED");
  pll_core->set_param("FDA_FEEDBACK", BitVector(4, 0));
  pll_core->set_param("DELAY_ADJUSTMENT_MODE_RELATIVE", "FIXED");
  pll_core->set_param("FDA_RELATIVE", BitVector(4, 0));
  pll_core->set_param("SHIFTREG_DIV_MODE", BitVector(1, 0));
  pll_core->set_param("PLLOUT_SELECT", "GENCLK");
  pll_core->set_param("DIVR", BitVector(4, 0));
  pll_core->set_param("DIVF", BitVector(7, 0));
  pll_core->set_param("DIVQ", BitVector(3, 0));
  pll_core->set_param("FILTER_RANGE", BitVector(3, 0));
  pll_core->set_param("EXTERNAL_DIVIDE_FACTOR", BitVector(32, 1));
  pll_core->set_param("ENABLE_ICEGATE", BitVector(1, 0));
  
  Model *pll_pad = new Model(this, "SB_PLL40_PAD");
  pll_pad->add_port("PACKAGEPIN", Direction::IN);
  pll_pad->add_port("RESETB", Direction::IN, Value::ZERO);
  pll_pad->add_port("BYPASS", Direction::IN, Value::ZERO);
  pll_pad->add_port("EXTFEEDBACK", Direction::IN, Value::ZERO);
  pll_pad->add_port("DYNAMICDELAY[0]", Direction::IN, Value::ZERO);
  pll_pad->add_port("DYNAMICDELAY[1]", Direction::IN, Value::ZERO);
  pll_pad->add_port("DYNAMICDELAY[2]", Direction::IN, Value::ZERO);
  pll_pad->add_port("DYNAMICDELAY[3]", Direction::IN, Value::ZERO);
  pll_pad->add_port("DYNAMICDELAY[4]", Direction::IN, Value::ZERO);
  pll_pad->add_port("DYNAMICDELAY[5]", Direction::IN, Value::ZERO);
  pll_pad->add_port("DYNAMICDELAY[6]", Direction::IN, Value::ZERO);
  pll_pad->add_port("DYNAMICDELAY[7]", Direction::IN, Value::ZERO);
  pll_pad->add_port("LATCHINPUTVALUE", Direction::IN, Value::ZERO);
  pll_pad->add_port("SCLK", Direction::IN, Value::ZERO);
  pll_pad->add_port("SDI", Direction::IN, Value::ZERO);
  pll_pad->add_port("SDO", Direction::IN, Value::ZERO);
  pll_pad->add_port("LOCK", Direction::OUT);
  pll_pad->add_port("PLLOUTGLOBAL", Direction::OUT);
  pll_pad->add_port("PLLOUTCORE", Direction::OUT);
  
  pll_pad->set_param("FEEDBACK_PATH", "SIMPLE");
  pll_pad->set_param("DELAY_ADJUSTMENT_MODE_FEEDBACK", "FIXED");
  pll_pad->set_param("FDA_FEEDBACK", BitVector(4, 0));
  pll_pad->set_param("DELAY_ADJUSTMENT_MODE_RELATIVE", "FIXED");
  pll_pad->set_param("FDA_RELATIVE", BitVector(4, 0));
  pll_pad->set_param("SHIFTREG_DIV_MODE", BitVector(1, 0));
  pll_pad->set_param("PLLOUT_SELECT", "GENCLK");
  pll_pad->set_param("DIVR", BitVector(4, 0));
  pll_pad->set_param("DIVF", BitVector(7, 0));
  pll_pad->set_param("DIVQ", BitVector(3, 0));
  pll_pad->set_param("FILTER_RANGE", BitVector(3, 0));
  pll_pad->set_param("EXTERNAL_DIVIDE_FACTOR", BitVector(32, 1));
  pll_pad->set_param("ENABLE_ICEGATE", BitVector(1, 0));

  Model *pll_2_pad = new Model(this, "SB_PLL40_2_PAD");
  pll_2_pad->add_port("PACKAGEPIN", Direction::IN);
  pll_2_pad->add_port("RESETB", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("BYPASS", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("EXTFEEDBACK", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("DYNAMICDELAY[0]", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("DYNAMICDELAY[1]", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("DYNAMICDELAY[2]", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("DYNAMICDELAY[3]", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("DYNAMICDELAY[4]", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("DYNAMICDELAY[5]", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("DYNAMICDELAY[6]", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("DYNAMICDELAY[7]", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("LATCHINPUTVALUE", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("SCLK", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("SDI", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("SDO", Direction::IN, Value::ZERO);
  pll_2_pad->add_port("LOCK", Direction::OUT);
  pll_2_pad->add_port("PLLOUTGLOBALA", Direction::OUT);
  pll_2_pad->add_port("PLLOUTCOREA", Direction::OUT);
  pll_2_pad->add_port("PLLOUTGLOBALB", Direction::OUT);
  pll_2_pad->add_port("PLLOUTCOREB", Direction::OUT);
  
  pll_2_pad->set_param("FEEDBACK_PATH", "SIMPLE");
  pll_2_pad->set_param("DELAY_ADJUSTMENT_MODE_FEEDBACK", "FIXED");
  pll_2_pad->set_param("FDA_FEEDBACK", BitVector(4, 0));
  pll_2_pad->set_param("DELAY_ADJUSTMENT_MODE_RELATIVE", "FIXED");
  pll_2_pad->set_param("FDA_RELATIVE", BitVector(4, 0));
  pll_2_pad->set_param("SHIFTREG_DIV_MODE", BitVector(1, 0));
  pll_2_pad->set_param("PLLOUT_SELECT_PORTA", "GENCLK");
  pll_2_pad->set_param("PLLOUT_SELECT_PORTB", "GENCLK");
  pll_2_pad->set_param("DIVR", BitVector(4, 0));
  pll_2_pad->set_param("DIVF", BitVector(7, 0));
  pll_2_pad->set_param("DIVQ", BitVector(3, 0));
  pll_2_pad->set_param("FILTER_RANGE", BitVector(3, 0));
  pll_2_pad->set_param("EXTERNAL_DIVIDE_FACTOR", BitVector(32, 1));
  pll_2_pad->set_param("ENABLE_ICEGATE_PORTA", BitVector(1, 0));
  pll_2_pad->set_param("ENABLE_ICEGATE_PORTB", BitVector(1, 0));

  Model *pll_2f_core = new Model(this, "SB_PLL40_2F_CORE");
  pll_2f_core->add_port("REFERENCECLK", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("RESETB", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("BYPASS", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("EXTFEEDBACK", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("DYNAMICDELAY[0]", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("DYNAMICDELAY[1]", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("DYNAMICDELAY[2]", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("DYNAMICDELAY[3]", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("DYNAMICDELAY[4]", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("DYNAMICDELAY[5]", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("DYNAMICDELAY[6]", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("DYNAMICDELAY[7]", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("LATCHINPUTVALUE", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("SCLK", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("SDI", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("SDO", Direction::IN, Value::ZERO);
  pll_2f_core->add_port("LOCK", Direction::OUT);
  pll_2f_core->add_port("PLLOUTGLOBALA", Direction::OUT);
  pll_2f_core->add_port("PLLOUTCOREA", Direction::OUT);
  pll_2f_core->add_port("PLLOUTGLOBALB", Direction::OUT);
  pll_2f_core->add_port("PLLOUTCOREB", Direction::OUT);
  
  pll_2f_core->set_param("FEEDBACK_PATH", "SIMPLE");
  pll_2f_core->set_param("DELAY_ADJUSTMENT_MODE_FEEDBACK", "FIXED");
  pll_2f_core->set_param("FDA_FEEDBACK", BitVector(4, 0));
  pll_2f_core->set_param("DELAY_ADJUSTMENT_MODE_RELATIVE", "FIXED");
  pll_2f_core->set_param("FDA_RELATIVE", BitVector(4, 0));
  pll_2f_core->set_param("SHIFTREG_DIV_MODE", BitVector(1, 0));
  pll_2f_core->set_param("PLLOUT_SELECT_PORTA", "GENCLK");
  pll_2f_core->set_param("PLLOUT_SELECT_PORTB", "GENCLK");
  pll_2f_core->set_param("DIVR", BitVector(4, 0));
  pll_2f_core->set_param("DIVF", BitVector(7, 0));
  pll_2f_core->set_param("DIVQ", BitVector(3, 0));
  pll_2f_core->set_param("FILTER_RANGE", BitVector(3, 0));
  pll_2f_core->set_param("EXTERNAL_DIVIDE_FACTOR", BitVector(32, 1));
  pll_2f_core->set_param("ENABLE_ICEGATE_PORTA", BitVector(1, 0));
  pll_2f_core->set_param("ENABLE_ICEGATE_PORTB", BitVector(1, 0));

  Model *pll_2f_pad = new Model(this, "SB_PLL40_2F_PAD");
  pll_2f_pad->add_port("PACKAGEPIN", Direction::IN);
  pll_2f_pad->add_port("RESETB", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("BYPASS", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("EXTFEEDBACK", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("DYNAMICDELAY[0]", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("DYNAMICDELAY[1]", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("DYNAMICDELAY[2]", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("DYNAMICDELAY[3]", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("DYNAMICDELAY[4]", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("DYNAMICDELAY[5]", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("DYNAMICDELAY[6]", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("DYNAMICDELAY[7]", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("LATCHINPUTVALUE", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("SCLK", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("SDI", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("SDO", Direction::IN, Value::ZERO);
  pll_2f_pad->add_port("LOCK", Direction::OUT);
  pll_2f_pad->add_port("PLLOUTGLOBALA", Direction::OUT);
  pll_2f_pad->add_port("PLLOUTCOREA", Direction::OUT);
  pll_2f_pad->add_port("PLLOUTGLOBALB", Direction::OUT);
  pll_2f_pad->add_port("PLLOUTCOREB", Direction::OUT);
  
  pll_2f_pad->set_param("FEEDBACK_PATH", "SIMPLE");
  pll_2f_pad->set_param("DELAY_ADJUSTMENT_MODE_FEEDBACK", "FIXED");
  pll_2f_pad->set_param("FDA_FEEDBACK", BitVector(4, 0));
  pll_2f_pad->set_param("DELAY_ADJUSTMENT_MODE_RELATIVE", "FIXED");
  pll_2f_pad->set_param("FDA_RELATIVE", BitVector(4, 0));
  pll_2f_pad->set_param("SHIFTREG_DIV_MODE", BitVector(1, 0));
  pll_2f_pad->set_param("PLLOUT_SELECT_PORTA", "GENCLK");
  pll_2f_pad->set_param("PLLOUT_SELECT_PORTB", "GENCLK");
  pll_2f_pad->set_param("DIVR", BitVector(4, 0));
  pll_2f_pad->set_param("DIVF", BitVector(7, 0));
  pll_2f_pad->set_param("DIVQ", BitVector(3, 0));
  pll_2f_pad->set_param("FILTER_RANGE", BitVector(3, 0));
  pll_2f_pad->set_param("EXTERNAL_DIVIDE_FACTOR", BitVector(32, 1));
  pll_2f_pad->set_param("ENABLE_ICEGATE_PORTA", BitVector(1, 0));
  pll_2f_pad->set_param("ENABLE_ICEGATE_PORTB", BitVector(1, 0));

  Model *warmboot = new Model(this, "SB_WARMBOOT");
  warmboot->add_port("BOOT", Direction::IN, Value::ZERO);
  warmboot->add_port("S1", Direction::IN, Value::ZERO);
  warmboot->add_port("S0", Direction::IN, Value::ZERO);
  
  Model *tbuf = new Model(this, "$_TBUF_");
  tbuf->add_port("A", Direction::IN);
  tbuf->add_port("E", Direction::IN);
  tbuf->add_port("Y", Direction::OUT);

  Model *mac16 = new Model(this, "SB_MAC16");
  mac16->add_port("CLK", Direction::IN);
  mac16->add_port("CE", Direction::IN, Value::ONE);
  for(int i = 0; i < 16; i++) {
    mac16->add_port("C[" + std::to_string(i) + "]", Direction::IN, Value::ZERO);
    mac16->add_port("A[" + std::to_string(i) + "]", Direction::IN, Value::ZERO);
    mac16->add_port("B[" + std::to_string(i) + "]", Direction::IN, Value::ZERO);
    mac16->add_port("D[" + std::to_string(i) + "]", Direction::IN, Value::ZERO);
  } 
  mac16->add_port("AHOLD", Direction::IN, Value::ZERO);
  mac16->add_port("BHOLD", Direction::IN, Value::ZERO);
  mac16->add_port("CHOLD", Direction::IN, Value::ZERO);
  mac16->add_port("DHOLD", Direction::IN, Value::ZERO);
  mac16->add_port("IRSTTOP", Direction::IN, Value::ZERO);
  mac16->add_port("IRSTBOT", Direction::IN, Value::ZERO);
  mac16->add_port("ORSTTOP", Direction::IN, Value::ZERO);
  mac16->add_port("ORSTBOT", Direction::IN, Value::ZERO);
  mac16->add_port("OLOADTOP", Direction::IN, Value::ZERO);
  mac16->add_port("OLOADBOT", Direction::IN, Value::ZERO);
  mac16->add_port("ADDSUBTOP", Direction::IN, Value::ZERO);
  mac16->add_port("ADDSUBBOT", Direction::IN, Value::ZERO);
  mac16->add_port("OHOLDTOP", Direction::IN, Value::ZERO);
  mac16->add_port("OHOLDBOT", Direction::IN, Value::ZERO); 
  mac16->add_port("CI", Direction::IN, Value::ZERO);
  mac16->add_port("ACCUMCI", Direction::IN, Value::ZERO);
  mac16->add_port("SIGNEXTIN", Direction::IN, Value::ZERO);

  for(int i = 0; i < 32; i++)
    mac16->add_port("O[" + std::to_string(i) + "]", Direction::OUT);
  
  
  mac16->add_port("CO", Direction::OUT);
  mac16->add_port("ACCUMCO", Direction::OUT);
  mac16->add_port("SIGNEXTOUT", Direction::OUT);
  
  const std::vector<std::pair<std::string, int> > mac16_params = 
    {{"C_REG", 1}, {"A_REG", 1}, {"B_REG", 1}, {"D_REG", 1},
     {"TOP_8x8_MULT_REG", 1}, {"BOT_8x8_MULT_REG", 1},
     {"PIPELINE_16x16_MULT_REG1", 1}, {"PIPELINE_16x16_MULT_REG2", 1},
     {"TOPOUTPUT_SELECT", 2}, {"TOPADDSUB_LOWERINPUT", 2},
     {"TOPADDSUB_UPPERINPUT", 1}, {"TOPADDSUB_CARRYSELECT", 2},
     {"BOTOUTPUT_SELECT", 2}, {"BOTADDSUB_LOWERINPUT", 2},
     {"BOTADDSUB_UPPERINPUT", 1}, {"BOTADDSUB_CARRYSELECT", 2},
     {"MODE_8x8", 1}, {"A_SIGNED", 1}, {"B_SIGNED", 1}};
  for(auto p : mac16_params)
    mac16->set_param(p.first, BitVector(p.second, 0));
  
  Model *hfosc = new Model(this, "SB_HFOSC");
  hfosc->add_port("CLKHFPU", Direction::IN, Value::ZERO);
  hfosc->add_port("CLKHFEN", Direction::IN, Value::ZERO);
  hfosc->add_port("CLKHF", Direction::OUT);
  hfosc->set_param("CLKHF_DIV", "0b00");
  
  Model *hfosc_trim = new Model(this, "SB_HFOSC_TRIM");
  hfosc_trim->add_port("CLKHFPU", Direction::IN, Value::ZERO);
  hfosc_trim->add_port("CLKHFEN", Direction::IN, Value::ZERO);
  for(int i = 0; i < 10; i++)
    hfosc_trim->add_port("TRIM" + std::to_string(i), Direction::IN, Value::ZERO);
  hfosc_trim->add_port("CLKHF", Direction::OUT);
  hfosc_trim->set_param("CLKHF_DIV", "0b00");
  
  Model *lfosc = new Model(this, "SB_LFOSC");
  lfosc->add_port("CLKLFPU", Direction::IN, Value::ZERO);
  lfosc->add_port("CLKLFEN", Direction::IN, Value::ZERO);
  lfosc->add_port("CLKLF", Direction::OUT);
  
  Model *spram = new Model(this, "SB_SPRAM256KA");
  for(int i = 0; i < 14; i++)
    spram->add_port("ADDRESS[" + std::to_string(i) + "]", Direction::IN, Value::ZERO);
  for(int i = 0; i < 16; i++)
    spram->add_port("DATAIN[" + std::to_string(i) + "]", Direction::IN, Value::ZERO);
  for(int i = 0; i < 4; i++)
    spram->add_port("MASKWREN[" + std::to_string(i) + "]", Direction::IN, Value::ZERO);
  spram->add_port("WREN", Direction::IN, Value::ZERO);
  spram->add_port("CHIPSELECT", Direction::IN, Value::ZERO);
  spram->add_port("CLOCK", Direction::IN);
  spram->add_port("STANDBY", Direction::IN, Value::ZERO);
  spram->add_port("SLEEP", Direction::IN, Value::ZERO);
  spram->add_port("POWEROFF", Direction::IN, Value::ZERO);
  for(int i = 0; i < 16; i++)
    spram->add_port("DATAOUT[" + std::to_string(i) + "]", Direction::OUT);
  
  Model *rgba_drv = new Model(this, "SB_RGBA_DRV");
  rgba_drv->add_port("CURREN", Direction::IN, Value::ZERO);
  rgba_drv->add_port("RGBLEDEN", Direction::IN, Value::ZERO);
  rgba_drv->add_port("RGB0PWM", Direction::IN, Value::ZERO);
  rgba_drv->add_port("RGB1PWM", Direction::IN, Value::ZERO);
  rgba_drv->add_port("RGB2PWM", Direction::IN, Value::ZERO);
  rgba_drv->add_port("RGB0", Direction::OUT);
  rgba_drv->add_port("RGB1", Direction::OUT);
  rgba_drv->add_port("RGB2", Direction::OUT);
  
  rgba_drv->set_param("CURRENT_MODE", "0b0");
  rgba_drv->set_param("RGB0_CURRENT", "0b000000");
  rgba_drv->set_param("RGB1_CURRENT", "0b000000");
  rgba_drv->set_param("RGB2_CURRENT", "0b000000");
  
  Model *i2c = new Model(this, "SB_I2C");
  i2c->add_port("SBCLKI", Direction::IN);
  i2c->add_port("SBRWI", Direction::IN, Value::ZERO);
  i2c->add_port("SBSTBI", Direction::IN, Value::ZERO);
  for(int i = 0; i < 8; i++)
    i2c->add_port("SBADRI" + std::to_string(i), Direction::IN, Value::ZERO);
  for(int i = 0; i < 8; i++)
    i2c->add_port("SBDATI" + std::to_string(i), Direction::IN, Value::ZERO);
  for(int i = 0; i < 8; i++)
    i2c->add_port("SBDATO" + std::to_string(i), Direction::OUT);
  i2c->add_port("SBACKO", Direction::OUT);
  i2c->add_port("I2CIRQ", Direction::OUT);
  i2c->add_port("I2CWKUP", Direction::OUT);
  i2c->add_port("SCLI", Direction::IN);
  i2c->add_port("SCLO", Direction::OUT);
  i2c->add_port("SCLOE", Direction::OUT);
  i2c->add_port("SDAI", Direction::IN);
  i2c->add_port("SDAO", Direction::OUT);
  i2c->add_port("SDAOE", Direction::OUT);
  
  //Default to upper left?
  i2c->set_param("BUS_ADDR74", "0b0001");
  i2c->set_param("0b1111100001", "0b1111100001");
  
  Model *spi = new Model(this, "SB_SPI");
  spi->add_port("SBCLKI", Direction::IN);
  spi->add_port("SBRWI", Direction::IN, Value::ZERO);
  spi->add_port("SBSTBI", Direction::IN, Value::ZERO);
  for(int i = 0; i < 8; i++)
    spi->add_port("SBADRI" + std::to_string(i), Direction::IN, Value::ZERO);
  for(int i = 0; i < 8; i++)
    spi->add_port("SBDATI" + std::to_string(i), Direction::IN, Value::ZERO);
  for(int i = 0; i < 8; i++)
    spi->add_port("SBDATO" + std::to_string(i), Direction::OUT);
  spi->add_port("SBACKO", Direction::OUT);
  spi->add_port("SPIIRQ", Direction::OUT);
  spi->add_port("SPIWKUP", Direction::OUT);
  spi->add_port("MI", Direction::IN);
  spi->add_port("SO", Direction::OUT);
  spi->add_port("SOE", Direction::OUT);
  spi->add_port("SI", Direction::IN);
  spi->add_port("MO", Direction::OUT);
  spi->add_port("MOE", Direction::OUT);
  spi->add_port("SCKI", Direction::IN);
  spi->add_port("SCKO", Direction::OUT);
  spi->add_port("SCKOE", Direction::OUT);
  spi->add_port("SCSNI", Direction::IN);
  for(int i = 0; i < 4; i++)
    spi->add_port("MCSNO" + std::to_string(i), Direction::OUT);
  for(int i = 0; i < 4; i++)
    spi->add_port("MCSNOE" + std::to_string(i), Direction::OUT);

  spi->set_param("BUS_ADDR74", "0b0000");
  
  Model *ledda = new Model(this, "SB_LEDDA_IP");
  ledda->add_port("LEDDCS", Direction::IN, Value::ZERO);
  ledda->add_port("LEDDCLK", Direction::IN);
  for(int i = 7; i >= 0; i--)
    ledda->add_port("LEDDDAT" + std::to_string(i), Direction::IN, Value::ZERO);
  for(int i = 3; i >= 0; i--)
    ledda->add_port("LEDDADDR" + std::to_string(i), Direction::IN, Value::ZERO);  
  ledda->add_port("LEDDDEN", Direction::IN, Value::ZERO);
  ledda->add_port("LEDDEXE", Direction::IN, Value::ZERO);
  ledda->add_port("LEDDRST", Direction::IN, Value::ZERO); //doesn't actually exist, for icecube code compatibility only
  ledda->add_port("PWMOUT0", Direction::OUT);
  ledda->add_port("PWMOUT1", Direction::OUT);
  ledda->add_port("PWMOUT2", Direction::OUT);
  ledda->add_port("LEDDON", Direction::OUT);


}

Model *
Design::find_model(const std::string &n) const
{
  return lookup_or_default(m_models, n, (Model *)nullptr);
}

void
Design::prune()
{
  for (const auto &p : m_models)
    p.second->prune();
}

#ifndef NDEBUG
void
Design::check() const
{
  for (const auto &p : m_models)
    p.second->check(this);
}
#endif

void
Design::write_blif(std::ostream &s) const
{
  assert(m_top);
  m_top->write_blif(s);
}

void
Design::write_verilog(std::ostream &s) const
{
  assert(m_top);
  m_top->write_verilog(s);
}

void
Design::dump() const
{
  write_blif(*logs);
}

Models::Models(const Design *d)
{
  lut4 = d->find_model("SB_LUT4");
  carry = d->find_model("SB_CARRY");
  lc = d->find_model("ICESTORM_LC");
  io = d->find_model("SB_IO");
  gb = d->find_model("SB_GB");
  gb_io = d->find_model("SB_GB_IO");
  io_i3c = d->find_model("SB_IO_I3C");
  io_od = d->find_model("SB_IO_OD_A");

  ram = d->find_model("SB_RAM40_4K");
  ramnr = d->find_model("SB_RAM40_4KNR");
  ramnw = d->find_model("SB_RAM40_4KNW");
  ramnrnw = d->find_model("SB_RAM40_4KNRNW");
  warmboot = d->find_model("SB_WARMBOOT");
  tbuf = d->find_model("$_TBUF_");
}
