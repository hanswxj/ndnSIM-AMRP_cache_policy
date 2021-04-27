/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018,  Regents of the University of California,
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

#include "cs.hpp"
#include "core/algorithm.hpp"
#include "core/logger.hpp"

#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/util/concepts.hpp>

namespace nfd {
namespace cs {

NDN_CXX_ASSERT_FORWARD_ITERATOR(Cs::const_iterator);

NFD_LOG_INIT("ContentStore");

bool flag = false;
double hitRateRatio = 0;
double missRateRatio = 0;

double hitRateRatio1 = 0;
double hitRateRatio2 = 0;
double hitRateRatio3 = 0;

double missRateRatio1 = 0;
double missRateRatio2 = 0;
double missRateRatio3 = 0;

unique_ptr<Policy>
makeDefaultPolicy()
{
  const std::string DEFAULT_POLICY = "lirs";     //lrfu
  return Policy::create(DEFAULT_POLICY);
}

Cs::Cs(size_t nMaxPackets)
  : m_shouldAdmit(true)
  , m_shouldServe(true)
{
  this->setPolicyImpl(makeDefaultPolicy());
  m_policy->setLimit(nMaxPackets);
}

void
Cs::insert(const Data& data, bool isUnsolicited)
{
  if (!m_shouldAdmit || m_policy->getLimit() == 0) {
    return;
  }
  NFD_LOG_DEBUG("insert " << data.getName());

  // recognize CachePolicy
  shared_ptr<lp::CachePolicyTag> tag = data.getTag<lp::CachePolicyTag>();
  if (tag != nullptr) {
    lp::CachePolicyType policy = tag->get().getPolicy();
    if (policy == lp::CachePolicyType::NO_CACHE) {
      return;
    }
  }

  iterator it;
  bool isNewEntry = false;
  std::tie(it, isNewEntry) = m_table.emplace(data.shared_from_this(), isUnsolicited);
  EntryImpl& entry = const_cast<EntryImpl&>(*it);

  entry.updateStaleTime();

  if (!isNewEntry) { // existing entry
    // XXX This doesn't forbid unsolicited Data from refreshing a solicited entry.
    if (entry.isUnsolicited() && !isUnsolicited) {
      entry.unsetUnsolicited();
    }

    m_policy->afterRefresh(it);
  }
  else {
    m_policy->afterInsert(it);
  }
  NFD_LOG_DEBUG("CS Size: " << m_policy->getCs()->size());
}

void
Cs::erase(const Name& prefix, size_t limit, const AfterEraseCallback& cb)
{
  BOOST_ASSERT(static_cast<bool>(cb));

  iterator first = m_table.lower_bound(prefix);
  iterator last = m_table.end();
  if (prefix.size() > 0) {
    last = m_table.lower_bound(prefix.getSuccessor());
  }

  size_t nErased = 0;
  while (first != last && nErased < limit) {
    m_policy->beforeErase(first);
    first = m_table.erase(first);
    ++nErased;
  }

  if (cb) {
    cb(nErased);
  }
}

void
Cs::find(const Interest& interest,
         const HitCallback& hitCallback,
         const MissCallback& missCallback) const
{
  BOOST_ASSERT(static_cast<bool>(hitCallback));
  BOOST_ASSERT(static_cast<bool>(missCallback));

  if (!m_shouldServe || m_policy->getLimit() == 0) {
    missCallback(interest);
    return;
  }
  const Name& prefix = interest.getName();
  bool isRightmost = interest.getChildSelector() == 1;
  NFD_LOG_DEBUG("find " << prefix << (isRightmost ? " R" : " L"));

  iterator first = m_table.lower_bound(prefix);
  iterator last = m_table.end();
  if (prefix.size() > 0) {
    last = m_table.lower_bound(prefix.getSuccessor());
  }

  iterator match = last;
  if (isRightmost) {
    match = this->findRightmost(interest, first, last);
  }
  else {
    match = this->findLeftmost(interest, first, last);
  }

  std::string prefdata = prefix.toUri().c_str();
  std::string delimiter = "%";
  std::string pref = prefdata.substr(0, prefdata.find(delimiter));

  if (match == last) {

    missRateRatio ++;
    if(pref == "/prefix/") {
      missRateRatio1++;
    }
    else if(pref == "/prefix2/") {
      missRateRatio2++;
    }
    else if(pref == "/prefix3/") {
      missRateRatio3++;
    }             

    // NFD_LOG_DEBUG(" TotalMiss : " << missRateRatio << " hit : "<< hitRateRatio);
    // if((missRateRatio2 != 0)||(missRateRatio3 != 0)) {
    //   NFD_LOG_DEBUG("  Miss P1: " << missRateRatio1<< " Miss P2: " << missRateRatio2<< " Miss P3: " <<missRateRatio3);
    // }        

    if(!flag) {
      if(hitRateRatio + missRateRatio >= 30 * m_policy->getLimit()) {
        hitRateRatio = 0;
        missRateRatio = 0;

        hitRateRatio1 = 0;
        hitRateRatio2 = 0;
        hitRateRatio3 = 0;

        missRateRatio1 = 0;
        missRateRatio2 = 0;
        missRateRatio3 = 0;
        flag = true;
        NFD_LOG_DEBUG("Training data completed, all CS reach the limit!");
      }
    }
    if(flag) {
      NFD_LOG_DEBUG(" TotalMiss : " << missRateRatio << " hit : "<< hitRateRatio);
      if((missRateRatio2 != 0)||(missRateRatio3 != 0)) {
        NFD_LOG_DEBUG("  Miss P1: " << missRateRatio1<< " Miss P2: " << missRateRatio2<< " Miss P3: " <<missRateRatio3);
      }
    }

    // NFD_LOG_DEBUG("  no-match");
    missCallback(interest);
    return;
  }
  
  hitRateRatio ++;
  if(pref == "/prefix/") {
    missRateRatio1++;
  }
  else if(pref == "/prefix2/") {
    missRateRatio2++;
  }
  else if(pref == "/prefix3/") {
    missRateRatio3++;
  }

  // NFD_LOG_DEBUG(" TotalHit : " << hitRateRatio << ", hit rate : " << hitRateRatio / (hitRateRatio + missRateRatio));
  // if((hitRateRatio2 != 0)||(hitRateRatio3 != 0)){
  //   NFD_LOG_DEBUG("  Hit P1: " << hitRateRatio1<< ", hit rate P1 : " << hitRateRatio1 / (hitRateRatio1 + missRateRatio1));
  //   NFD_LOG_DEBUG("  Hit P2: " << hitRateRatio2<< ", hit rate P2 : " << hitRateRatio2 / (hitRateRatio2 + missRateRatio2));
  //   NFD_LOG_DEBUG("  Hit P3: " << hitRateRatio3<< ", hit rate P3 : " << hitRateRatio2 / (hitRateRatio3 + missRateRatio3));
  // }

  if(flag) {
    NFD_LOG_DEBUG(" TotalHit : " << hitRateRatio << ", hit rate : " << hitRateRatio / (hitRateRatio + missRateRatio));
    if((hitRateRatio2 != 0)||(hitRateRatio3 != 0)){
      NFD_LOG_DEBUG("  Hit P1: " << hitRateRatio1<< ", hit rate P1 : " << hitRateRatio1 / (hitRateRatio1 + missRateRatio1));
      NFD_LOG_DEBUG("  Hit P2: " << hitRateRatio2<< ", hit rate P2 : " << hitRateRatio2 / (hitRateRatio2 + missRateRatio2));
      NFD_LOG_DEBUG("  Hit P3: " << hitRateRatio3<< ", hit rate P3 : " << hitRateRatio2 / (hitRateRatio3 + missRateRatio3));
    }
  }
    
  // NFD_LOG_DEBUG("  matching " << match->getName());
  m_policy->beforeUse(match);
  hitCallback(interest, match->getData());
}

iterator
Cs::findLeftmost(const Interest& interest, iterator first, iterator last) const
{
  return std::find_if(first, last, bind(&cs::EntryImpl::canSatisfy, _1, interest));
}

iterator
Cs::findRightmost(const Interest& interest, iterator first, iterator last) const
{
  // Each loop visits a sub-namespace under a prefix one component longer than Interest Name.
  // If there is a match in that sub-namespace, the leftmost match is returned;
  // otherwise, loop continues.

  size_t interestNameLength = interest.getName().size();
  for (iterator right = last; right != first;) {
    iterator prev = std::prev(right);

    // special case: [first,prev] have exact Names
    if (prev->getName().size() == interestNameLength) {
      NFD_LOG_TRACE("  find-among-exact " << prev->getName());
      iterator matchExact = this->findRightmostAmongExact(interest, first, right);
      return matchExact == right ? last : matchExact;
    }

    Name prefix = prev->getName().getPrefix(interestNameLength + 1);
    iterator left = m_table.lower_bound(prefix);

    // normal case: [left,right) are under one-component-longer prefix
    NFD_LOG_TRACE("  find-under-prefix " << prefix);
    iterator match = this->findLeftmost(interest, left, right);
    if (match != right) {
      return match;
    }
    right = left;
  }
  return last;
}

iterator
Cs::findRightmostAmongExact(const Interest& interest, iterator first, iterator last) const
{
  return find_last_if(first, last, bind(&EntryImpl::canSatisfy, _1, interest));
}

void
Cs::dump()
{
  NFD_LOG_DEBUG("dump table");
  for (const EntryImpl& entry : m_table) {
    NFD_LOG_TRACE(entry.getFullName());
  }
}

void
Cs::setPolicy(unique_ptr<Policy> policy)
{
  BOOST_ASSERT(policy != nullptr);
  BOOST_ASSERT(m_policy != nullptr);
  size_t limit = m_policy->getLimit();
  this->setPolicyImpl(std::move(policy));
  m_policy->setLimit(limit);
}

void
Cs::setPolicyImpl(unique_ptr<Policy> policy)
{
  NFD_LOG_DEBUG("set-policy " << policy->getName());
  m_policy = std::move(policy);
  m_beforeEvictConnection = m_policy->beforeEvict.connect([this] (iterator it) {
      m_table.erase(it);
    });

  m_policy->setCs(this);
  BOOST_ASSERT(m_policy->getCs() == this);
}

void
Cs::enableAdmit(bool shouldAdmit)
{
  if (m_shouldAdmit == shouldAdmit) {
    return;
  }
  m_shouldAdmit = shouldAdmit;
  NFD_LOG_INFO((shouldAdmit ? "Enabling" : "Disabling") << " Data admittance");
}

void
Cs::enableServe(bool shouldServe)
{
  if (m_shouldServe == shouldServe) {
    return;
  }
  m_shouldServe = shouldServe;
  NFD_LOG_INFO((shouldServe ? "Enabling" : "Disabling") << " Data serving");
}

} // namespace cs
} // namespace nfd
