/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2017 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */

#ifndef NDN_FACE_HPP
#define NDN_FACE_HPP

#include "data.hpp"
#include "name.hpp"
#include "interest.hpp"
#include "interest-filter.hpp"
#include "encoding/nfd-constants.hpp"
#include "lp/nack.hpp"
#include "net/asio-fwd.hpp"
#include "security/key-chain.hpp"
#include "security/signing-info.hpp"

namespace ndn {

class Transport;

class PendingInterestId;
class RegisteredPrefixId;
class InterestFilterId;

namespace nfd {
class Controller;
} // namespace nfd

/**
 * @brief Callback invoked when expressed Interest gets satisfied with a Data packet
 */
typedef function<void(const Interest&, const Data&)> DataCallback;

/**
 * @brief Callback invoked when Nack is sent in response to expressed Interest
 */
typedef function<void(const Interest&, const lp::Nack&)> NackCallback;

/**
 * @brief Callback invoked when expressed Interest times out
 */
typedef function<void(const Interest&)> TimeoutCallback;

/**
 * @brief Callback invoked when incoming Interest matches the specified InterestFilter
 */
typedef function<void(const InterestFilter&, const Interest&)> InterestCallback;

/**
 * @brief Callback invoked when registerPrefix or setInterestFilter command succeeds
 */
typedef function<void(const Name&)> RegisterPrefixSuccessCallback;

/**
 * @brief Callback invoked when registerPrefix or setInterestFilter command fails
 */
typedef function<void(const Name&, const std::string&)> RegisterPrefixFailureCallback;

/**
 * @brief Callback invoked when unregisterPrefix or unsetInterestFilter command succeeds
 */
typedef function<void()> UnregisterPrefixSuccessCallback;

/**
 * @brief Callback invoked when unregisterPrefix or unsetInterestFilter command fails
 */
typedef function<void(const std::string&)> UnregisterPrefixFailureCallback;

/**
 * @brief Provide a communication channel with local or remote NDN forwarder
 */
class Face : noncopyable
{
public:
  class Error : public std::runtime_error
  {
  public:
    explicit
    Error(const std::string& what)
      : std::runtime_error(what)
    {
    }
  };

  /**
   * @brief Exception thrown when attempting to send a packet over size limit
   */
  class OversizedPacketError : public Error
  {
  public:
    /**
     * @brief Constructor
     * @param pktType packet type, 'I' for Interest, 'D' for Data, 'N' for Nack
     * @param name packet name
     * @param wireSize wire encoding size
     */
    OversizedPacketError(char pktType, const Name& name, size_t wireSize);

  public:
    const char pktType;
    const Name name;
    const size_t wireSize;
  };

public: // constructors
  /**
   * @brief Create Face using default transport and given io_service
   *
   * Usage examples:
   *
   * @code
   * Face face1;
   * Face face2(face1.getIoService());
   *
   * // Now the following ensures that events on both faces are processed
   * face1.processEvents();
   * // or face1.getIoService().run();
   * @endcode
   *
   * or
   *
   * @code
   * boost::asio::io_service ioService;
   * Face face1(ioService);
   * Face face2(ioService);
   *
   * ioService.run();
   * @endcode
   *
   * @param ioService A reference to boost::io_service object that should control all
   *                  IO operations.
   * @throw ConfigFile::Error the configuration file cannot be parsed or specifies an unsupported protocol
   */
  explicit
  Face(DummyIoService& ioService);

  /**
   * @brief Create Face using given transport (or default transport if omitted)
   * @param transport the transport for lower layer communication. If nullptr,
   *                  a default transport will be used. The default transport is
   *                  determined from a FaceUri in NDN_CLIENT_TRANSPORT environ,
   *                  a FaceUri in configuration file 'transport' key, or UnixTransport.
   *
   * @throw ConfigFile::Error @p transport is nullptr, and the configuration file cannot be
   *                          parsed or specifies an unsupported protocol
   * @note shared_ptr is passed by value because ownership is shared with this class
   */
  explicit
  Face(shared_ptr<Transport> transport = nullptr);

  /**
   * @brief Create Face using given transport and KeyChain
   * @param transport the transport for lower layer communication. If nullptr,
   *                  a default transport will be used.
   * @param keyChain the KeyChain to sign commands
   *
   * @sa Face(shared_ptr<Transport>)
   *
   * @throw ConfigFile::Error @p transport is nullptr, and the configuration file cannot be
   *                          parsed or specifies an unsupported protocol
   * @note shared_ptr is passed by value because ownership is shared with this class
   */
  Face(shared_ptr<Transport> transport, KeyChain& keyChain);

  virtual
  ~Face();

public: // consumer
  /**
   * @brief Express Interest
   * @param interest the Interest; a copy will be made, so that the caller is not
   *                 required to maintain the argument unchanged
   * @param afterSatisfied function to be invoked if Data is returned
   * @param afterNacked function to be invoked if Network NACK is returned
   * @param afterTimeout function to be invoked if neither Data nor Network NACK
   *                     is returned within InterestLifetime
   * @throw OversizedPacketError encoded Interest size exceeds MAX_NDN_PACKET_SIZE
   */
  const PendingInterestId*
  expressInterest(const Interest& interest,
                  const DataCallback& afterSatisfied,
                  const NackCallback& afterNacked,
                  const TimeoutCallback& afterTimeout);

