#include "cs-policy-dlirs.hpp"
#include "cs.hpp"
#include "core/logger.hpp"

NFD_LOG_INIT("DlirsPolicy");

namespace nfd {
namespace cs {
namespace dlirs {

const std::string DlirsPolicy::POLICY_NAME = "dlirs";
NFD_REGISTER_CS_POLICY(DlirsPolicy);

DlirsPolicy::DlirsPolicy()
  : Policy(POLICY_NAME),
  cacheSize(10),
  lirSize_(9),
  hirSize_(1),
  curlir(0), curhir(0), curnhir(0), hir_lir(0)   
{}

void
DlirsPolicy::setLimit(size_t nMaxEntries){
	Policy::setLimit(nMaxEntries);
	cacheSize = nMaxEntries;
	hirSize_ = 1 + (int)(nMaxEntries / 100);
	lirSize_ = cacheSize - hirSize_;
	NFD_LOG_INFO("HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
}

void
DlirsPolicy::doAfterInsert(iterator i)
{
	NFD_LOG_INFO("After Insert Function" );
	NFD_LOG_INFO("HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
	NFD_LOG_INFO("Cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nhir is "<<curnhir);

	
	if (curlir < lirSize_) {
		NFD_LOG_INFO("LIR is not full, insert to LIR, and lirsize is "<< lirSize_<<" after the insertion");
		stackS_.pushEntry({std::make_shared<EntryInfo>(i->getName(), EntryInfo::kLIR), i});
		curlir ++;
		
		stackS_.debugToString("LRU stack S");
        listQ_.debugToString("LRU list Q");
	}
	else if (curhir < hirSize_)
	{
		NFD_LOG_INFO("ResidentHIR is not full, insert to ResidentHIR and hirsize is "<< hirSize_<<" after the insertion");
		addAResidentHIREntry(i);
		curhir ++;

		stackS_.debugToString("LRU stack S");
        listQ_.debugToString("LRU list Q");
	}
	else{
		removeNHIR(curhir + curlir + curnhir - 2 * cacheSize);
		NFD_LOG_INFO("ResidentHIR and LIR are full, remove a ResidentHIR" );		
				
		bool is_Demoted = listQ_.get_isDemotedByLocation(BottomLocation);
		EntryPair tmp = listQ_.getAndRemoveBottomEntry();
		LRUStackSLocation location = stackS_.find(tmp.first->getName());
		if(location != InvalidLocation) {
			stackS_.setStateByLocation(location, EntryInfo::knonResidentHIR);
			curnhir ++;
			stackS_.set_isDemotedByLocation(location, false);
		}
		if(is_Demoted) hir_lir --;		

		location = stackS_.find(i->getName());
		if (location >= 0) {
			NFD_LOG_INFO("This entry is a nonResidentHIR, it's in stack S" );
			hitHIRInStackS(location, i);
			adjustSize(true);
			curnhir--;
			changeLIRtoHIR(curlir - lirSize_);		
		}
		else {
			NFD_LOG_INFO("This new entry is not in both cache and stack S, save it to cache" );
			addAResidentHIREntry(i);
		} 

		stackS_.debugToString("LRU stack S");
        listQ_.debugToString("LRU list Q");

		this->emitSignal(beforeEvict, tmp.second);
	}
	NFD_LOG_INFO("After doAfterInsert, HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
	NFD_LOG_INFO("After doAfterInsert, cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nhir is "<<curnhir);
}

void
DlirsPolicy::doAfterRefresh(iterator i)
{
	NFD_LOG_INFO("After Refresh Function" );
	NFD_LOG_INFO("HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
	NFD_LOG_INFO("Cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nhir is "<<curnhir);

	LRUStackSLocation location = stackS_.find(i->getName());    //在stackS中找这个条目的位置
	if(location >= 0)    //在stackS中找到了
	{
		EntryInfo::EntryState state = stackS_.getStateByLocation(location);    //得到该条目的state
		if(state == EntryInfo::kLIR)
		{
			NFD_LOG_INFO("This entry is a LIR in Stack S");
			stackS_.movToTop(location, i);
			int a = stackS_.stackPruning();
			curnhir -= a;
			
			stackS_.debugToString("LRU stack S");
            listQ_.debugToString("LRU list Q");
		}
		else
		{
			NFD_LOG_INFO("This entry is a ResidentHIR in Stack S");
			hitHIRInStackS(location, i);
			listQ_.findAndRemove(i->getName());
			
			stackS_.debugToString("LRU stack S");
            listQ_.debugToString("LRU list Q");
		}
	}
	else
	{
		NFD_LOG_INFO("This entry is not in Stack S but it's a ResidentHIR in list Q ");
		location = listQ_.find(i->getName());
		bool flag = listQ_.get_isDemotedByLocation(location);
		if (location >= 0) {
			if(flag){
				adjustSize(false);
				listQ_.set_isDemotedByLocation(location, false);
				hir_lir --;
			}
			stackS_.pushEntry(listQ_.getEntryByLocation(location));
			listQ_.movToEnd(location, i);

			// removeHIR(curhir - hirSize_);
			changeHIRtoLIR(lirSize_ - curlir);

			stackS_.debugToString("LRU stack S");
            listQ_.debugToString("LRU list Q");
		}
		else
			NFD_LOG_INFO("hit but there is not such a man in LRU S stack");
	}
	NFD_LOG_INFO("After doAfterRefresh, HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
	NFD_LOG_INFO("After doAfterRefresh, cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nhir is "<<curnhir);
	// hitTimes_++;
}

void
DlirsPolicy::doBeforeErase(iterator i)
{
	removeHIR(curhir - hirSize_);  
}

void
DlirsPolicy::doBeforeUse(iterator i)
{
	NFD_LOG_INFO("Before Use Function" );
	NFD_LOG_INFO("HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
	NFD_LOG_INFO("Cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nhir is "<<curnhir);

	LRUStackSLocation location = stackS_.find(i->getName());    //在stackS中找这个条目的位置
	if(location >= 0)    //在stackS中找到了
	{
		EntryInfo::EntryState state = stackS_.getStateByLocation(location);    //得到该条目的state
		if(state == EntryInfo::kLIR)
		{
			NFD_LOG_INFO("This entry is a LIR in Stack S");
			stackS_.movToTop(location, i);
			int a = stackS_.stackPruning();
			curnhir -= a;

			stackS_.debugToString("LRU stack S");
            listQ_.debugToString("LRU list Q");
		}
		else
		{
			NFD_LOG_INFO("This entry is a ResidentHIR in Stack S");
			hitHIRInStackS(location, i);
			listQ_.findAndRemove(i->getName());

			stackS_.debugToString("LRU stack S");
            listQ_.debugToString("LRU list Q");
		}
	}
	else
	{
		NFD_LOG_INFO("This entry is not in Stack S but is a ResidentHIR in list Q ");
		location = listQ_.find(i->getName());
		bool flag = listQ_.get_isDemotedByLocation(location);
		if (location >= 0) {
			if(flag){
				adjustSize(false);
				listQ_.set_isDemotedByLocation(location, false);
				hir_lir --;
			}
			stackS_.pushEntry(listQ_.getEntryByLocation(location));
			listQ_.movToEnd(location, i);

			// removeHIR(curhir - hirSize_);
			changeHIRtoLIR(lirSize_ - curlir);

			stackS_.debugToString("LRU stack S");
            listQ_.debugToString("LRU list Q");
		}
		else
			NFD_LOG_INFO("hit but there is not such a man in LRU S stack");
	}
	NFD_LOG_INFO("After doBeforeUse, HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
	NFD_LOG_INFO("After doBeforeUse, cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nhir is "<<curnhir);
	// hitTimes_++;
}

void 
DlirsPolicy::evictEntries() 
{
	
}

void 
DlirsPolicy::hitHIRInStackS(LRUStackSLocation location, iterator i)
{
	stackS_.movToTop(location, i);
	stackS_.setTopState(EntryInfo::kLIR);
	bool flag = stackS_.getTopEntry().first->get_isDemoted();
	if(flag) {
		stackS_.set_isDemotedByLocation(stackS_.getContainerSize() - 1, false);
		hir_lir --;
	}
	stackS_.setBottomState(EntryInfo::kresidentHIR);
	stackS_.set_isDemotedByLocation(BottomLocation, true);
	hir_lir ++;
	listQ_.pushToEnd(stackS_.getBottomEntry());
	int a = stackS_.stackPruning();
	curnhir -= a;
}

void 
DlirsPolicy::addAResidentHIREntry(iterator i)
{
	auto info = std::make_shared<EntryInfo>(i->getName(), EntryInfo::kresidentHIR);
	stackS_.pushEntry({info, i});
	listQ_.pushToEnd({info, i});
}

void DlirsPolicy::adjustSize(bool hitHIR)
{
	NFD_LOG_INFO("Adjust Size Function" );
	NFD_LOG_INFO("HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
	int delta = 0;
	if (hitHIR) {
		if (curnhir > hir_lir) {   //Hn > Hd
			// delta = 1;
			delta = 0;
		} else {
			// delta = (int)((double)hir_lir / (double)curnhir + 0.5);
			delta = 1;
		}
	}
	else {
		if (hir_lir > curnhir) {  //Hd > Hn
			// delta = -1;
			delta = 0;
		} else {
			// delta = -(int)((double)curnhir / (double)hir_lir + 0.5);
			delta = -1;
		}
	}
	hirSize_ += delta;
	if (hirSize_ < 1) {
		hirSize_ = 1;
	}
	if (hirSize_ > cacheSize - 1) {   //(int)(cacheSize * 0.24), cacheSize - 1
		hirSize_ = cacheSize - 1;
	}
	lirSize_ = cacheSize - hirSize_;
	NFD_LOG_INFO("After adjustSize, HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
}

void 
DlirsPolicy::changeHIRtoLIR(int k) 
{
	NFD_LOG_INFO("Change HIR to LIR Function" );
	NFD_LOG_INFO("Cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nonHIR size is "<<curnhir);	
	if(k <= 0) return;
	while(k-- > 0) {
		EntryPair HIRentry = listQ_.getAndRemoveFrontEntry();
		bool flag = HIRentry.first->get_isDemoted();
		LRUStackSLocation location = stackS_.find(HIRentry.first->getName());
		if(location != InvalidLocation) {
			stackS_.setStateByLocation(location, EntryInfo::kLIR);
			stackS_.set_isDemotedByLocation(location, false);
		}
		else {
			HIRentry.first->setState(EntryInfo::kLIR);
			HIRentry.first->set_isDemoted(false);
			stackS_.pushEntry(HIRentry);
		}
		curhir --, curlir ++;
		if(flag) hir_lir --;
	}
	NFD_LOG_INFO("After change HIR to LIR, HIR size is "<<hirSize_<<", LIR size is "<<lirSize_);
	NFD_LOG_INFO("After change HIR to LIR, cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nonHIR size is "<<curnhir);
	BOOST_ASSERT((curhir == hirSize_) && (curlir == lirSize_));
}

void 
DlirsPolicy::changeLIRtoHIR(int k) 
{
	NFD_LOG_INFO("Change LIR to HIR Function" );
	NFD_LOG_INFO("Cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nonHIR size is "<<curnhir);	
	if(k <= 0) return;
	while(k-- > 0) {
		stackS_.setBottomState(EntryInfo::kresidentHIR);
		stackS_.set_isDemotedByLocation(BottomLocation, true);
		listQ_.pushToEnd(stackS_.getBottomEntry());
		int a = stackS_.stackPruning();
		curnhir -= a;
		curlir--, curhir++;
		hir_lir++;
	}
	NFD_LOG_INFO("After change LIR to HIR, cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nonHIR size is "<<curnhir);
	BOOST_ASSERT((curhir == hirSize_) && (curlir == lirSize_));
}

void 
DlirsPolicy::removeHIR(int k) 
{
	if(k < 0) return;
	while(k-- > 0) {
		bool is_Demoted = listQ_.get_isDemotedByLocation(BottomLocation);
		EntryPair HIRentry = listQ_.getAndRemoveBottomEntry();
		LRUStackSLocation location = stackS_.find(HIRentry.first->getName());
		if(location != InvalidLocation) {
			stackS_.setStateByLocation(location, EntryInfo::knonResidentHIR);
			curnhir ++;
			stackS_.set_isDemotedByLocation(location, false);
		} 
		curhir --;
		if(is_Demoted) hir_lir --;
		this->emitSignal(beforeEvict, HIRentry.second);
	}
	BOOST_ASSERT(curhir == hirSize_);
}

void 
DlirsPolicy::removeNHIR(int k) 
{
	NFD_LOG_INFO("Remove k nonHIR Function" );
	NFD_LOG_INFO("Cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nonHIR size is "<<curnhir);
	if(k <= 0) return;
	int a = stackS_.erase_K_nHIR(k);
	curnhir -= a;
	NFD_LOG_INFO("After remove NHIR, cur HIR size is "<<curhir<<", cur LIR size is "<<curlir<<", cur nonHIR size is "<<curnhir);
	BOOST_ASSERT(curhir + curlir + curnhir == 2 * cacheSize);
}

void 
LRUStack::debugToString(std::string const& name)
  {
    NFD_LOG_INFO(" " );
	NFD_LOG_INFO("#############" << name << "#############" );
    std::for_each( container_.begin(), container_.end(), [](EntryPair& item)
      { 
        NFD_LOG_INFO("<" << item.first->getName().toUri() << " , " <<
        item.first->returnStateStr(item.first->getState()) << ">");
        } );
    NFD_LOG_INFO(" " );
  }

} // namespace lru
} // namespace cs
} // namespace nfd
