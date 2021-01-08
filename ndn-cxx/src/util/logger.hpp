/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2017 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */

#ifndef NDN_UTIL_LOGGER_HPP
#define NDN_UTIL_LOGGER_HPP

#include "ns3/log.h"

namespace ndn {
namespace util {

/** \brief declare a log module
 */
#define NDN_LOG_INIT(name) NS_LOG_COMPONENT_DEFINE("ndn-cxx." BOOST_STRINGIZE(name));

#define NDN_LOG_TRACE(expression) NS_LOG_LOGIC(expression)
#define NDN_LOG_DEBUG(expression) NS_LOG_DEBUG(expression)
#define NDN_LOG_INFO(expression)  NS_LOG_INFO(expression)
#define NDN_LOG_WARN(expression)  NS_LOG_ERROR(expression)
#define NDN_LOG_ERROR(expression) NS_LOG_WARN(expression)
#define NDN_LOG_FATAL(expression) NS_LOG_FATAL(expression)

} // namespace util
} // namespace ndn

#endif // NDN_UTIL_LOGGER_HPP
