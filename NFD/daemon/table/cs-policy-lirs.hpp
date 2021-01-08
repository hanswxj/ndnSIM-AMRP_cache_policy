/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2016,  Regents of the University of California,
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

#ifndef NFD_DAEMON_TABLE_CS_POLICY_LIRS_HPP
#define NFD_DAEMON_TABLE_CS_POLICY_LIRS_HPP

#include "cs-policy.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace nfd {
namespace cs {
namespace lirs {

// struct EntryItComparator
// {
//   bool
//   operator()(const iterator& a, const iterator& b) const
//   {
//     return *a < *b;
//   }
// };

// typedef boost::multi_index_container<
//     std::pair<EntryInfo*, iterator>,
//     boost::multi_index::indexed_by<
//       boost::multi_index::sequenced<>,
//       boost::multi_index::ordered_unique<
//         boost::multi_index::identity<iterator>, EntryItComparator
//       >
//     >
//   > Queue;

#define NODEBUG
#define LRUStackSLocation int
#define LRUStackLocation  int
#define LRUListQLocation  int
#define InvalidLocation -1
#define BottomLocation 0

class EntryInfo {
public:
	enum EntryState {
		kLIR = 0,
		kresidentHIR,
		knonResidentHIR,
		kInvalid
	};

	EntryInfo(Name name, EntryState state) :
		m_name(name),
		m_state(state)
	{}

	void setState(EntryState state){
		m_state = state;
	}

	EntryState getState(){
		return m_state;
	}

	const Name& getName(){
		return m_name;
	}

  std::string returnStateStr(EntryState state)
  {
    switch(state)
    {
      case kLIR:
        return "LIR";
      case kresidentHIR:
        return "resident HIR";
      case knonResidentHIR:
        return "no resident HIR";
      default:
        return "Invalid";
    }
  }

private:
	Name m_name;
	EntryState m_state;
};

typedef std::pair<std::shared_ptr<EntryInfo>, iterator> EntryPair;

class LRUStack
{
public:
  void movToTop(LRUStackLocation location, iterator i)
  {
    if(location == container_.size() - 1)
      return;
    auto it = container_.begin() + location;
    EntryPair tmp = container_[location];
    container_.erase(it);
    container_.push_back({tmp.first, i});
	}

  void debugToString(std::string const& name);
  
  LRUStackLocation find(const Name &name)
  {
    LRUStackLocation location = InvalidLocation;
    int step = 0;
    for(auto it : container_)
    {
      if (it.first->getName() == name)
      {
        location = step;
        break;
      }
      else
        ++step;
    }
    return location;
  }

  void pushEntry(EntryPair item){
		container_.push_back(item);
  }

  void eraseBottomEntry(){
		container_.erase(container_.begin());
  }

  void eraseEntryByLocation(LRUStackLocation location){
    container_.erase(container_.begin() + location);
  }

  void setTopState(EntryInfo::EntryState state){
    setStateByLocation(getContainerSize() - 1 , state);
  }

  void setBottomState(EntryInfo::EntryState state){
    setStateByLocation(BottomLocation , state);
  }

  void setStateByLocation(LRUStackLocation location , EntryInfo::EntryState state){
    (container_[location]).first->setState(state);
  }

  EntryInfo::EntryState getBottomState(){
    return (container_[BottomLocation]).first->getState();
  }

  EntryInfo::EntryState getStateByLocation(LRUStackLocation location){
		return (container_[location]).first->getState();
  }

  EntryPair getBottomEntry(){
		return container_[BottomLocation];
  }

  EntryPair getTopEntry(){
		return container_[getContainerSize() - 1];
  }

  EntryPair getEntryByLocation(LRUStackLocation location){
    return container_[location];
  }

  int getSize(){
		return container_.size();
  }

private:
  typedef std::vector<EntryPair> EntryVec;
  EntryVec container_;
    
  int getContainerSize(){
		return container_.size();
  }
};

class LRUStackS:public LRUStack
{
public:
  void stackPruning()       //栈剪枝，当栈底部的lir条目移至stack首部，则如果底部的条目不是lir条目，必须移除
  {                         //因为栈底的hir和non-hir条目需保证栈底部为lir条目
    while (true)
    {
      EntryInfo::EntryState state = getBottomState();
      assert(state != EntryInfo::kInvalid);
      if(state == EntryInfo::kresidentHIR || state == EntryInfo::knonResidentHIR)
				eraseBottomEntry();
      else
				break;
    }
  }

  void findAndSetState(const Name &name, EntryInfo::EntryState state)
  {
    LRUStackSLocation location = find(name);
    if(location != InvalidLocation)
      setStateByLocation(location , state);
  }
};

class LRUListQ:public LRUStack
{
public:
  void pushToEnd(EntryPair item){
    pushEntry(item);
  }

  void popFront(){
    eraseBottomEntry();
  }

  void movToEnd(LRUListQLocation location, iterator i){
    movToTop(location, i);
  }

  void findAndRemove(const Name &name) 
  {
		LRUListQLocation location = find(name);
		if(location != InvalidLocation)
			eraseEntryByLocation(location);
  }

  EntryPair getAndRemoveFrontEntry()
  {
    EntryPair tmp = getBottomEntry();
    eraseBottomEntry();
    return tmp;
  }
};

/** \brief LRU cs replacement policy
 *
 * The least recently used entries get removed first.
 * Everytime when any entry is used or refreshed, Policy should witness the usage
 * of it.
 */
class LirsPolicy : public Policy
{
public:
  LirsPolicy();

public:
  static const std::string POLICY_NAME;
  void setLimit(size_t nMaxEntries);

private:
  virtual void
  doAfterInsert(iterator i) override;

  virtual void
  doAfterRefresh(iterator i) override;

  virtual void
  doBeforeErase(iterator i) override;

  virtual void
  doBeforeUse(iterator i) override;

private:
	void evictEntries() override;

	void hitHIRInStackS(LRUStackSLocation location, iterator i);

	void addAResidentHIREntry(iterator i);

private:
	int lirSize_;
	int hirSize_;
	LRUStackS stackS_;
	LRUListQ listQ_;
};

} // namespace lirs

using lirs::LirsPolicy;

} // namespace cs
} // namespace nfd

#endif // NFD_DAEMON_TABLE_CS_POLICY_LRU_HPP
