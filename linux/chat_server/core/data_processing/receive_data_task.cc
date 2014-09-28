/**
 *  \file
 *  \brief     ReceiveDataTask class implementation
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "receive_data_task.h"
#include "process_message_task.h"
#include <common/exception_dispatcher.h>

namespace cs
{
namespace engine
{

ReceiveDataTask::ReceiveDataTask(const network::ConnectionHolderPtr& holder)
{
   CHECK_ARGUMENT(holder.get() != 0, "Empty connection holder!");
   CHECK_ARGUMENT(holder->IsSocketValid(), "Invalid socket");

   m_connection = holder;
}

void ReceiveDataTask::Execute()
{
   try
   {
      network::SocketDescriptor currentSocket = m_connection->GetSocketDescriptor();

      result_t error = m_connection->ReadAndAppendSocketData();
      switch (error)
      {
         case result_code::sOk:
            break;
         case result_code::eBufferOverflow:
            // TODO message length is exceeded
            // submit error message to the client that message length is exceeded
            return;
         case result_code::eConnectionClosed:
            LOGDBG << "Remote end is closed on socket " << currentSocket;
            m_connection->Close();
            return;
         default:
            LOGERR << "Error while reading data on socket " << currentSocket;
            return;
      }

      std::string tempString;
      error = m_connection->GetNextSocketData(tempString);
      if (error != result_code::sOk)
      {
         LOGDBG << "Skip data processing, no termination";
         return;
      }

      // skip empty string
      if (tempString[0] == ChatTerminationSymbol)
         return;

      // dispatch write task further
      MessageDescription message;
      message.sender = m_connection;
      message.senderSocket = currentSocket;
      message.senderName = m_connection->GetUsername();
      message.data = tempString;
      engine::TaskPtr newTask( new ProcessMessageTask(message) );
      network::ConnectionManager::GetInstance().PostSlowTask(newTask);
   }
   catch(const std::exception& )
   {
      // this function will be running inside a thread pool so we don't
      // let it throw exception further. Instead try to post error
      // outgoing message to the remote client
      // TODO: error message
      helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

} // namespace engine
} // namespace cs
