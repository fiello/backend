/**
 *  \file
 *  \brief     Holds ConfigurationManager class implementation
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "configuration_manager.h"
#include <logger/logger.h>
#include <common/exception_dispatcher.h>
#include <common/compiled_definitions.h>
// third-party
#include <fstream>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

namespace
{

using namespace cs::config;

static const struct
{
   ParameterId id;
   std::string name;
}
ParameterName[] =
{
   {Daemon, "daemon"},
   {TcpIf, "tcp_if"},
   {TcpPort, "tcp_port"},
   {LogLevel, "loglevel"},
   {FastPoolSize, "fast_pool_size"},
   {SlowPoolSize, "slow_pool_size"}
};

/**
 * Get parameter id by the given setting name. Throws eInvalidArgument if setting
 * name is invalid.
 * @param name - name of the setting to be retrieved
 * @returns - id of the setting
 */
ParameterId GetParameterIdByName(const std::string& name)
{
   static const size_t arraySize = sizeof(ParameterName)/sizeof(ParameterName[0]);

   for(size_t i = 0; i < arraySize; ++i)
      if (ParameterName[i].name == name)
         return ParameterName[i].id;

   THROW_INVALID_ARGUMENT << "Unable to get parameter id by name: " << name;
}

/**
 * Get parameter name by the given id. Throws eInvalidArgument if parameter id
 * is invalid
 * @param id - id of the parameter we need to get a name for
 * @returns - name of the parameter
 */
std::string GetParameterNameById(const ParameterId id)
{
   static const size_t arraySize = sizeof(ParameterName)/sizeof(ParameterName[0]);

   for(size_t i = 0; i < arraySize; ++i)
      if (ParameterName[i].id == id)
         return ParameterName[i].name;

   THROW_INVALID_ARGUMENT << "Unable to get parameter name by id: " << (int)id;
}

/**
 * Check setting for valid value
 * @param id - id of the setting to be checked
 * @param settingValue - value of the setting to be validated
 * @returns - result of the operation:
 *            - sOk if value is valid
 *            - eInvalidArgument if value is invalid
 */
result_t CheckSettingValue(const ParameterId id, const int settingValue)
{
   switch (id)
   {
      case FastPoolSize:
      case SlowPoolSize:
      {
         const int minimumLevel = 1;
         const int maximumLevel = 50;
         if (settingValue < minimumLevel || settingValue > maximumLevel)
         {
            LOGERR << "FastPoolSize/SlowPoolSize configurations value must be within these bounds [" << minimumLevel << ";" << maximumLevel << "]";
            return cs::result_code::eInvalidArgument;
         }
      }
      default:
         break;
   }
   return cs::result_code::sOk;
}

/**
 * Generate config file with default settings values
 * @param configName - name of the output file to be generated
 */
void GenerateConfigFile(const std::string& configName)
{
   static const struct
   {
      ParameterId id;
      std::string value;
   }
   ParameterDefaultValues[] =
   {
      {Daemon, "0"},
      {TcpIf, "eth0"},
      {TcpPort, "6667"},
      {LogLevel, "1"},
      {FastPoolSize, "10"},
      {SlowPoolSize, "5"}
   };

   std::ofstream outFile(configName.c_str(), std::fstream::out);
   if (!outFile.good())
      LOGEMPTY << "Unable to create the file specified: " << configName;

   std::ostringstream outStream;
   const size_t arraySize = sizeof(ParameterDefaultValues)/sizeof(ParameterDefaultValues[0]);
   for(size_t i = 0; i < arraySize; ++i)
      outStream << GetParameterNameById((ParameterId)i) << "=" << ParameterDefaultValues[i].value << std::endl;

   outFile << outStream.str();
   outFile.close();
}

} // unnamed namespace



