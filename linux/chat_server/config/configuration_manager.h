/**
 *  \file
 *  \brief     Holds ConfigurationManager class declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_CONFIG_CONFIGURATION_MANAGER_H_
#define CS_CONFIG_CONFIGURATION_MANAGER_H_

#include <common/result_code.h>
// third-party
#include <string>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

namespace cs
{
namespace config
{

/// List of application specific settings
enum ParameterId
{
   /// Integer settings that defines if application will be running in a daemon mode in
   /// Linux OS. Acceptable values: 0 (usual application), 1 (daemon mode)
   Daemon,

   /// String setting that defines what network interface will be used for TCP-based
   /// connections. Acceptable values: eth0, 192.168.0.1, ...
   TcpIf,

   /// Integer setting that defines what port the TCP-based connection will be opened on.
   /// No further attempts will be made to open connection on higher ports if current port
   /// is busy/denied for access. Acceptable values: 2000, ...
   TcpPort,

   /// Integer setting that defines application log level. For acceptable values please
   /// refer to the cs::logger::LevelId enum.
   LogLevel,

   /// Integer setting that defines size of the 'front-end' thread pool responsible for
   /// receiving new data from clients and sending back data from server.
   /// Acceptable values: 2, ...
   FastPoolSize,

   /// Integer setting that defines size of the 'back-end' thread pool responsible for
   /// processing client data, commutating clients between each other, processing
   /// service messages. Acceptable values: 2, ...
   SlowPoolSize
};

/**
 * \class   cs::config::ConfigurationManager
 * \brief   Class to manage applications settings.
 * \details Responsible parsing command line arguments, reading settings from file,
 *          etcClass is designed as a signleton. In order to get access to the
 *          class instance use GetInstance method.
 */
class ConfigurationManager : public boost::noncopyable
{
public:
   /**
    * Static method to get instance to singleton object
    * @returns - reference to the singleton object
    */
   static ConfigurationManager& GetInstance();

   /**
    * Read command line arguments and recognize input parameters
    * @param argc - number of arguments in the command line
    * @param argv - pointer to the array with command line arguments
    * @returns - result code of the operation:
    *            - sOk if procedure succeeded and the application can continue running
    *            - eFail if procedure detected some abnormal behavior and application
    *              should stop running
    *            - specific error in case of exception
    */
   result_t ReadComandLineArguments(const int argc, char *argv[]);

   /**
    * @param configFile - configuration file that the application should read settings
    *                     from. In case of default value (empty string) application will
    *                     attempt to read settings from config file specified previously
    *                     (if any was provided before).
    * @returns - result code of the operation:
    *            - sOk if procedure succeeded and the application can continue running
    *            - eFail if procedure detected some abnormal behavior and application
    *              should stop running
    *            - specific error in case of exception
    */
   result_t LoadSettingsFromFile(const std::string& configFile = "");

   /**
    * Get string setting value by id
    * @param id - id of the setting we want to get a value for
    * @param value - output string where value of the setting will be copied to
    * @returns - result code of the operation:
    *            - sOk if setting was found
    *            - eNotFound if setting value was not specified
    *            - eInvalidArgument if invalid parameter id was passed in
    */
   result_t GetSetting(const ParameterId id, std::string& value);

   /**
    * Get integer setting value by id
    * @param id - id of the setting we want to get a value for
    * @param value - output integer where value of the setting will be copied to
    * @returns - result code of the operation:
    *            - sOk if setting was found
    *            - eNotFound if setting value was not specified
    *            - eInvalidArgument if invalid parameter id was passed in
    */
   result_t GetSetting(const ParameterId id, int& value);

private:
   typedef std::map<ParameterId, std::string> ConfigDataStorage;
   typedef boost::lock_guard<boost::mutex> LOCK;

   /// Make default constructor private to fit singleton pattern
   ConfigurationManager(){}

   /// file name with configuration settings
   std::string       m_configFileName;
   /// container which holds actual application settings
   ConfigDataStorage m_configData;
   /// guard to synchronize access to settings from different threads
   boost::mutex      m_settingsAccessGuard;
};

} // namespace config
} // namespace cs

/**
 *  \namespace cs::config
 *  \brief     Holds ConfigurationManager class implementation and the list of application
 *             specific settings
 */

#endif // CS_CONFIG_CONFIGURATION_MANAGER_H_
