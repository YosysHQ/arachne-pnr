
#include "netlist.hh"
#include "chipdb.hh"
#include "pcf.hh"
#include "carry.hh"
#include "configuration.hh"

class random_generator;

class DesignState
{
public:
  random_generator &rg;
  const ChipDB *chipdb;
  const Package &package;
  Design *d;
  Models models;
  Model *top;
  Constraints constraints;
  CarryChains chains;
  std::set<Instance *, IdLess> locked;
  std::map<Instance *, int, IdLess> placement;
  std::map<Instance *, uint8_t, IdLess> gb_inst_gc;
  std::vector<Net *> cnet_net;
  Configuration conf;
  
public:
  DesignState(random_generator &rg_, const ChipDB *chipdb_, const Package &package_);
  ~DesignState();
  
  bool is_dual_pll(Instance *inst) const;
  std::vector<int> pll_out_io_cells(Instance *inst, int cell) const;
  
  void set_design(Design *d_);
};
