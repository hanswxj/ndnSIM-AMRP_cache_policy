#ifndef NFD_DAEMON_TABLE_CS_POLICY_CCP_HPP
#define NFD_DAEMON_TABLE_CS_POLICY_CCP_HPP

#include "cs-policy.hpp"

#include <map>
#include <list>

namespace nfd {
namespace cs {
namespace ccp {

const double c = 0.5;
const int T = 2;

struct CpInfo {
  double p;
  uint32_t n;
  iterator entry;
};


class CcpPolicy : public Policy
{
public:
  CcpPolicy();

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
};

} // namespace ccp

using ccp::CcpPolicy;

} // namespace cs
} // namespace nfd

#endif // NFD_DAEMON_TABLE_CS_POLICY_LRU_HPP
