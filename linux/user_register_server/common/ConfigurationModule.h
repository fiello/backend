
#ifndef CONFIGURATIONMODULE_H_
#define CONFIGURATIONMODULE_H_

//native
#include "Logger.h"
#include "ReturnCodes.h"
#include "CompiledDefinitions.h"
//third-party
#include <boost/variant.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/thread/mutex.hpp>
#include <map>

//////////////////////////////////////////////////////////////////////////
namespace Config
{
	using namespace trc;
	namespace po = boost::program_options;
	namespace pt = boost::property_tree;

	enum etConfigParameter
	{
		eCONFIG_UNDEFINED = - 1,

		//config file params
		eCONFIG_DAEMON_MODE,
		eCONFIG_TCP_IF,
		eCONFIG_TCP_PORT,
		eCONFIG_UDP_IF,
		eCONFIG_UDP_PORT,
		eCONFIG_MAINT,
		eCONFIG_DATA_FILE,
		eCONFIG_SLEEP,
		eCONFIG_LOG_LEVEL,

		//additional comamnd-line params
		eCONFIG_KILL_PROCESS,
		eCONFIG_THREAD_POOL,

		eCONFIG_COUNT
	};

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Configuration module, designed as a singleton and intended to perform read/write
	 * configs and settings and peform some parsing of input params from command line
	 */
	class CConfigurationModule
	{
		typedef boost::variant<int, std::string> Setting; // variant type for storing setting
		typedef std::map<int, Setting> tMapSetting;
		typedef tMapSetting::iterator tIterSetting;
		typedef std::map<int, std::string> tMapParams;
		typedef tMapParams::iterator tIterParams;

	private: // permit no copy, default ctors/dtors
		CConfigurationModule();
		CConfigurationModule(const CConfigurationModule& src);
		CConfigurationModule& operator= (const CConfigurationModule& src);
		~CConfigurationModule(){;}

	public: // methods
		static CConfigurationModule& Instance();
		void Destroy();

		// work with command line options
		bool ProcessServerOptions(bool bIsFirstLaunch, int argc, char *argv[]);
		template <typename TValue>
		bool GetProgramOption(const char* szOptionName, TValue& OptionValue);
		template <typename TValue>
		bool GetProgramOption(int iParamID, TValue& OptionValue);
		template <typename TValue>
		void SetProgramOption(int iParamID, const TValue& OptionValue);

		// work with config file
		template <typename TValue>
		bool GetSetting(int id, TValue& retValue);
		template <typename TValue>
		void SetSetting(int id, const TValue& val);
		static std::string GetStartPath() { return m_strInitialStartDir; }

		template <typename TValue>
		bool GetDefaultValue(int iParamID, TValue& retValue);
		bool IsParameterPresent(const std::string&);
		bool IsParameterPresent(int);
		std::string GetParameterName(int iParamID);


	private: //members
		static CConfigurationModule* m_pSelf;
		static boost::mutex m_configAccess;
		static std::string m_strInitialStartDir;

		tMapSetting m_SettingsStore; // settings store
		tMapSetting m_mapDefaultSettings; // data that can be cured as default
		tMapParams m_mapParamNames; // string name to the Setting parameter

		po::variables_map m_poOptionsMap;	//options that were specified in the command line
		pt::ptree m_treeConfig;
		std::string m_strConfigFile; //absolute path to the server config file
		bool m_bIsFirstLaunch;

	private: // methods
		// Init various path settings, create config paths if needed, etc
		bool IsConfigFilePresent();
		void ReadSettingsFromFile();
		void InitParamMap();
		std::string GetExecutablePath();

		void CheckServerParameters(int iParamID, bool bIsFirstLaunch = false);
		template <typename TValue>
		bool CureParameter(int iParamID, TValue& retValue);

		// Set of overloaded functions to get setting from config file based on its name
		void GetParameterFromTree(const std::string& strParamName, int& ulValue);
		void GetParameterFromTree(const std::string& strParamName, std::string& strValue);

		// Set of overloaded functions to write setting to config file based on its name
		void WriteParameterToTree(const std::string& strParamName, int ulValue);
		void WriteParameterToTree(const std::string& strParamName, const std::string& strValue);
	};

