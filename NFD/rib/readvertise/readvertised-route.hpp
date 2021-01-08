/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2017,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NFD_RIB_READVERTISE_READVERTISED_ROUTE_HPP
#define NFD_RIB_READVERTISE_READVERTISED_ROUTE_HPP

#include "core/scheduler.hpp"
#include <ndn-cxx/security/signing-info.hpp>

namespace nfd {
namespace rib {

/** \brief state of a readvertised route
 */
class ReadvertisedRoute : noncopyable
{
public:
  explicit
  ReadvertisedRoute(const Name& prefix);

public:
  Name prefix; ///< readvertised prefix
  mutable ndn::security::SigningInfo signer; ///< signer for commands
  mutable size_t nRibRoutes; ///< number of RIB routes that cause the readvertisement
  mutable time::milliseconds retryDelay; ///< retry interval (not used for refresh)
  mutable scheduler::ScopedEventId retryEvt; ///< retry or refresh event
};

inline bool
operator<(const ReadvertisedRoute& lhs, const ReadvertisedRoute& rhs)
{
  return lhs.prefix < rhs.prefix;
}

using ReadvertisedRouteContainer = std::set<ReadvertisedRoute>;

} // namespace rib
} // namespace nfd

#endif // NFD_RIB_READVERTISE_READVERTISED_ROUTE_HPP
