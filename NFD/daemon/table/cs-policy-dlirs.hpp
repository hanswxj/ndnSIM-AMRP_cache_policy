#ifndef NFD_DAEMON_TABLE_CS_POLICY_DLIRS_HPP
#define NFD_DAEMON_TABLE_CS_POLICY_DLIRS_HPP

#include "cs-policy.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace nfd {
namespace cs {
namespace dlirs {


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
		m_state(state),
    m_isDemoted(false)
	{}

	void setState(EntryState state){
		m_state = state;
	}

  void set_isDemoted(bool flag){
		m_isDemoted = flag;
	}

	EntryState getState(){
		return m_state;
	}

	const Name& getName(){
		return m_name;
	}

  bool get_isDemoted(){
    return m_isDemoted;
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
  bool m_isDemoted = false;
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

  void set_isDemotedByLocation(LRUStackLocation location , bool flag){
    (container_[location]).first->set_isDemoted(flag);
  }

  EntryInfo::EntryState getBottomState(){
    return (container_[BottomLocation]).first->getState();
  }

  EntryInfo::EntryState getStateByLocation(LRUStackLocation location){
		return (container_[location]).first->getState();
  }

  bool get_isDemotedByLocation(LRUStackLocation location){
		return (container_[location]).first->get_isDemoted();
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
  int stackPruning()       //栈剪枝，当栈底部的lir条目移至stack首部，则如果底部的条目不是lir条目，必须移除
  {                         //因为栈底的hir和non-hir条目需保证栈底部为lir条目
    int delnhir = 0;
    while (true)
    {
      EntryInfo::EntryState state = getBottomState();
      assert(state != EntryInfo::kInvalid);
      if(state == EntryInfo::kresidentHIR)
      {
        delnhir ++;
        eraseBottomEntry();
      }
      else if(state == EntryInfo::knonResidentHIR)
      {
				eraseBottomEntry();
      }        
      else
				break;
    }
    return delnhir;
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


class DlirsPolicy : public Policy
{
public:
  DlirsPolicy();

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

  void adjustSize(bool hitHIR );

private:
	int cacheSize;
  int lirSize_;
	int hirSize_;
  int curlir;
  int curhir;
  int curnhir;
  int hir_lir;
	LRUStackS stackS_;
	LRUListQ listQ_;
};

} // namespace dlirs

using dlirs::DlirsPolicy;

} // namespace cs
} // namespace nfd

#endif // NFD_DAEMON_TABLE_CS_POLICY_LRU_HPP
