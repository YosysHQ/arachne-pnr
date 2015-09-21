
#include "designstate.hh"

DesignState::DesignState(const ChipDB *chipdb_, const Package &package_, Design *d_)
  : chipdb(chipdb_),
    package(package_),
    d(d_),
    top(d_->top()),
    models(d_)
{
}