	//////////////////////////////////////////////////////////////////////
	/**
	 * Check whether program option was specified in the command line
	 * @param szOptionName - in, name of the option to find
	 * @param strOptionValue - in/out, value of the option if exists. If option doesn't exist
	 *     then input param remain unchanged
	 * @return - true if option was found in the list of command line params
	 */
	template <typename TValue>
	bool CConfigurationModule::GetProgramOption(const char* szOptionName, TValue& OptionValue)
	{
		try
		{
			if (m_poOptionsMap.count(szOptionName))
			{
				if (!m_poOptionsMap[szOptionName].value().empty())
				{
					OptionValue = m_poOptionsMap[szOptionName].as<TValue>();
					return true;
				}
			}
		}
		catch(const boost::bad_any_cast& ex)
		{
			LOGERROR << "Error while converting command line option, option=" << szOptionName<<", err message="<<ex.what();
		}
		catch(...)
		{
			LOGDEBUG << "Error while reading comamnd-line option, option name=" << szOptionName;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////
	/**
	 * Check whether program option was specified in the command line
	 * @param iParamID - in, id of the option to find
	 * @param strOptionValue - in/out, value of the option if exists. If option doesn't exist
	 *     then input param remain unchanged
	 * @return - true if option was found in the list of command line params
	 */
	template <typename TValue>
	bool CConfigurationModule::GetProgramOption(int iParamID, TValue& OptionValue)
	{
		try
		{
			std::string strParamName = GetParameterName(iParamID);
			if (!strParamName.empty())
				return GetProgramOption(strParamName.c_str(),OptionValue);
		}
		catch(...){}
		LOGEMPTY << "Error while searching for parameter with id="<<iParamID;
		return false;
	}

	//////////////////////////////////////////////////////////////////////
	/**
	 * Modify value of the existent program option
	 * @param iParamID - in, id of the option to find
	 * @param OptionValue - in/out, value of the option if exists. If option doesn't exist
	 *     then input param remain unchanged
	 * @return - true if option was found in the list of command line params
	 */
	template <typename TValue>
	void CConfigurationModule::SetProgramOption(int iParamID, const TValue& OptionValue)
	{
		std::string strOptionName = GetParameterName(iParamID);
		try
		{
			if (m_poOptionsMap.count(strOptionName))
			{
				m_poOptionsMap.erase(strOptionName);
			}
			m_poOptionsMap.insert(std::make_pair(strOptionName,po::variable_value(OptionValue,false)));
			m_poOptionsMap.notify();
			CheckServerParameters(iParamID);
		}
		catch(const boost::bad_any_cast& ex)
		{
			LOGERROR << "Error while converting command line option, option=" << strOptionName<<", err message="<<ex.what();
		}
		catch(...)
		{
			LOGDEBUG << "Error while reading comamnd-line option, option name=" << strOptionName;
		}
	}



	//////////////////////////////////////////////////////////////////////
	/**
	 * Retrieves settings from internal storage.
	 * Setting parameter should be passed in as a reference.
	 * @param id - in, id of the setting to be checked and retrieved
	 * @param retValue - in/out, setting from the storage if exists
	 * @return - true if setting is in the storage, false otherwise
	 */
	template <typename TValue>
	bool CConfigurationModule::GetSetting(int id, TValue& retValue)
	{
		try
		{
			if ( id <= eCONFIG_UNDEFINED || id >= eCONFIG_COUNT)
			{
				LOGERROR << "Incorrect id: " << id;
				return false;
			}
			else
			{
				tIterSetting iter = m_SettingsStore.find(id);
				if ( iter != m_SettingsStore.end() )
				{
					retValue = boost::get<TValue>(iter->second);
					return true;
				}
				else
				{
					//try searching it in the config tree
					std::string strParamName = GetParameterName(id);
					if (!strParamName.empty() && m_treeConfig.count(strParamName))
					{
						GetParameterFromTree(strParamName, retValue);
						m_SettingsStore[id] = retValue;
						return true;
					}
				}
			}
		}
		CATCH
		return false;
	}

	//////////////////////////////////////////////////////////////////////
	/**
	 * Write setting to an internal container
	 * @param id - in, id of the setting to be stored
	 * @param val - in, value of the setting to be stored
	 */
	template <typename TValue>
	void CConfigurationModule::SetSetting(int id, const TValue& val)
	{
		try
		{
			if ( id <= eCONFIG_UNDEFINED || id >= eCONFIG_COUNT)
			{
				LOGERROR << "Incorrect id: " << id;
			}
			else
			{
				Setting setting = val;
				m_SettingsStore[id] = setting;

				//propagate this setting to config file if necessary
				std::string strParamName = GetParameterName(id);
				if (!strParamName.empty())
				{
					WriteParameterToTree(strParamName, val);
				}
			}
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Try to perform some data healing here in order to get rid of unpredictable
	 * and unexpected values in center of program
	 * @param iParamID - id of the parameter to look at the internal containers and verify
	 * 					the value is in place
	 * @param retValue -in/out, reference to the param value which will be read from the internal containers
	 * @return bool - true if value was found and passed back, false otherwise
	 */
	template <typename TValue>
	bool CConfigurationModule::CureParameter(int iParamID, TValue& retValue)
	{
		try
		{
			if (!GetProgramOption(iParamID,retValue) &&
				!GetSetting(iParamID,retValue)
				)
			{
				//print out error message only if parameter is present in the config file
				if (IsParameterPresent(iParamID))
				{
					std::string strErrorMsg("' option is incorrect (either in the command-line parameter or in the config file), default setting will be applied.");
					strErrorMsg.append("Please type '").append(SERVER_NAME).append(" --help' for more info.");
					strErrorMsg = "'" + GetParameterName(iParamID) + strErrorMsg;
					LOGFATAL << strErrorMsg;
				}
				return false;
			}
			return true;
		}
		CATCH_RETURN(false)
	}
	
	//////////////////////////////////////////////////////////////////////
	/*
	 * Maintain some pre-defined hardcoded queue with settings parameter. It can be used in order
	 * to perform some data healing in case of corrupted files/incorrect settings/etc
	 * @param iParamID - in, parameter id we need to cure
	 * @param retValue - in/out, reference to the the value of this parameter, if any is found
	 * @return - true if parameter was found in internal container and cured, false otherwise
	 */
	template <typename TValue>
	bool CConfigurationModule::GetDefaultValue(int iParamID, TValue& retValue)
	{
		try
		{
			tIterSetting iter = m_mapDefaultSettings.find(iParamID);
			if ( iter != m_mapDefaultSettings.end() )
			{
				retValue = boost::get<TValue>(iter->second);
			}
		}
		CATCH
		return false;
	}
	//////////////////////////////////////////////////////////////////////

} //namespace Config

#endif // #ifndef CONFIGURATIONMODULE_H_
