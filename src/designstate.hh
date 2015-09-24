
#include "netlist.hh"
#include "chipdb.hh"
#include "pcf.hh"
#include "carry.hh"
#include "configuration.hh"

class DesignState
{
public:
  const ChipDB *chipdb;
  const Package &package;
  Design *d;
  Model *top;
  Constraints constraints;
  CarryChains chains;
  Models models;
  std::set<Instance *, IdLess> locked;
  std::map<Instance *, int, IdLess> placement;
  std::map<Instance *, uint8_t, IdLess> gb_inst_gc;
  Configuration conf;
  
public:
  DesignState(const ChipDB *chipdb_, const Package &package_, Design *d_);
  
  bool is_dual_pll(Instance *inst) const;
  std::vector<int> pll_out_io_cells(Instance *inst, int cell) const;
};
