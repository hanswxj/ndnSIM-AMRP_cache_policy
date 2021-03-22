#include "cs-policy-ccp.hpp"
#include "cs.hpp"
#include "core/logger.hpp"

#include "ns3/simulator.h"

NFD_LOG_INIT("CcpPolicy");

namespace nfd {
namespace cs {
namespace ccp {

bool compare(const CpInfo* l, const CpInfo* r) {
  return l->p < r->p;
}

const std::string CcpPolicy::POLICY_NAME = "ccp";
NFD_REGISTER_CS_POLICY(CcpPolicy);

CcpPolicy::CcpPolicy()
  : Policy(POLICY_NAME)
{
  ns3::Simulator::Schedule(ns3::Seconds(T), &CcpPolicy::update, this);
}

void
CcpPolicy::doAfterInsert(iterator i)
{
  NFD_LOG_INFO("doAfterInsert " << i->getName());

  CpInfo *info = new CpInfo;
  info->p = 0;
  info->n = 1;
  info->entry = i;
  
  m_cpInfos.insert({i->getName(), info});
  m_queue.push_back(info);

  print();

  this->evictEntries();
}

void
CcpPolicy::doAfterRefresh(iterator i)
{
  NFD_LOG_INFO("doAfterRefresh " << i->getName());

  ++ m_cpInfos[i->getName()]->n;
}

void
CcpPolicy::doBeforeErase(iterator i)
{
  NFD_LOG_INFO("doBeforeErase " << i->getName());

  // auto iter = m_cpInfos.find(i);
  // m_cpInfos.erase(iter);

  // auto list_iter = std::find(m_queue.begin(), m_queue.end(), iter->second);
  // m_queue.erase(list_iter);
}

void
CcpPolicy::doBeforeUse(iterator i)
{
  NFD_LOG_INFO("doBeforeUse " << i->getName());

  ++ m_cpInfos[i->getName()]->n;
}

void
CcpPolicy::evictEntries()
{
  BOOST_ASSERT(this->getCs() != nullptr);
  while (this->getCs()->size() > this->getLimit()) {
    BOOST_ASSERT(!m_queue.empty());

    auto cpinfo =  m_queue.front();
    m_queue.pop_front();

    iterator i = cpinfo->entry;
    m_cpInfos.erase(i->getName());

    NFD_LOG_INFO("evictEntries " << i->getName());

    this->emitSignal(beforeEvict, i);
  }
  m_queue.sort(compare);
}

void 
CcpPolicy::update() 
{
  NFD_LOG_INFO("update");

  double a = 1 + c * T;
  for (auto &cpinfo : m_queue) // update p
  {
    cpinfo->p = (a * cpinfo->n + cpinfo->p) / (a + 1);
    cpinfo->n = 0;

    // NFD_LOG_INFO("update (" << cpinfo->entry->getName() << ") p: " << cpinfo->p);
  }

  m_queue.sort(compare);

  ns3::Simulator::Schedule(ns3::Seconds(T), &CcpPolicy::update, this);
}

void 
CcpPolicy::print()
{
  NFD_LOG_INFO(" " );
	NFD_LOG_INFO("############# Cache #############" );
  for (auto &info : m_queue)
    NFD_LOG_INFO("<" << info->entry->getName().toUri()<< "> "<<info->p);
	NFD_LOG_INFO(" " );
}

} // namespace ccp
} // namespace cs
} // namespace nfd