namespace cs
{
namespace config
{

ConfigurationManager& ConfigurationManager::GetInstance()
{
   // g++ guarantees thread-safe initialization for static variable
   static ConfigurationManager manager;
   return manager;
}

result_t ConfigurationManager::ReadComandLineArguments(const int argc, char *argv[])
{
   try
   {
      using namespace boost::program_options;

      // read command line options using boost program_options library
      std::string generalMessage =
      std::string("\nDescription  : Simple text chat application for Linux platform.\n") +
         "Product name : " + CORE_PRODUCT_NAME + "\n" +
         "Developed by : Dmitry Sinelnikov [dmitry.sineln@gmail.com]\n\n" +
         "Generic options";
      options_description generic(generalMessage);
      options_description mandatory("Mandatory options");
      generic.add_options()
         ("help","produce this help message")
         ("version","print server version")
         ("gen-config", value<std::string>(), "generate 'arg' configuration file with default settings");
      mandatory.add_options()
         ("config", value<std::string>(), "run server with 'arg' configuration file");

      generic.add(mandatory);
      variables_map programOptions;	//options that were specified in the command line
      try
      {
         store(parse_command_line(argc, argv, generic), programOptions);
         programOptions.notify();
      }
      catch( const std::exception &ex)
      {
         LOGEMPTY << "Error while parsing command line arguments: " << ex.what() << "\n";
         return result_code::eFail;
      }

      // print help message and exit
      if (programOptions.count("help") || (argc == 1))
      {
         LOGEMPTY << generic;
         return result_code::eFail;
      }

      // print version of the application and exit
      if (programOptions.count("version"))
      {
         LOGEMPTY << "'" << CORE_PRODUCT_NAME << "' product version: " << CORE_VERSION << "\n";
         return result_code::eFail;
      }

      // generate config file with default settings
      if (programOptions.count("gen-config"))
      {
         GenerateConfigFile(programOptions["gen-config"].as<std::string>());
         return result_code::eFail;
      }

      // need this synthetic solution here (instead of ->required() in 'add_options()' call)
      // since --version and --help flags should not produce any error without config
      if (!programOptions.count("config"))
      {
         LOGEMPTY << "Error while parsing command line arguments: missing required option 'config'";
         LOGEMPTY << generic << "\n\n";
         return result_code::eFail;
      }

      // now when we have a config let's try loading settings from it
      return LoadSettingsFromFile(programOptions["config"].as<std::string>());
   }
   catch(const std::exception &)
   {
      return helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

result_t ConfigurationManager::LoadSettingsFromFile(const std::string& configFile)
{
   try
   {
      if (!configFile.empty())
         m_configFileName = configFile;

      std::ifstream inStream(m_configFileName.c_str(), std::fstream::in);
      if (!inStream.good())
         THROW_BASIC_EXCEPTION(result_code::eFail) << "Unable to open config file: " << m_configFileName;


      static const size_t ConfigFileMaximumLines = 512;
      static const boost::regex   ConfigFileRegExpression("(\\w*?)[[:blank:]]*=[[:blank:]]*([A-Za-z0-9\\\\.]*?)");

      ConfigDataStorage tempConfigData;
      std::string line, name, value;
      size_t lineCount = 0;
      boost::smatch matches;
      while ( !inStream.eof() && (++lineCount <= ConfigFileMaximumLines) )
      {
         std::getline(inStream, line);
         boost::trim(line);
         if (!line.empty() && line[0] != '\0' &&
            line[0] != '!' &&	// comment
            line[0] != '#' &&	// comment
            line[0] != '-' &&	// comment
            line[0] != ';')	// comment
         {
            if (boost::regex_match(line, matches, ConfigFileRegExpression))
            {
               name  = matches.str(1);
               value = matches.str(2);
               tempConfigData.insert( std::make_pair(GetParameterIdByName(name), value) );
            }
         }
      }
      inStream.close();

      LOCK lock(m_settingsAccessGuard);
      m_configData.swap(tempConfigData);

      return result_code::sOk;
   }
   catch(const std::exception &)
   {
      return helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

result_t ConfigurationManager::GetSetting(const ParameterId id, std::string& settingValue)
{
   try
   {
      LOCK lock(m_settingsAccessGuard);
      ConfigDataStorage::const_iterator it = m_configData.find(id);
      if (it == m_configData.end())
      {
         LOGERR << "Unable to find requested setting, id = " << id << ", name = " << GetParameterNameById(id);
         return result_code::eNotFound;
      }

      settingValue = it->second;
      LOGDBG << "Got setting pair: [" << (int)id << ", " << settingValue << "]";
      return result_code::sOk;
   }
   catch(const std::exception &)
   {
      return helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

result_t ConfigurationManager::GetSetting(const ParameterId id, int& value)
{
   try
   {
      std::string tempValue;
      result_t error = GetSetting(id, tempValue);
      if (error != result_code::sOk)
         return error;

      std::istringstream stream(tempValue);
      stream >> value;
      return CheckSettingValue(id, value);
   }
   catch(const std::exception &)
   {
      return helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

} // namespace config
} // namespace cs
