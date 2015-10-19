/*
 * Copyright (C) 2007-2015 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3 of the
 * License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <Common/Compat.h>

#include "FixedRandomStringGenerator.h"

#include <Common/Logger.h>
#include <Common/Random.h>

#include <cstdlib>

using namespace Hypertable;

namespace {
  const char cb64[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}


FixedRandomStringGenerator::FixedRandomStringGenerator(int n) : m_nchars(n) {
  HT_ASSERT(n>0);
  m_nints = ((m_nchars * 6) + 7) / 8;
  m_ivec.resize(m_nints);
}



void FixedRandomStringGenerator::write(char *buf) {
  size_t indexi=0, indexo=0;
  uint8_t *in = (uint8_t *)&m_ivec[0];

  for (size_t i=0; i<m_nints; i++)
    m_ivec[i] = Random::number32();

  indexi = 0;
  indexo = 0;
  while (indexo < m_nchars) {

    buf[indexo++] = cb64[ in[indexi] >> 2 ];
    if (indexo == m_nchars)
      break;

    buf[indexo++] = cb64[((in[indexi] & 0x03) << 4)
                         | ((in[indexi+1] & 0xf0) >> 4)];
    if (indexo == m_nchars)
      break;

    buf[indexo++] = cb64[((in[indexi+1] & 0x0f) << 2)
                         | ((in[indexi+2] & 0xc0) >> 6)];
    if (indexo == m_nchars)
      break;

    buf[indexo++] = cb64[ in[indexi+2] & 0x3f ];

    indexi += 3;
  }

  buf[indexo] = 0;
}
