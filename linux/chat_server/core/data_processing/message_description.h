/**
 *  \file
 *  \brief     Holds list of chat messages and description of message context
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_ENGINE_MESSAGE_DESCRIPTION_H
#define CS_ENGINE_MESSAGE_DESCRIPTION_H

#include <network/connection/connection_holder.h>

namespace cs
{
namespace engine
{

/// termination symbol that is treated as end-of-sequence while parsing new portion of
/// data from client side
static const char ChatTerminationSymbol = '\n';
/// service symbol that is treated as start-of-command flag for the server. Sequence between
/// this symbol and ChatTerminationSymbol (or its extension) is treated as a chat command
static const char ChatServiceSymbol = '\\';
/// nickname that is used in responses from Server
static const std::string ServerSenderName = "SERVER";

/// enum with acceptable chat commands
enum ChatCommandId
{
   /// Description: print block of help messages to the user who entered this command
   /// Format: \help
   CommandHelp,

   /// Description: print list of all currently active chat participants to the user who entered this command
   /// Format: \listall
   CommandListParticipants,

   /// Description: current user will be assigned a new nickname it it's not used by someone else.
   ///           All users will be notified about this event
   /// Format: \nickname <new_nickname>
   CommandNickName,

   /// Description: send private message to the dedicated user
   /// Format: \private <nickname> message
   CommandPrivateMessage,

   /// Description: force server to close connection for the user who entered this command
   /// Format: \quit
   CommandQuit,

   /// Description: introduction message that should be sent to one user only
   /// Format: \intro
   CommandIntro
};

/**
 *  \struct    cs::engine::MessageDescription
 *  \brief     Holds MessageDescription struct declaration
 *  \details   Struct that describes message context. Commonly used by the data
 *             processing tasks to pass message context between each other. Holds
 *             raw data from network and message direction (sender/receiver sides)
 */
struct MessageDescription
{
   /**
    * Constructor, initializes internal variables with default values
    */
   MessageDescription()
      : senderSocket(cs::network::INVALID_DESCRIPTOR)
      , senderName("")
      , data("")
   {}

   /// handle of the socket that the data was received from
   network::SocketDescriptor     senderSocket;
   /// holder of the connection that sent initial message
   network::ConnectionHolderPtr  sender;
   /// holder of the connection that the message will be sent to
   network::ConnectionHolderPtr  receiver;
   /// sender name
   std::string                   senderName;
   /// raw data received from network. ProcessMessageTask however assumes
   /// that this block of data has ChatTerminationSymbol at the last position
   std::string                   data;
};

} // namespace engine
} // namespace cs

#endif // CS_ENGINE_MESSAGE_DESCRIPTION_H
