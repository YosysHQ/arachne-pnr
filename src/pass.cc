
#include "pass.hh"
#include "designstate.hh"
#include "util.hh"

std::map<std::string, Pass *> Pass::passes;

void
Pass::print_passes()
{
  for (const auto &p : passes)
    {
      p.second->usage();
      std::cout << "\n";
    }
}

void
Pass::run(DesignState &ds, std::string pass_name, std::vector<std::string> args)
{
  Pass *pass = passes.at(pass_name);
  if (!pass)
    ::fatal(fmt("unknown pass `" << pass_name << "'"));
  
  *logs << pass_name << "...\n";
  pass->run(ds, args);
#ifndef NDEBUG
  ds.d->check();
#endif
}

Pass::Pass(const std::string &n)
  : m_name(n)
{
  extend(passes, n, this);
}

Pass::Pass(const std::string &n, const std::string &d)
  : m_name(n), m_desc(d)
{
  extend(passes, n, this);
}

Pass::~Pass()
{
}

void
Pass::fatal(const std::string &msg) const
{
  std::cerr << m_name << ": " << msg << "\n";
  exit(EXIT_FAILURE);
}
