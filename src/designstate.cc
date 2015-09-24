
#include "designstate.hh"

DesignState::DesignState(const ChipDB *chipdb_, const Package &package_, Design *d_)
  : chipdb(chipdb_),
    package(package_),
    d(d_),
    top(d_->top()),
    models(d_)
{
}

bool
DesignState::is_dual_pll(Instance *inst) const
{
  assert(models.is_pllX(inst));
  if (inst->instance_of()->name() == "SB_PLL40_2F_CORE"
      || inst->instance_of()->name() == "SB_PLL40_2_PAD"
      || inst->instance_of()->name() == "SB_PLL40_2F_PAD")
    return true;
  else
    {
      assert(inst->instance_of()->name() == "SB_PLL40_PAD"
	     || inst->instance_of()->name() == "SB_PLL40_CORE");
      return false;
    }
}

std::vector<int>
DesignState::pll_out_io_cells(Instance *inst, int cell) const
{
  assert(models.is_pllX(inst)
	 && chipdb->cell_type[cell] == CellType::PLL);
  
  bool dual = is_dual_pll(inst);
  
  const auto &p_a = chipdb->cell_mfvs.at(cell).at("PLLOUT_A");
  Location io_loc_a(p_a.first, std::stoi(p_a.second));
  int io_cell_a = chipdb->loc_cell(io_loc_a);
  
  std::vector<int> r;
  r.push_back(io_cell_a);
  
  if (dual)
    {
      const auto &p_b = chipdb->cell_mfvs.at(cell).at("PLLOUT_B");
      Location io_loc_b(p_b.first, std::stoi(p_b.second));
      int io_cell_b = chipdb->loc_cell(io_loc_b);
      r.push_back(io_cell_b);
    }
  
  return r;
}
