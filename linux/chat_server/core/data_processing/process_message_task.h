/**
 *  \file
 *  \brief     ProcessMessageTask class declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_ENGINE_PROCESS_MESSAGE_TASK_H
#define CS_ENGINE_PROCESS_MESSAGE_TASK_H

#include "write_answer_task.h"
#include "task.h"
// third-party
#include <boost/noncopyable.hpp>

namespace cs
{
namespace engine
{

/**
 *  \class     cs::engine::ProcessMessageTask
 *  \brief     Class responsible for data processing
 *  \details   Back-end class in the chain of data processing tasks. Responsible
 *             for decomposing raw data into disjoint lines and separating "chat messages"
 *             from "chat commands". Chat messages are broadcasted to all alive connections.
 *             Chat commands are validated for input arguments and executed. Server responses
 *             are also generated in this class. The heaviest class, should be executed in
 *             a slow pool.
 */
class ProcessMessageTask
   : public boost::noncopyable
   , public ITask
{
public:
   /**
    * Constructor, accept message description to work with
    * @param holder - reference to the structure that describes message context
    */
   ProcessMessageTask(const MessageDescription& message);

   /**
    * Interface method, implements data processing, parsing and responding
    */
   virtual void Execute();
 
private:
   /// Schedule accumulated chat messages to be processed by next type of task
   /// - WriteAnswerTask
   void ProcessChatMessages();
   /// Add new chat message to the list of chat messages. Sender name will be posted in from of chat
   /// message so that remote client could identify the sender
   void StoreChatMessage(const std::string& senderName, const std::string& chatMessage);
   /// Process service message (chat command from client or server) that was detected in message data
   result_t ProcessServiceMessage(const std::string& serviceMessage);
   /// If service message matched predefined pattern and split into fragments these fragments are
   /// passed to this function to be validated, executed and written back to the client as an answer
   result_t AssembleServiceMessage(
      const ChatCommandId id,
      const std::string& commandArgument,
      const std::string& commandText);

   /// description of message context
   MessageDescription   m_messageDescription;
   /// list of text chat messages decomposed from network data
   MessageList          m_messageList;
};

} // namespace engine
} // namespace cs

#endif // CS_ENGINE_PROCESS_MESSAGE_TASK_H
