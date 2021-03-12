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
	lirSize_ = (int)nMaxEntries * 0.9;
	hirSize_ = cacheSize - lirSize_;
}

void
DlirsPolicy::doAfterInsert(iterator i)
{
	NFD_LOG_INFO("After Insert Function" );

	if (curlir < lirSize_) {
		NFD_LOG_INFO("LIR is not full, insert to LIR, and lirsize is "<< lirSize_<<" after the insertion");
		stackS_.pushEntry({std::make_shared<EntryInfo>(i->getName(), EntryInfo::kLIR), i});
		curlir ++;
		
		stackS_.debugToString("LRU stack S");
        //std::cout << "\n";
        listQ_.debugToString("LRU list Q");
        //std::cout << "\n";
	}
	else if (curhir < hirSize_)
	{
		NFD_LOG_INFO("ResidentHIR is not full, insert to ResidentHIR and hirsize is "<< hirSize_<<" after the insertion");
		addAResidentHIREntry(i);
		curhir ++;

		stackS_.debugToString("LRU stack S");
        //std::cout << "\n";
        listQ_.debugToString("LRU list Q");
        //std::cout << "\n";
	}
	else{
		NFD_LOG_INFO("ResidentHIR and LIR are full, remove a ResidentHIR" );
		EntryPair tmp = listQ_.getAndRemoveFrontEntry();
		stackS_.findAndSetState(tmp.first->getName(), EntryInfo::knonResidentHIR);  //if find will set
		curnhir ++;

		LRUStackSLocation location = stackS_.find(i->getName());
		if (location >= 0) {
			NFD_LOG_INFO("This entry is a nonResidentHIR, it's in stack S" );
			hitHIRInStackS(location, i);
			adjustSize(true);
			hir_lir ++;

		}
		else {
			NFD_LOG_INFO("This new entry is not in both cache and stack S, save it to cache" );
			addAResidentHIREntry(i);
		} 

		stackS_.debugToString("LRU stack S");
        //std::cout << "\n";
        listQ_.debugToString("LRU list Q");
        //std::cout << "\n";

		this->emitSignal(beforeEvict, tmp.second);
	}
	NFD_LOG_INFO("HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
}

void
DlirsPolicy::doAfterRefresh(iterator i)
{
	NFD_LOG_INFO("After Refresh Function" );

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
            //std::cout << "\n";
            listQ_.debugToString("LRU list Q");
            //std::cout << "\n";
		}
		else
		{
			NFD_LOG_INFO("This entry is a ResidentHIR in Stack S");
			hitHIRInStackS(location, i);
			listQ_.findAndRemove(i->getName());
			curhir --, curlir ++;

			stackS_.debugToString("LRU stack S");
            //std::cout << "\n";
            listQ_.debugToString("LRU list Q");
            //std::cout << "\n";
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
			}
			stackS_.pushEntry(listQ_.getEntryByLocation(location));
			listQ_.movToEnd(location, i);

			stackS_.debugToString("LRU stack S");
            //std::cout << "\n";
            listQ_.debugToString("LRU list Q");
            //std::cout << "\n";
		}
		else
			NFD_LOG_INFO("hit but there is not such a man in LRU S stack");
	}
	NFD_LOG_INFO("HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
	// hitTimes_++;
}

void
DlirsPolicy::doBeforeErase(iterator i)
{
  
}

void
DlirsPolicy::doBeforeUse(iterator i)
{
	NFD_LOG_INFO("Before Use Function" );

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
            //std::cout << "\n";
            listQ_.debugToString("LRU list Q");
            //std::cout << "\n";
		}
		else
		{
			NFD_LOG_INFO("This entry is a ResidentHIR in Stack S");
			hitHIRInStackS(location, i);
			listQ_.findAndRemove(i->getName());

			stackS_.debugToString("LRU stack S");
            //std::cout << "\n";
            listQ_.debugToString("LRU list Q");
            //std::cout << "\n";
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
			}
			stackS_.pushEntry(listQ_.getEntryByLocation(location));
			listQ_.movToEnd(location, i);

			stackS_.debugToString("LRU stack S");
            //std::cout << "\n";
            listQ_.debugToString("LRU list Q");
            //std::cout << "\n";
		}
		else
			NFD_LOG_INFO("hit but there is not such a man in LRU S stack");
	}
	NFD_LOG_INFO("HIR size is "<<hirSize_<<", LIR size is "<<lirSize_<<", total size is "<<cacheSize);
	// hitTimes_++;
}

void 
DlirsPolicy::evictEntries() {}

void 
DlirsPolicy::hitHIRInStackS(LRUStackSLocation location, iterator i)
{
	stackS_.movToTop(location, i);
	stackS_.setTopState(EntryInfo::kLIR);
	stackS_.setBottomState(EntryInfo::kresidentHIR);
	stackS_.set_isDemotedByLocation(BottomLocation, true);
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
	int delta = 0;
	if (hitHIR) {
		if (curnhir > hir_lir) {
			delta = 1;
		} else {
			delta = (int)((double)hir_lir / (double)curnhir + 0.5);
		}
	} else {
		if (hir_lir > curnhir) {
			delta = -1;
		} else {
			delta = -(int)((double)curnhir / (double)hir_lir + 0.5);
		}
	}
	hirSize_ += delta;
	if (hirSize_ < 1) {
		hirSize_ = 1;
	}
	if (hirSize_ > cacheSize - 1) {
		hirSize_ = cacheSize - 1;
	}
	lirSize_ = cacheSize - hirSize_;
}

void 
LRUStack::debugToString(std::string const& name)
  {
    NFD_LOG_INFO(" " );
	NFD_LOG_INFO("#############" << name << "#############" );
    //std::cout << "#############" << name << "#############\n";
    std::for_each( container_.begin(), container_.end(), [](EntryPair& item)
      { 
        NFD_LOG_INFO("<" << item.first->getName().toUri() << " , " <<
        item.first->returnStateStr(item.first->getState()) << ">");
        //std::cout << "<" << item.first->getName().toUri() << " , " <<
        //item.first->returnStateStr( item.first->getState() ) << ">\n";
        } );
    //NFD_LOG_INFO("#############" << name << "#############" );
	NFD_LOG_INFO(" " );
    //std::cout << "#############" << name << "#############\n";
  }

} // namespace lru
} // namespace cs
} // namespace nfd
