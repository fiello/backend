/**
 *  \file
 *  \brief     ProcessMessageTask class implementation
 *  \details   Holds implementation of ProcessMessageTask class with the number
 *             of auxiliary helper functions.
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "process_message_task.h"
#include <common/exception_dispatcher.h>
#include <common/compiled_definitions.h>
#include <network/connection/connection_manager.h>
// third-party
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

namespace
{

using namespace cs::engine;

/**
 * Local function to get command id by the given name. Caller must be prepared to handle exception
 * with eInvalidArgument code if input command id doesn't match any of the hard-coded commands
 * @param commandName - name of the command to be validated
 * @param id - id of the command if any was found
 */
void GetCommandIdByName(const std::string& commandName, cs::engine::ChatCommandId& id)
{
   static const struct
   {
      ChatCommandId id;
      std::string name;
   }
   ChatCommandNames [] =
   {
      {CommandHelp, "help"},
      {CommandListParticipants, "listall"},
      {CommandNickName, "nickname"},
      {CommandPrivateMessage, "private"},
      {CommandQuit, "quit"},
      {CommandIntro, "intro"}
   };

   static const size_t arraySize = sizeof(ChatCommandNames)/sizeof(ChatCommandNames[0]);

   for(size_t i = 0; i < arraySize; ++i)
      if (ChatCommandNames[i].name == commandName)
      {
         id = ChatCommandNames[i].id;
         return;
      }

   THROW_INVALID_ARGUMENT << "Unable to get command id by name: " << commandName;
}

/**
 * Helper function to validate nickname of the user (used with chat command parsing mostly).
 * Validation is performed for the nickname length and its content.
 * @param nickname - nickname to be validated
 * @param errorMessage - output argument with error message if any
 * @returns - result of the operation performed:
 *             - sOk if nickname passed validation and can be applied
 *             - eInvalidArgument if validation failed. Additional information can be found
 *               in the errorMessage argument
 */
result_t ValidateNickname(const std::string& nickname, std::string& errorMessage)
{
   static const size_t MaxNicknameLength = 50;
   std::ostringstream message;
   std::string upperNickname(nickname);
   boost::to_upper(upperNickname);

   if ( nickname.empty() || (nickname.length() > MaxNicknameLength) ||
        (upperNickname == ServerSenderName) )
   {
      message << "Nickname error: \nNickname can contain only letters [a-z] and digits [0-9].\n"
         << "Empty nicknames are not allowed.\n"
         << "Maximum length of nickname is " << MaxNicknameLength << " symbols.\n"
         << "Nickname cannot be the '" << ServerSenderName << "' service name.";
      errorMessage = message.str();
      return cs::result_code::eInvalidArgument;
   }
   return cs::result_code::sOk;
}

/**
 * Helper function to fire WriteAnswerTask with specific message list. Intended for delivery
 * of several chat messages to all users. This helper function happens to
 * @param messageDescription - message context description to be passed to WriteAnswerTask
 * @param messageList - list of chat messages
 */
void PostMultipleMessages(const MessageDescription& messageDescription, MessageList& messageList)
{
   WriteAnswerTask* task = new WriteAnswerTask(messageDescription, messageList);
   // capture new connections for it - small trick to save time for fast pool
   task->CaptureActiveConnections(messageDescription.senderSocket);
   cs::engine::TaskPtr newTask(task);
   cs::network::ConnectionManager::GetInstance().PostFastTask(newTask);
}

/**
 * Helper function to fire WriteAnswerTask with single message only. Intended for delivery of single
 * message (both chat and service) to one/all users.
 * @param messageDescription - message context description that contains both message data
 *                             and sender/receiver connections' descriptions
 */
void PostSingleMessage(const MessageDescription& messageDescription)
{
   cs::engine::TaskPtr newTask( new WriteAnswerTask(messageDescription) );
   cs::network::ConnectionManager::GetInstance().PostFastTask(newTask);
}

/**
 * Helper function to post service (error / information) message to dedicated user who entered
 * a chat command.
 * @param messageDescription - original message context description. Sender field should
 *                             contain valid connection (can be closed, but still should exist)
 */
