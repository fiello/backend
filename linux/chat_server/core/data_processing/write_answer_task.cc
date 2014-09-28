/**
 *  \file
 *  \brief     WriteAnswerTask class implementation
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */
#include "write_answer_task.h"
#include <common/exception_dispatcher.h>
#include <network/connection/connection_manager.h>

namespace cs
{
namespace engine
{

WriteAnswerTask::WriteAnswerTask(const MessageDescription& message, MessageList& messageList)
   : m_messageDescription(message)
{
   // we don't really care here if source socket is closed, because aim of this task is to send
   // to another opened connections. So just store socket descriptor for future use
   m_messageList.swap(messageList);
   LOGDBG << "Process message list: " << m_messageList.size();

   m_messageDescription.sender.reset(); // force connection release as we don't need it anymore
}

WriteAnswerTask::WriteAnswerTask(const MessageDescription& message)
   : m_messageDescription(message)
{
   LOGDBG << "Process single message ";
   m_messageDescription.sender.reset();
}

void WriteAnswerTask::Execute()
{
   try
   {
      if (!m_messageList.empty())
      {
         LOGDBG << "Handle message list: " << m_messageList.size();
         for (MessageList::const_iterator msg = m_messageList.begin();
            msg != m_messageList.end();
            ++msg)
         {
            for (network::ConnectionHolderList::const_iterator it = m_activeConnections.begin();
               it != m_activeConnections.end();
               ++it)
            {
               // don't allow sending in 2 cases: either it's a listening socket or it's a sender itself
               if ((*it)->IsListeningSocket() || ((*it)->GetSocketDescriptor() == m_messageDescription.senderSocket))
                  continue;

               (*it)->WriteDataToSocket(*msg);
            }
         }
      }
      else if (!m_messageDescription.data.empty())
      {
         LOGDBG << "Handle single message";
         m_messageDescription.receiver->WriteDataToSocket(m_messageDescription.data);
      }
      else
      {
         LOGERR << "Attempt to execute an empty write message task";
      }
   }
   catch(const std::exception& )
   {
      helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

void WriteAnswerTask::CaptureActiveConnections(network::SocketDescriptor sourceDescriptor)
{
   network::ConnectionManager::GetInstance().GetActiveConnections(m_activeConnections);
}

} // namespace engine
} // namespace cs
