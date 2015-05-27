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

#ifndef PNR_CASTING_HH
#define PNR_CASTING_HH

template<class T> bool
isa(const typename T::Base *v)
{
  return v->kind() == T::kindof;
}

template<class T> const T *
cast(const typename T::Base *v)
{
  assert (isa<T>(v));
  return static_cast<const T *>(v);
}

template<class T> T *
cast(typename T::Base *v)
{
  assert (isa<T>(v));
  return static_cast<T *>(v);
}

template<class T> const T *
dyn_cast(const typename T::Base *v)
{
  if (isa<T>(v))
    return static_cast<const T *>(v);
  else
    return 0;
}

template<class T> T *
dyn_cast(typename T::Base *v)
{
  if (isa<T>(v))
    return static_cast<T *>(v);
  else
    return 0;
}

#endif
