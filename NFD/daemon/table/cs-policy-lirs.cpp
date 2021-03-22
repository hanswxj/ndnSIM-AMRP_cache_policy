#include "cs-policy-lirs.hpp"
#include "cs.hpp"
#include "core/logger.hpp"

NFD_LOG_INIT("LirsPolicy");

namespace nfd {
namespace cs {
namespace lirs {

const std::string LirsPolicy::POLICY_NAME = "lirs";
NFD_REGISTER_CS_POLICY(LirsPolicy);

LirsPolicy::LirsPolicy()
  : Policy(POLICY_NAME),
  lirSize_(9),
  hirSize_(1),
  cacheSize(10)   
{}

void
LirsPolicy::setLimit(size_t nMaxEntries){
	Policy::setLimit(nMaxEntries);
	hirSize_ = 1+ (int)(nMaxEntries / 10);
	lirSize_ = nMaxEntries - hirSize_;
	cacheSize = (int)nMaxEntries;
}

void
LirsPolicy::doAfterInsert(iterator i)
{
	NFD_LOG_INFO("After Insert Function "<<i->getName());

	if ((lirSize_ --) > 0) {
		NFD_LOG_INFO("LIR is not full, insert to LIR, and lirsize is "<< lirSize_<<" after the insertion");
		stackS_.pushEntry({std::make_shared<EntryInfo>(i->getName(), EntryInfo::kLIR), i});
		
		stackS_.debugToString("LRU stack S");
        listQ_.debugToString("LRU list Q");
	}
	else if ((hirSize_ --) > 0)
	{
		NFD_LOG_INFO("ResidentHIR is not full, insert to ResidentHIR and hirsize is "<< hirSize_<<" after the insertion");
		addAResidentHIREntry(i);

		stackS_.debugToString("LRU stack S");
        listQ_.debugToString("LRU list Q");
	}
	else{		
		// for(auto it = stackS_.container_.begin(); it != stackS_.container_.end(); ) {
		// 	if(stackS_.getSize() <= 2 * cacheSize) break;
		// 	if(it->first->getState()== EntryInfo::knonResidentHIR){
		// 		it = stackS_.container_.erase(it);
		// 	}
		// 	else it ++;
		// }

		NFD_LOG_INFO("ResidentHIR and LIR are full, remove a ResidentHIR" );
		EntryPair tmp = listQ_.getAndRemoveFrontEntry();
		stackS_.findAndSetState(tmp.first->getName(), EntryInfo::knonResidentHIR); //if find will set

		LRUStackSLocation location = stackS_.find(i->getName());
		if (location >= 0) {
			NFD_LOG_INFO("This entry is a nonResidentHIR, it's in stack S" );
			hitHIRInStackS(location, i);
		}
		else {
			NFD_LOG_INFO("This new entry is not in both cache and stack S, save it to cache" );
			addAResidentHIREntry(i);
		} 


		// LRUStackSLocation location = stackS_.find(i->getName());
		// if (location >= 0) {
		// 	NFD_LOG_INFO("This entry is a nonResidentHIR, it's in stack S" );
		// 	listQ_.debugToString("LRU list Q");
		// 	EntryPair tmp = listQ_.getAndRemoveFrontEntry();
		//     stackS_.findAndSetState(tmp.first->getName(), EntryInfo::knonResidentHIR);
		// 	hitHIRInStackS(location, i);
		// 	this->emitSignal(beforeEvict, tmp.second);
		// }
		// else {
		// 	NFD_LOG_INFO("This new entry is not in both cache and stack S, save it to stack S" );
		// 	auto info = std::make_shared<EntryInfo>(i->getName(), EntryInfo::knonResidentHIR);
		// 	stackS_.pushEntry({info, i});
		// 	this->emitSignal(beforeEvict, i);
		// }

		stackS_.debugToString("LRU stack S");
        listQ_.debugToString("LRU list Q");


		this->emitSignal(beforeEvict, tmp.second);
	}
}

void
LirsPolicy::doAfterRefresh(iterator i)
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
			stackS_.stackPruning();
			
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
		if (location >= 0) {
			stackS_.pushEntry(listQ_.getEntryByLocation(location));
			listQ_.movToEnd(location, i);

			stackS_.debugToString("LRU stack S");
            listQ_.debugToString("LRU list Q");
		}
		else
			NFD_LOG_INFO("hit but there is not such a man in LRU S stack");
	}

	// hitTimes_++;
}

void
LirsPolicy::doBeforeErase(iterator i)
{
  
}

void
LirsPolicy::doBeforeUse(iterator i)
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
			stackS_.stackPruning();

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
		if (location >= 0) {
			stackS_.pushEntry(listQ_.getEntryByLocation(location));
			listQ_.movToEnd(location, i);

			stackS_.debugToString("LRU stack S");
            listQ_.debugToString("LRU list Q");
		}
		else
			NFD_LOG_INFO("hit but there is not such a man in LRU S stack");
	}

	// hitTimes_++;
}

void 
LirsPolicy::evictEntries() {}

void 
LirsPolicy::hitHIRInStackS(LRUStackSLocation location, iterator i)
{
	stackS_.movToTop(location, i);
	stackS_.setTopState(EntryInfo::kLIR);
	stackS_.setBottomState(EntryInfo::kresidentHIR);
	listQ_.pushToEnd(stackS_.getBottomEntry());
	stackS_.stackPruning();
}

void 
LirsPolicy::addAResidentHIREntry(iterator i)
{
	auto info = std::make_shared<EntryInfo>(i->getName(), EntryInfo::kresidentHIR);
	stackS_.pushEntry({info, i});
	listQ_.pushToEnd({info, i});
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
