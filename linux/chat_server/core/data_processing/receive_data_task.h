/**
 *  \file
 *  \brief     ReceiveDataTask class declaration
 *  \details   Holds declaration of ReceiveDataTask class responsible for reading data from
 *             network connection, concatenating different fragments between each other and
 *             launching consequent tasks for data processing.
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_ENGINE_RECEIVE_DATA_TASK_H
#define CS_ENGINE_RECEIVE_DATA_TASK_H

#include <network/connection/connection_holder.h>
#include "task.h"
// third-party
#include <boost/noncopyable.hpp>

namespace cs
{
namespace engine
{

/**
 *  \class     cs::engine::ReceiveDataTask
 *  \brief     Class that implements reading data from network interface
 *  \details   Front-end task in the chain of data processing. Responsible
 *             only for reading and concatenating of the data. Quite light class that does not
 *             perform data processing, therefore can be executed in a fast pool.
 */
class ReceiveDataTask
   : public boost::noncopyable
   , public ITask
{
public:
   /**
    * Constructor, requires holder for network connection
    * @param holder - smart object holding network connection
    */
   ReceiveDataTask(const network::ConnectionHolderPtr& holder);

   /**
    * Interface method, implements reading data from given network connection
    */
   virtual void Execute();

private:
   /// holds active network connection that we need to receive data from
   network::ConnectionHolderPtr  m_connection;
};

} // namespace engine
} // namespace cs

#endif // CS_ENGINE_RECEIVE_DATA_TASK_H
