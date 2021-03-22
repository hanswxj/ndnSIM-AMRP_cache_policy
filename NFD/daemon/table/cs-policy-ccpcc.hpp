#ifndef NFD_DAEMON_TABLE_CS_POLICY_CCPCC_HPP
#define NFD_DAEMON_TABLE_CS_POLICY_CCPCC_HPP

#include "cs-policy.hpp"

#include <map>
#include <list>

namespace nfd {
namespace cs {
namespace ccpcc {


const double u_a = 0.5;
const double u_b = 0.5;

const double c = 0.5;
const int T = 2;

struct CpInfo {
  Name prefix;
  double p;
  uint32_t n;
  iterator entry;
};

struct HistoryCpInfo {
  double p;
  uint32_t n;
};


class CcpccPolicy : public Policy
{
public:
  CcpccPolicy();

public:
  static const std::string POLICY_NAME;

private:
  void
  doAfterInsert(iterator i) override;

  void
  doAfterRefresh(iterator i) override;

  void
  doBeforeErase(iterator i) override;

  void
  doBeforeUse(iterator i) override;

  void
  evictEntries() override;

  void update();

  void print();

private:
  std::list<CpInfo*> m_queue;
  std::map<Name, CpInfo*> m_cpInfos;
  std::map<Name, HistoryCpInfo> m_historyCpInfos;
  std::map<Name, uint32_t> m_congInfos;
};

} // namespace ccpcc

using ccpcc::CcpccPolicy;

} // namespace cs
} // namespace nfd

#endif // NFD_DAEMON_TABLE_CS_POLICY_LRU_HPP
