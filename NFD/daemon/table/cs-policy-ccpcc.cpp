#include "cs-policy-ccpcc.hpp"
#include "cs.hpp"
#include "core/logger.hpp"

#include "ns3/simulator.h"

NFD_LOG_INIT("CcpccPolicy");

namespace nfd {
namespace cs {
namespace ccpcc {

bool compare(const CpInfo* l, const CpInfo* r) {
  return l->p < r->p;
}

const std::string CcpccPolicy::POLICY_NAME = "ccpcc";
NFD_REGISTER_CS_POLICY(CcpccPolicy);

CcpccPolicy::CcpccPolicy()
  : Policy(POLICY_NAME)
{
  ns3::Simulator::Schedule(ns3::Seconds(T), &CcpccPolicy::update, this);
}

void
CcpccPolicy::doAfterInsert(iterator i)
{
  NFD_LOG_INFO("doAfterInsert " << i->getName());

  const Name &prefix = i->getData().getName().getPrefix(-1);
  auto iter = m_congInfos.find(prefix);
  if (iter == m_congInfos.end()) {
   iter = m_congInfos.insert({prefix, 0}).first;
  }

  if (i->getData().getCongestionMark() > 0) {
    ++ iter->second;
  }

  double history_p = 0;
  uint32_t history_n = 0;
  if (m_historyCpInfos.count(i->getName()) == 1) {
    history_p = m_historyCpInfos[i->getName()].p;
    history_n = m_historyCpInfos[i->getName()].n;

    m_historyCpInfos.erase(i->getName());
  }

  double a = 1 + c * T;
  double cur_p = u_a * (a * history_n + history_p) / (a + 1) + u_b * iter->second;
  if (this->getCs()->size() == this->getLimit() && cur_p <= m_queue.front()->p) {
    HistoryCpInfo history_info;
    history_info.n = history_n + 1;
    history_info.p = history_p;
    m_historyCpInfos.insert({i->getName(), history_info});

    NFD_LOG_INFO("<cur_p = " << cur_p << "> min_p = " << m_queue.front()->p);
    print();

    this->emitSignal(beforeEvict, i);
    return;
  } 

  CpInfo *info = new CpInfo;
  info->prefix = prefix;
  info->p = cur_p;
  info->n = 1;
  info->entry = i;
  
  m_cpInfos.insert({i->getName(), info});
  m_queue.push_back(info);
  m_queue.sort(compare);

  print();

  this->evictEntries();
}

void
CcpccPolicy::doAfterRefresh(iterator i)
{
  NFD_LOG_INFO("doAfterRefresh " << i->getName());

  ++ m_cpInfos[i->getName()]->n;
}

void
CcpccPolicy::doBeforeErase(iterator i)
{
  NFD_LOG_INFO("doBeforeErase " << i->getName());

  // auto iter = m_cpInfos.find(i);
  // m_cpInfos.erase(iter);

  // auto list_iter = std::find(m_queue.begin(), m_queue.end(), iter->second);
  // m_queue.erase(list_iter);
}

void
CcpccPolicy::doBeforeUse(iterator i)
{
  NFD_LOG_INFO("doBeforeUse " << i->getName());

  ++ m_cpInfos[i->getName()]->n;
}

void
CcpccPolicy::evictEntries()
{
  BOOST_ASSERT(this->getCs() != nullptr);
  while (this->getCs()->size() > this->getLimit()) {
    BOOST_ASSERT(!m_queue.empty());

    auto cpinfo =  m_queue.front();
    m_queue.pop_front();

    iterator i = cpinfo->entry;
    m_cpInfos.erase(i->getName());

    HistoryCpInfo history_info;
    history_info.n = cpinfo->n;
    history_info.p = cpinfo->p;
    m_historyCpInfos.insert({i->getName(), history_info});

    NFD_LOG_INFO("evictEntries " << i->getName());

    this->emitSignal(beforeEvict, i);
  }
}

void 
CcpccPolicy::update() 
{
  NFD_LOG_INFO("update");

  double a = 1 + c * T;
  for (auto &cpinfo : m_queue) // update p
  {
    cpinfo->p = u_a * (a * cpinfo->n + cpinfo->p) / (a + 1) + u_b * m_congInfos[cpinfo->prefix];
    cpinfo->n = 0;

    // NFD_LOG_INFO("update (" << cpinfo->entry->getName() << ") p: " << cpinfo->p);
  }

  for (auto iter = m_historyCpInfos.begin(); iter != m_historyCpInfos.end();) {
    NFD_LOG_INFO("history (" << iter->first << ") p: " << iter->second.p << " n: " << iter->second.n);
    iter->second.p = (a * iter->second.n + iter->second.p) / (a + 1);
    iter->second.n = 0;
    // if (iter->second.p < 0.01)
    //   m_historyCpInfos.erase(iter ++);
    // else 
      ++ iter;
  }

  m_congInfos.clear();

  m_queue.sort(compare);

  ns3::Simulator::Schedule(ns3::Seconds(T), &CcpccPolicy::update, this);
}

void 
CcpccPolicy::print()
{
  NFD_LOG_INFO(" " );
	NFD_LOG_INFO("############# Cache #############" );
  for (auto &info : m_queue)
    NFD_LOG_INFO("<" << info->entry->getName().toUri()<< "> "<<info->p);
	NFD_LOG_INFO(" " );
}

} // namespace ccpcc
} // namespace cs
} // namespace nfd
