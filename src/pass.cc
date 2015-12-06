
#include "pass.hh"
#include "designstate.hh"
#include "util.hh"

std::map<std::string, Pass *> Pass::passes;

void
Pass::run(DesignState &ds, std::string pass_name, std::vector<std::string> args)
{
  Pass *pass = passes.at(pass_name);
  if (!pass)
    fatal(fmt("unknown pass `" << pass_name << "'"));
  
  *logs << pass_name << "...\n";
  pass->run(ds, args);
#ifndef NDEBUG
  ds.d->check();
#endif
}

Pass::Pass(const std::string &name_)
  : m_name(name_)
{
  extend(passes, name_, this);
}

Pass::~Pass()
{
}
