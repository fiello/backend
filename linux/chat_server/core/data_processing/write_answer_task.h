/**
 *  \file
 *  \brief     WriteAnswerTask class declaration
 *  \details   Holds declaration of WriteAnswerTask class responsible for writing data back
 *             to the opened connection (for both server and chat messages)
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_ENGINE_WRITE_ANSWER_TASK_H
#define CS_ENGINE_WRITE_ANSWER_TASK_H

#include "message_description.h"
#include "task.h"
#include <network/connection/connection_manager.h>
#include <network/descriptor.h>
// third-party
#include <boost/noncopyable.hpp>
#include <list>

namespace cs
{
namespace engine
{

typedef std::list<std::string> MessageList;

/**
 *  \class     cs::engine::WriteAnswerTask
 *  \brief     Class that implements writing response data back to network interface
 *  \details   Front-end task in the chain of data processing. Responsible for writing data
 *             back to opened connection. Data can be either broadcast chat messages from other
 *             user or p2p private chat messages or server messages to a dedicated client.
 *             Quite light class that does not perform data processing, therefore can be
 *             executed in a fast pool.
 */
class WriteAnswerTask
   : public boost::noncopyable
   , public ITask
{
public:
   /**
    * Custom constructor, intended for sending list of messages to all users
    * @param message - description of message context
    * @param messageList - list of messages to be sent
    */
   WriteAnswerTask(const MessageDescription& message, MessageList& messageList);

   /**
    * Custom constructor, intended for sending single message to one/all users
    * @param message - description of message context
    */
   WriteAnswerTask(const MessageDescription& message);

   /**
    * Interface method, implements writing data to network interface of specific connection
    */
   virtual void Execute();

   /**
    * Helper function to obtain list of active connections and store them locally
    * in container. Aim of this is to get connections at the point when WriteAnswerTask has
    * been constructed already but is not being processed yet. Trick to save time for the
    * fast pool, therefore this method better be called in slow pool
    * @param sourceDescriptor - descriptor of the source sender, need it to exclude source
    *                           connection from the list of active connections
    */
   void CaptureActiveConnections(network::SocketDescriptor sourceDescriptor);
 
private:
   /// list of active connections that will be targeted while sending simple chat message
   network::ConnectionHolderList m_activeConnections;
   /// context of the message to be sent
   MessageDescription            m_messageDescription;
   /// list of messages to be sent
   MessageList                   m_messageList;
};

} // namespace engine
} // namespace cs

#endif // CS_ENGINE_WRITE_ANSWER_TASK_H
