/**
 *  \file
 *  \brief     NetworkManager class implementation
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "network_manager.h"
#include "interface_addresses_holder.h"
#include <network/socket/socket_address_holder.h>
#include <network/socket/socket_wrapper.h>
#include <config/configuration_manager.h>
#include <logger/logger.h>
#include <common/exception_dispatcher.h>
// third-party
#include <netdb.h>
#include <boost/bind.hpp>
#include <boost/regex.hpp>

namespace
{

/**
 * Helper function to get list of IP addresses from the given network device. Never throws.
 * @param deviceName - name of the network device we need to get IP addresses from
 * @param listIpAddresses - output container with the list of IP addresses if any were received
 * @returns - result code of the operation
 *             - sOk if at least one address was found
 *             - eNotFound if no addresses were found on the given device name
 *             - another error code in case of internal error/exception
 */
result_t GetIpListFromDeviceName(const std::string& deviceName, std::list<std::string>& listIpAddresses)
{
   using namespace cs::network;
   try
   {
      InterfaceAddressesHolder interfaceAddresses;
      char ipAddress[NI_MAXHOST] = {0};
      int error;
      std::list<std::string> tempIpList;

      for (ifaddrs* interface = interfaceAddresses;
           interface != NULL;
           interface = interface->ifa_next)
      {
         if ((deviceName == interface->ifa_name) && (interface->ifa_addr->sa_family == AF_INET))
         {
            ::memset(&ipAddress, 0, sizeof(ipAddress));
            error = ::getnameinfo(interface->ifa_addr, sizeof(sockaddr_in), ipAddress, NI_MAXHOST, 0, 0, NI_NUMERICHOST);
            if (error != 0)
               continue;

            tempIpList.push_back(ipAddress);
         }
      }
      if (tempIpList.empty())
         THROW_BASIC_EXCEPTION(cs::result_code::eNotFound) << "Unable to find IPv4 address binded to the following device: " << deviceName;

      listIpAddresses.swap(tempIpList);
      return cs::result_code::sOk;
   }
   catch(const std::exception &)
   {
      return cs::helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

/**
 * Helper function to reveal if given string matches IPv4 address structure.
 * Acceptable mask for the address is as follows: xxx.xxx.xxx.xxx
 * @param ipAddress - string with the address to be validated
 * @returns - true if given string contains a valid IPv4 address,
 *            false otherwise
 */
bool IsIpV4Address(const std::string& ipAddress)
{
   static const boost::regex IpAddressRegExpression("^((\\d{1,3}\\.){3}\\d{1,3})$");
   boost::smatch matches;

   if(boost::regex_match(ipAddress, matches, IpAddressRegExpression))
      return true;

   return false;
}

} // unnamed namespace


namespace cs
{
namespace network
{

NetworkManager::NetworkManager()
   : m_shutdownRequested(false)
{}

void NetworkManager::Initialize()
{
   // Initialize connection listener before opening any socket connection
   ConnectionManager& connectionManager = ConnectionManager::GetInstance();
   connectionManager.Initialize();

   // read settings from Configuration Manager (we are interested in network interface and local port only)
   config::ConfigurationManager& configManager = config::ConfigurationManager::GetInstance();
   std::string interfaceName("");
   int localPort = 0;
   result_t error;
   if (( error = configManager.GetSetting(config::TcpIf, interfaceName) ) != result_code::sOk)
      THROW_BASIC_EXCEPTION(error) << "Unable to retrieve network address";
   if (( error = configManager.GetSetting(config::TcpPort, localPort) ) != result_code::sOk)
      THROW_BASIC_EXCEPTION(error) << "Unable to retrieve local port";

   LOGDBG << "Got network settings: interface - " << interfaceName << ", port - " << localPort;

   // obosleted value but still requried by Linux OS API function
   static const int SocketBacklogSize = 100;

   // prepare list of IP addresses we want to bind to
   std::list<std::string> ipAddresses;
   if (IsIpV4Address(interfaceName))
   {
      ipAddresses.push_back(interfaceName);
   }
   else
   {
      // It's not an IPv4 address so assume it's a pure interface name (like lo, eth0).
      // Try to retrieve list of ip addresses binded to this interface
      LOGDBG << "Trying to resolve interface name to the ip address";
      error = GetIpListFromDeviceName(interfaceName, ipAddresses);
      if (error != result_code::sOk)
         THROW_BASIC_EXCEPTION(error) << "Unable to retrieve list of ip addresses";
   }

   // finally create&bind listening socket, place it into a holder and add to connection listener to monitor its activity
   for (std::list<std::string>::const_iterator it = ipAddresses.begin(); it != ipAddresses.end(); ++it)
   {
      LOGDBG << "Bind to the ip address: " << (*it);
      SocketWrapperPtr socket( new SocketWrapper(AF_INET, SOCK_STREAM, IPPROTO_IP) );
      socket->SetSocketOption(SOL_SOCKET, SO_REUSEADDR, 1);
      SocketAddressHolder socketAddress(*it, localPort);
      socket->Bind(socketAddress);
      socket->SetNonblocking();
      socket->Listen(SocketBacklogSize);
      // create a holder to store the socket and mark this holder with listener flag to distinguish it from other sockets
      ConnectionHolderPtr connectionHolder( new ConnectionHolder(socket, true) );
      connectionManager.AddConnection(connectionHolder);
   }
}

void NetworkManager::Start()
{
   LOGDBG << "Starting NetworkManager";
   m_listenerThread.reset( new boost::thread( boost::bind(&NetworkManager::ListeningThreadRoutine, this) ) );
}

void NetworkManager::Shutdown()
{
   if (!m_shutdownRequested)
   {
      LOGDBG << "Shutdown NetworkManager";
      m_shutdownRequested = true;
      ConnectionManager::GetInstance().Shutdown();
      if (m_listenerThread.get())
      {
      	m_listenerThread->join();
      	m_listenerThread.reset();
      }
   }
}

void NetworkManager::ListeningThreadRoutine()
{
   try
   {
      LOGDBG << "Listening thread routine";
      // Timeout below is the maximum wait time during which connection listener will be waiting for new connections.
      // In fact it's a time interval which we have to wait before ProcessConnections returns control back to thread routine
      static const int connectionWaitTimeout = 100;
      ConnectionManager& connectionManager = ConnectionManager::GetInstance();
      while (!m_shutdownRequested)
         connectionManager.ProcessConnections(connectionWaitTimeout);
   }
   catch(const std::exception&)
   {
      helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
   LOGDBG << "Exiting from listening thread routine";
}

} // namespace network
} // namespace cs
