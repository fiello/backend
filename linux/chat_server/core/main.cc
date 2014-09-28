/**
 *  \file
 *  \brief     Main file where application breath begins: entry point of the application
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

/* Uncomment header below if need to enable critical signal handling: SIGSEGV, SIGFPE
#include <common/signal_translator.h>*/
#include "server_engine.h"
#include <config/configuration_manager.h>
#include <common/exception_dispatcher.h>
#include <common/result_code.h>

/**
 * Entry point of the application. Responsible for parsing command line arguments and
 * launching server engine.
 * @param argc - number of input arguments (handled by OS)
 * @param argv - pointer to the array with input arguments (handled by OS)
 * @returns - application resulting code:
 *            - sOk if application finished running properly
 *            - eFail if arguments parsing was not successful
 *            - specific error if any is thrown from server engine
 */
int main(int argc, char *argv[])
{
   using namespace cs;

   // read command line options and apply settings from config file
   config::ConfigurationManager& configManager = config::ConfigurationManager::GetInstance();
   result_t error = configManager.ReadComandLineArguments(argc, argv);
   if (error != result_code::sOk)
      return error;

   try
   {
      // create server engine and start it (blocking call to Start)
      boost::scoped_ptr<engine::ServerEngine> server( new engine::ServerEngine() );
      server->Start();
   }
   catch(const std::exception&)
   {
      return helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }

   return result_code::sOk;
}
