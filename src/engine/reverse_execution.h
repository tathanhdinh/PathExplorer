/*
 * Copyright (C) 2013  Ta Thanh Dinh <thanhdinh.ta@inria.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef ENGINE_H
#define ENGINE_H

#include "checkpoint.h"
#include <boost/shared_ptr.hpp>

namespace engine
{

class reverse_execution
{
public:
  static void move_backward(boost::shared_ptr<checkpoint>& past_checkpoint);
  static void move_forward(boost::shared_ptr<checkpoint>& future_checkpoint);
};

} // end of reverse_execution_engine namespace

#endif // ENGINE_H
