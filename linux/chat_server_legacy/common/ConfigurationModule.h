
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
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <map>

//////////////////////////////////////////////////////////////////////////
namespace Config
{
	using namespace trc;
	namespace po = boost::program_options;
	namespace pt = boost::property_tree;
	namespace ipc = boost::interprocess;

	enum etConfigParameter
	{
		eCONFIG_START = - 1,

		eCONFIG_DAEMON_MODE,
		eCONFIG_TCP_IF,
		eCONFIG_TCP_PORT,
		eCONFIG_LOG_LEVEL,

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

	private: // permit no copy, explicit ctors/dtors
		CConfigurationModule();
		CConfigurationModule(const CConfigurationModule& src);
		CConfigurationModule& operator= (const CConfigurationModule& src);
		~CConfigurationModule(){;}

	public: // methods
		static CConfigurationModule& Instance();
		void Destroy();

		// work with command line options
		bool ProcessServerOptions(int argc, char *argv[]);
		template <typename TValue>
		bool GetProgramOption(const char* szOptionName, TValue& OptionValue);
		template <typename TValue>
		bool GetProgramOption(int iParamID, TValue& OptionValue);

		// work with config file
		template <typename TValue>
		bool GetSetting(int id, TValue& retValue);
		template <typename TValue>
		void SetSetting(int id, const TValue& val);
		void WriteSettingsToFile();
		void SetLogLevel();
		static std::string GetStartPath() { return m_strInitialStartDir; }

	private: //members
		static CConfigurationModule* m_pSelf;
		static ipc::interprocess_mutex m_ipcAccess;
		static std::string m_strInitialStartDir;

		tMapSetting m_SettingsStore; // settings store
		tMapSetting m_mapDefaultSettings; // data that can be cured as default
		tMapParams m_mapParamNames; // string name to the Setting parameter

		po::variables_map m_poOptionsMap;	//options that were specified in the command line
		pt::ptree m_treeConfig;
		std::string m_strConfigFile; //absolute path to the server config file

	private: // methods
		// Init various path settings, create config paths if needed, etc
		bool IsConfigFilePresent();
		void ReadSettingsFromFile();
		void InitParamMap();
		std::string GetParameterName(int iParamID);

		template <typename TValue>
		bool IsParameterCurable(int iParamID, TValue& retValue);

		// Set of overloaded functions to get setting from config file based on its name
		void GetParameterFromTree(const std::string& strParamName, int& ulValue);
		void GetParameterFromTree(const std::string& strParamName, std::string& strValue);

		// Set of overloaded functions to write setting to config file based on its name
		void WriteParameterToTree(const std::string& strParamName, int ulValue);
		void WriteParameterToTree(const std::string& strParamName, std::string& strValue);
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
					OptionValue = m_poOptionsMap[szOptionName].as<TValue>();
				return true;
			}
		}
		CATCH
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
		CATCH
		LOGERROR << "Unable to get parameter with the id = "<<iParamID;
		return false;
	}

	//////////////////////////////////////////////////////////////////////
	/**
	 * Retrieves settings from internal storage, config file or Client Platform's config manager.
	 * Setting parameter should be passed in as a reference.
	 * @param id - in, id of the setting to be checked and retrieved
	 * @param setting - in/out, setting from the storage if exists
	 * @return - true if setting is in the storage, false otherwise
	 */
	template <typename TValue>
	bool CConfigurationModule::GetSetting(int id, TValue& retValue)
	{
		try
		{
			if ( id <= eCONFIG_START || id >= eCONFIG_COUNT)
			{
				LOGERROR << "Incorrect id: " << id;
				return false;
			}
			else
			{
				tIterSetting iter = m_SettingsStore.find(id);
				if ( iter != m_SettingsStore.end() )
					retValue = boost::get<TValue>(iter->second);
				else
				{
					//try searching it in the config tree
					std::string strParamName = GetParameterName(id);
					if (!strParamName.empty() && m_treeConfig.count(strParamName))
					{
						GetParameterFromTree(strParamName, retValue);
						m_SettingsStore[id] = retValue;
					}
					else if (!IsParameterCurable(id,retValue))
					{
						LOGDEBUG << "Parameter not found, id: " << id;
						throw RC_FAILED;
					}
				}
				return true;
			}
		}
		CATCH_RETURN(false)
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
			if ( id <= eCONFIG_START || id >= eCONFIG_COUNT)
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
	 * Maintain some pre-defined hardcoded queue with settins parameter. It can be used in order
	 * to perform some data healing in case of corrupted files/incorrect settings/etc
	 * @param iParamID - in, parameter id we need to cure
	 * @param retValue - in/out, reference to the the value of this parameter, if any is found
	 */
	template <typename TValue>
	bool CConfigurationModule::IsParameterCurable(int iParamID, TValue& retValue)
	{
		try
		{
			tIterSetting iter = m_mapDefaultSettings.find(iParamID);
			if ( iter != m_mapDefaultSettings.end() )
			{
				retValue = boost::get<TValue>(iter->second);

				//ok, this parameter seems to be curable, let's
				//capture it and write to the tree for future needs
				std::string strParamName = GetParameterName(iParamID);
				if (!strParamName.empty())
				{
					WriteParameterToTree(strParamName,retValue);
					return true;
				}
			}
		}
		CATCH
		return false;
	}
	//////////////////////////////////////////////////////////////////////

} //namespace Config

#endif // #ifndef CONFIGURATIONMODULE_H_