  /**
   * @brief Cancel previously expressed Interest
   *
   * @param pendingInterestId The ID returned from expressInterest.
   */
  void
  removePendingInterest(const PendingInterestId* pendingInterestId);

  /**
   * @brief Cancel all previously expressed Interests
   */
  void
  removeAllPendingInterests();

  /**
   * @brief Get number of pending Interests
   */
  size_t
  getNPendingInterests() const;

public: // producer
  /**
   * @brief Set InterestFilter to dispatch incoming matching interest to onInterest
   * callback and register the filtered prefix with the connected NDN forwarder
   *
   * This version of setInterestFilter combines setInterestFilter and registerPrefix
   * operations and is intended to be used when only one filter for the same prefix needed
   * to be set.  When multiple names sharing the same prefix should be dispatched to
   * different callbacks, use one registerPrefix call, followed (in onSuccess callback) by
   * a series of setInterestFilter calls.
   *
   * @param interestFilter Interest filter (prefix part will be registered with the forwarder)
   * @param onInterest     A callback to be called when a matching interest is received
   * @param onFailure      A callback to be called when prefixRegister command fails
   * @param flags          (optional) RIB flags
   * @param signingInfo    (optional) Signing parameters.  When omitted, a default parameters
   *                       used in the signature will be used.
   *
   * @return Opaque registered prefix ID which can be used with unsetInterestFilter or
   *         removeRegisteredPrefix
   */
  const RegisteredPrefixId*
  setInterestFilter(const InterestFilter& interestFilter,
                    const InterestCallback& onInterest,
                    const RegisterPrefixFailureCallback& onFailure,
                    const security::SigningInfo& signingInfo = security::SigningInfo(),
                    uint64_t flags = nfd::ROUTE_FLAG_CHILD_INHERIT);

  /**
   * @brief Set InterestFilter to dispatch incoming matching interest to onInterest
   * callback and register the filtered prefix with the connected NDN forwarder
   *
   * This version of setInterestFilter combines setInterestFilter and registerPrefix
   * operations and is intended to be used when only one filter for the same prefix needed
   * to be set.  When multiple names sharing the same prefix should be dispatched to
   * different callbacks, use one registerPrefix call, followed (in onSuccess callback) by
   * a series of setInterestFilter calls.
   *
   * @param interestFilter Interest filter (prefix part will be registered with the forwarder)
   * @param onInterest     A callback to be called when a matching interest is received
   * @param onSuccess      A callback to be called when prefixRegister command succeeds
   * @param onFailure      A callback to be called when prefixRegister command fails
   * @param flags          (optional) RIB flags
   * @param signingInfo    (optional) Signing parameters.  When omitted, a default parameters
   *                       used in the signature will be used.
   *
   * @return Opaque registered prefix ID which can be used with unsetInterestFilter or
   *         removeRegisteredPrefix
   */
  const RegisteredPrefixId*
  setInterestFilter(const InterestFilter& interestFilter,
                    const InterestCallback& onInterest,
                    const RegisterPrefixSuccessCallback& onSuccess,
                    const RegisterPrefixFailureCallback& onFailure,
                    const security::SigningInfo& signingInfo = security::SigningInfo(),
                    uint64_t flags = nfd::ROUTE_FLAG_CHILD_INHERIT);

  /**
   * @brief Set InterestFilter to dispatch incoming matching interest to onInterest callback
   *
   * @param interestFilter Interest
   * @param onInterest A callback to be called when a matching interest is received
   *
   * This method modifies library's FIB only, and does not register the prefix with the
   * forwarder.  It will always succeed.  To register prefix with the forwarder, use
   * registerPrefix, or use the setInterestFilter overload taking two callbacks.
   *
   * @return Opaque interest filter ID which can be used with unsetInterestFilter
   */
  const InterestFilterId*
  setInterestFilter(const InterestFilter& interestFilter,
                    const InterestCallback& onInterest);

  /**
   * @brief Register prefix with the connected NDN forwarder
   *
   * This method only modifies forwarder's RIB and does not associate any
   * onInterest callbacks.  Use setInterestFilter method to dispatch incoming Interests to
   * the right callbacks.
   *
   * @param prefix      A prefix to register with the connected NDN forwarder
   * @param onSuccess   A callback to be called when prefixRegister command succeeds
   * @param onFailure   A callback to be called when prefixRegister command fails
   * @param signingInfo (optional) Signing parameters.  When omitted, a default parameters
   *                    used in the signature will be used.
   * @param flags       Prefix registration flags
   *
   * @return The registered prefix ID which can be used with unregisterPrefix
   * @see nfd::RouteFlags
   */
  const RegisteredPrefixId*
  registerPrefix(const Name& prefix,
                 const RegisterPrefixSuccessCallback& onSuccess,
                 const RegisterPrefixFailureCallback& onFailure,
                 const security::SigningInfo& signingInfo = security::SigningInfo(),
                 uint64_t flags = nfd::ROUTE_FLAG_CHILD_INHERIT);