void PostServerMessage(const MessageDescription& messageDescription, const std::string& text)
{
   MessageDescription newMessage(messageDescription);
   // respond to user with error server message
   newMessage.receiver = newMessage.sender;
   newMessage.data = ServerSenderName + "> " + text + ChatTerminationSymbol;
   PostSingleMessage(newMessage);
}

} // unnamed namespace


namespace cs
{
namespace engine
{

ProcessMessageTask::ProcessMessageTask(const MessageDescription& message)
{
   size_t length = message.data.length();
   CHECK_ARGUMENT(length > 1, "Message data is empty!");
   CHECK_ARGUMENT(message.data[length-1] == ChatTerminationSymbol, "No termination symbol!");

   m_messageDescription = message;
}

void ProcessMessageTask::Execute()
{
   try
   {
      // split socket data into smaller pieces using termination symbol
      LOGDBG << "Processing: " << m_messageDescription.data;
      std::string singleChatMessage;
      size_t terminationPosition = m_messageDescription.data.find(ChatTerminationSymbol) + 1;

      for (size_t i = 0; i < m_messageDescription.data.size(); i += terminationPosition)
      {
         singleChatMessage = m_messageDescription.data.substr(i, terminationPosition);

         if (singleChatMessage[0] == ChatServiceSymbol)
         {
            // before processing service message get rid of all collected chat messages, because
            // service message can affect further output (user changed his nickname or even quit)
            ProcessChatMessages();
            result_t error = ProcessServiceMessage(singleChatMessage);
            if (error == result_code::eConnectionClosed)
            {
               // connection is closed, no need to work further
               break;
            }
            else if (error != result_code::sOk)
            {
               // assume some wrong command from user - treat it as simple chat message
               StoreChatMessage(m_messageDescription.senderName, singleChatMessage);
            }
         }
         else
         {
            StoreChatMessage(m_messageDescription.senderName, singleChatMessage);
         }

         terminationPosition = m_messageDescription.data.find(ChatTerminationSymbol, i) + 1;
      }

      ProcessChatMessages();
   }
   catch(const std::exception& )
   {
      helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}


void ProcessMessageTask::ProcessChatMessages()
{
   LOGDBG << "Processing message list: " << m_messageList.size();
   if (m_messageList.empty())
   {
      LOGDBG << "Skip empty chat log";
      return;
   }

   PostMultipleMessages(m_messageDescription, m_messageList);
}

void ProcessMessageTask::StoreChatMessage(const std::string& senderName, const std::string& singleChatMessage)
{
   m_messageList.push_back(senderName + "> " + singleChatMessage);
}

result_t ProcessMessageTask::ProcessServiceMessage(const std::string& serviceMessage)
{
   try
   {
      static const boost::regex ServiceMessageRegExpression("^\\\\([A-Za-z]{1,})\\s*(\\s{1,}[A-Za-z0-9]{1,})?(\\s{1,}.*)?[\r]?\n$");
      LOGDBG << "Process service message: " << serviceMessage;
      boost::smatch matches;

      if (!boost::regex_match(serviceMessage, matches, ServiceMessageRegExpression))
         return result_code::eFail;

      std::string chatCommand = matches.str(1);
      std::string commandArgument = matches.str(2);
      std::string commandText = matches.str(3);
      boost::trim(commandArgument);
      LOGDBG << "command = " << chatCommand << "; argument = " << commandArgument << ";"
            " text length " << commandText.length();

      ChatCommandId id;
      GetCommandIdByName(chatCommand, id);
      return AssembleServiceMessage(id, commandArgument, commandText);
   }
   catch(const std::exception&)
   {
      return helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

result_t ProcessMessageTask::AssembleServiceMessage(
   const ChatCommandId id,
   const std::string& commandArgument,
   const std::string& commandText)
{
   network::ConnectionManager& manager = network::ConnectionManager::GetInstance();
   std::string messageText;

   switch (id)
   {
      case CommandQuit:
         m_messageDescription.sender->Close();
         return result_code::eConnectionClosed;
      case CommandHelp:
      {
         std::ostringstream helpMessage;
         helpMessage << "Help message for the " << CORE_PRODUCT_NAME << " version " << CORE_VERSION << ":\n"
               << "\tList of commands available:\n"
               << "\t\\help - produces this help message\n"
               << "\t\\quit - quit chat\n"
               << "\t\\listall - list all active participants\n"
               << "\t\\nickname <new nickname> - change your nickname to a new one\n"
               << "\t\\private <nickname> <message> - post a private message to the dedicated participant";
         PostServerMessage(m_messageDescription, helpMessage.str());
         break;
      }
      case CommandListParticipants:
      {
         network::ConnectionHolderList connections;
         manager.GetActiveConnections(connections);
         messageText = "Active users: ";
         for (network::ConnectionHolderList::const_iterator it = connections.begin();
            it != connections.end();
            ++it)
         {
            if ( (*it)->IsListeningSocket() || (*it)->IsConnectionClosed())
               continue;

            messageText = messageText + ChatTerminationSymbol + " " + (*it)->GetUsername();
         }

         PostServerMessage(m_messageDescription, messageText);
         break;
      }
      case CommandNickName:
      {
         if (ValidateNickname(commandArgument, messageText) != result_code::sOk)
         {
            PostServerMessage(m_messageDescription, messageText);
            return result_code::sOk;
         }

         network::SocketDescriptor socket = m_messageDescription.senderSocket;
         result_t error = manager.SetClientUsername(socket, commandArgument);
         if (error == result_code::eAlreadyDefined)
         {
            // respond to user with error server message
            messageText = "Nickname '" + commandArgument + "' is already in use. Please try another one.";
            PostServerMessage(m_messageDescription, messageText);
            return result_code::sOk;
         }
         else if (error != result_code::sOk)
         {
            LOGERR << "Unable to set username '" << commandArgument << "' for socket " << socket;
            return error;
         }

         PostServerMessage(m_messageDescription, "ok.");
         messageText = "User '" + m_messageDescription.senderName +
               "' is now known as '" + commandArgument + "'" + ChatTerminationSymbol;
         StoreChatMessage(ServerSenderName, messageText);

         m_messageDescription.senderName = commandArgument;
         break;
      }
      case CommandPrivateMessage:
      {
         // don't allow sending loop-back messages
         if (commandArgument == m_messageDescription.senderName)
         {
            messageText = "Private loop-back messages are not allowed.";
            PostServerMessage(m_messageDescription, messageText);
            return result_code::sOk;
         }

         if (ValidateNickname(commandArgument, messageText) != result_code::sOk)
         {
            PostServerMessage(m_messageDescription, messageText);
            return result_code::sOk;
         }

         result_t error = manager.FindConnectionByUsername(commandArgument, m_messageDescription.receiver);
         if (error == result_code::eNotFound )
         {
            // respond to user with error server message
            messageText = "User with the nickname '" + commandArgument + "' doesn't exist.";
            PostServerMessage(m_messageDescription, messageText);
            return result_code::sOk;
         }

         // make a copy of message messadge context as we are about to post single chat message whose context is
         // modifier (receiver, data)das
         MessageDescription newMessage(m_messageDescription);
         newMessage.data = newMessage.senderName + ":private> " + commandText + ChatTerminationSymbol;
         PostSingleMessage(newMessage);
         break;
      }
      case CommandIntro:
      {
         // it's a service message and can be posted by the service account only
         if (m_messageDescription.senderName != ServerSenderName)
            return result_code::eFail;

         std::ostringstream introMessage;
         introMessage << "Hello! You have just entered the chat server (" << CORE_PRODUCT_NAME << " v" << CORE_VERSION << ")."
            "Your current nickname '" << m_messageDescription.receiver->GetUsername() << "' is an auto-generated nickname, you "
            "may want to use the '\\nickname' command to change it. For detailed list of available commands and options "
            "plese use the \\help command." << ChatTerminationSymbol;

         m_messageDescription.data = m_messageDescription.senderName + "> " + introMessage.str();
         PostSingleMessage(m_messageDescription);
         break;
      }
      default:
         return result_code::eInvalidArgument;
   }
   return result_code::sOk;
}


} // namespace engine
} // namespace cs
