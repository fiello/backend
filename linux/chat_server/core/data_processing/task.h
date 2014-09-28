/**
 *  \file
 *  \brief     ITask interface declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_ENGINE_TASK_H
#define CS_ENGINE_TASK_H

#include <common/result_code.h>
// third-party
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/function.hpp>

namespace cs
{
namespace engine
{

class ITask;
typedef boost::shared_ptr<ITask> TaskPtr;

/**
 *  \class     cs::engine::ITask
 *  \brief     Interface class for data processing tasks
 *  \details   Class that describes an interface for data processing tasks. Those tasks are
 *             implemented in scope of cs::engine namespace. They are responsible for
 *             reading and parsing data from clients, processing received data and
 *             composing responses.
 */
class ITask
{
public:
   /**
    * Main method that should hold data processing procedure. Each type of task will
    * implement its own algorithm and logic, there is no need for the caller to be aware
    * of it. Consequent task (if any) will be created and posted to the same thread
    * pool by current task in its Execute method.
    */
   virtual void Execute() = 0;
};

} // namespace engine
} // namespace cs

#endif // CS_ENGINE_TASK_H