  /**
   * @brief Remove the registered prefix entry with the registeredPrefixId
   *
   * This does not affect another registered prefix with a different registeredPrefixId,
   * even it if has the same prefix name.  If there is no entry with the
   * registeredPrefixId, do nothing.
   *
   * unsetInterestFilter will use the same credentials as original
   * setInterestFilter/registerPrefix command
   *
   * @param registeredPrefixId The ID returned from registerPrefix
   */
  void
  unsetInterestFilter(const RegisteredPrefixId* registeredPrefixId);

  /**
   * @brief Remove previously set InterestFilter from library's FIB
   *
   * This method always succeeds and will **NOT** send any request to the connected
   * forwarder.
   *
   * @param interestFilterId The ID returned from setInterestFilter.
   */
  void
  unsetInterestFilter(const InterestFilterId* interestFilterId);

  /**
   * @brief Unregister prefix from RIB
   *
   * unregisterPrefix will use the same credentials as original
   * setInterestFilter/registerPrefix command
   *
   * If registeredPrefixId was obtained using setInterestFilter, the corresponding
   * InterestFilter will be unset too.
   *
   * @param registeredPrefixId The ID returned from registerPrefix
   * @param onSuccess          Callback to be called when operation succeeds
   * @param onFailure          Callback to be called when operation fails
   */
  void
  unregisterPrefix(const RegisteredPrefixId* registeredPrefixId,
                   const UnregisterPrefixSuccessCallback& onSuccess,
                   const UnregisterPrefixFailureCallback& onFailure);

  /**
   * @brief Publish data packet
   * @param data the Data; a copy will be made, so that the caller is not required to
   *             maintain the argument unchanged
   *
   * This method can be called to satisfy incoming Interests, or to add Data packet into the cache
   * of the local NDN forwarder if forwarder is configured to accept unsolicited Data.
   *
   * @throw OversizedPacketError encoded Data size exceeds MAX_NDN_PACKET_SIZE
   */
  void
  put(Data data);

  /**
   * @brief Send a network NACK
   * @param nack the Nack; a copy will be made, so that the caller is not required to
   *             maintain the argument unchanged
   * @throw OversizedPacketError encoded Nack size exceeds MAX_NDN_PACKET_SIZE
   */
  void
  put(lp::Nack nack);

public: // IO routine
  /**
   * @brief Process any data to receive or call timeout callbacks.
   *
   * This call will block forever (default timeout == 0) to process IO on the face.
   * To exit, one expected to call face.shutdown() from one of the callback methods.
   *
   * If positive timeout is specified, then processEvents will exit after this timeout, if
   * not stopped earlier with face.shutdown() or when all active events finish.  The call
   * can be called repeatedly, if desired.
   *
   * If negative timeout is specified, then processEvents will not block and process only
   * pending events.
   *
   * @param timeout     maximum time to block the thread
   * @param keepThread  Keep thread in a blocked state (in event processing), even when
   *                    there are no outstanding events (e.g., no Interest/Data is expected)
   *
   * @note This may throw an exception for reading data or in the callback for processing
   * the data.  If you call this from an main event loop, you may want to catch and
   * log/disregard all exceptions.
   *
   * @throw OversizedPacketError encoded packet size exceeds MAX_NDN_PACKET_SIZE
   */
  void
  processEvents(time::milliseconds timeout = time::milliseconds::zero(),
                bool keepThread = false)
  {
    this->doProcessEvents(timeout, keepThread);
  }

  /**
   * @brief Shutdown face operations
   *
   * This method cancels all pending operations and closes connection to NDN Forwarder.
   *
   * Note that this method does not stop IO service and if the same IO service is shared
   * between multiple Faces or with other IO objects (e.g., Scheduler).
   */
  void
  shutdown();

  /**
   * @brief Return nullptr (cannot use IoService in simulations), preserved for API compatibility
   */
  DummyIoService&
  getIoService()
  {
    static DummyIoService io;
    return io;
  }

NDN_CXX_PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  /**
   * @return underlying transport
   */
  shared_ptr<Transport>
  getTransport();

protected:
  virtual void
  doProcessEvents(time::milliseconds timeout, bool keepThread);

private:
  /**
   * @throw ConfigFile::Error on parse error and unsupported protocols
   */
  shared_ptr<Transport>
  makeDefaultTransport();

  /**
   * @throw Face::Error on unsupported protocol
   * @note shared_ptr is passed by value because ownership is transferred to this function
   */
  void
  construct(shared_ptr<Transport> transport, KeyChain& keyChain);

  void
  onReceiveElement(const Block& blockFromDaemon);

  void
  asyncShutdown();

private:
  shared_ptr<Transport> m_transport;

  unique_ptr<nfd::Controller> m_nfdController;

  class Impl;
  shared_ptr<Impl> m_impl;
};

} // namespace ndn

#endif // NDN_FACE_HPP
