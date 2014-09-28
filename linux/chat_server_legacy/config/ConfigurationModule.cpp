
// native
#include "ConfigurationModule.h"
// third-party
#include <iostream>
#include "IniParser.h"
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

namespace Config
{
	//////////////////////////////////////////////////////////////////////////
	CConfigurationModule* CConfigurationModule::m_pSelf = NULL;
	ipc::interprocess_mutex CConfigurationModule::m_ipcAccess;
	std::string CConfigurationModule::m_strInitialStartDir = "";
	namespace bfs = boost::filesystem;

	static const char* gParameterName[] =
	{
		"daemon",
		"tcp_if",
		"tcp_port",
		"loglevel"
	};

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Constructor of configuration module
	 * Identifies and saves config paths and autostart
	 */
	CConfigurationModule::CConfigurationModule():
		m_strConfigFile("")
	{
		try
		{
			// Setting start-up log-level
			LOG_SETLEVEL(CErrorLevel::Warn);
			InitParamMap();
		}
		catch(...)
		{
			LOGFATAL << "Unable to start configuration module";
		}
	}

	//////////////////////////////////////////////////////////////////////
	/**
	 * Propagate setting for log level from storage to the logger
	 */
	void CConfigurationModule::SetLogLevel()
	{
		try
		{
			std::string strErrorMsg("Config file has corrupted data (loglevel), default setting will be applied. ");
			strErrorMsg.append("Please type '").append(SERVER_NAME).
					append(" --help' for more info.");
			int level;
			if (GetSetting<int>(eCONFIG_LOG_LEVEL, level))
			{
				//perform some data healing here - level should be definitely limited
				if (level > CErrorLevel::Fatal || level < CErrorLevel::Debug)
				{
					LOGERROR << strErrorMsg;
					level = CErrorLevel::Warn;
				}
				LOG_SETLEVEL(static_cast<CErrorLevel::etLogLevel>(level));
				return;
			}
			LOGERROR << strErrorMsg;
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Method to access singleton instance
	 */
	CConfigurationModule& CConfigurationModule::Instance()
	{
		try
		{
			if ( m_pSelf == NULL )
			{
				ipc::scoped_lock<ipc::interprocess_mutex> lock(m_ipcAccess);
				if ( m_pSelf == NULL )
				{
					m_pSelf = new CConfigurationModule();
				}
			}
		}
		CATCH
		return *m_pSelf;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Method to destroy singleton explicitly
	 */
	void CConfigurationModule::Destroy()
	{
		try
		{
			ipc::scoped_lock<ipc::interprocess_mutex> lock(m_ipcAccess);
			delete m_pSelf;
			m_pSelf = NULL;
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Propagate and validate options received from the command line
	 * @param argc - in, number of paramters to parse
	 * @param argv - in, input options to parse
	 */
	bool CConfigurationModule::ProcessServerOptions(int argc, char *argv[])
	{
		try
		{
			//compose absolute path to the config file
			bfs::path fullServerPath(bfs::initial_path<bfs::path>());
			m_strInitialStartDir = fullServerPath.string();
			LOG_SETLOG_DIR(m_strInitialStartDir);
			fullServerPath = bfs::system_complete(bfs::path(argv[0]));
			m_strConfigFile = fullServerPath.branch_path().string();
			if (m_strConfigFile.at(m_strConfigFile.length()-1) != '/')
			{
				m_strConfigFile += "/";
			}
			m_strConfigFile += CONFIG_FILE;

			if (!bfs::exists(m_strConfigFile))
			{
				LOGEMPTY << "Fatal: configuration file is missed: " << CONFIG_FILE;
				return false;
			}

			// preparing and parsing command line options
			po::options_description hidden("Hidden options");
			po::options_description generic("Generic options");
			po::options_description config("Configuration options");
			po::options_description cmd;
			po::options_description visible("Allowed options for the '" + std::string(SERVER_NAME) + "'");
			generic.add_options()
				("help","produce help message")
				("version","print server version");

			cmd.add(generic).add(config).add(hidden);
			visible.add(generic).add(config);
			try
			{
				po::store(po::parse_command_line(argc,argv,cmd), m_poOptionsMap);
				m_poOptionsMap.notify();
			}
			catch( const std::exception &ex)
			{
				LOGEMPTY << "Error while parsing input options: " << ex.what();
				return false;
			}

			if (m_poOptionsMap.count("help"))
			{
				LOGEMPTY << visible;
				return false;
			}

			if (m_poOptionsMap.count("version"))
			{
				LOGEMPTY << "'" << SERVER_PRODUCT_NAME << "' product version: " << SERVER_VERSION;
				return false;
			}

			ReadSettingsFromFile();
			SetLogLevel();

			return true;
		}
		CATCH
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Attempt to write settings from internal container to user config file
	 */
	void CConfigurationModule::WriteSettingsToFile()
	{
		try
		{
			if (!IsConfigFilePresent())
			{
				//attempt to re-create config file
				std::ofstream fileConfig(m_strConfigFile,std::ios_base::out);
				if (!fileConfig)
				{
					LOGERROR << "Unable to create config file: " << m_strConfigFile;
					return;
				}
			}
			pt::ini_parser::write_ini(m_strConfigFile, m_treeConfig);
		}
		catch (const std::exception& ex )
		{
			LOGERROR << "Error when writing to config file, message: " << ex.what();
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Attempt to read user config file and then store these settings in the
	 * internal container
	 */
	void CConfigurationModule::ReadSettingsFromFile()
	{
		try
		{
			if (IsConfigFilePresent())
			{
				pt::ini_parser::read_ini(m_strConfigFile, m_treeConfig);
			}
		}
		catch (const std::exception& ex )
		{
			LOGERROR << "Exception, message: " << ex.what();
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Method to check if config file is present
	 */
	bool CConfigurationModule::IsConfigFilePresent()
	{
		try
		{
			if (boost::filesystem::exists(m_strConfigFile))
					return true;
		}
		CATCH
		return false;
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Init map with parameters' names
	 */
	void CConfigurationModule::InitParamMap()
	{
		try
		{
			for (int i = eCONFIG_DAEMON_MODE; i <= eCONFIG_LOG_LEVEL; ++i )
			{
				m_mapParamNames[i].assign(gParameterName[i-eCONFIG_DAEMON_MODE]);
			}

			//fulfil some data for curable params
			m_mapDefaultSettings[eCONFIG_DAEMON_MODE] = 0;
			m_mapDefaultSettings[eCONFIG_LOG_LEVEL] = 1;
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Convert Setting id to the string name
	 * @param paramID - in, id of the parameter to be retrieved
	 * @return - string name of the settings. In case if parameter is not found then
	 * return string is empty
	 */
	std::string CConfigurationModule::GetParameterName(int iParamID)
	{
		std::string strParamName("");
		try
		{
			tIterParams iter = m_mapParamNames.find(iParamID);
			if ( iter != m_mapParamNames.end() )
			{
				strParamName = iter->second;
			}
		}
		CATCH
		return strParamName;
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Get interger-type parameter from internal tree by its name
	 * @param strParamName - in, parameter name
	 * @param iValue - in/out, reference to variable to store param value, if any is found
	 */
	void CConfigurationModule::GetParameterFromTree(const std::string& strParamName, int& iValue)
	{
		try
		{
			iValue = m_treeConfig.get<int>(strParamName);
		}
		CATCH_THROW()
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Get string-type parameter from internal tree by its name
	 * @param strParamName - in, parameter name
	 * @param strValue - in/out, reference to variable to store param value, if any is found
	 */
	void CConfigurationModule::GetParameterFromTree(const std::string& strParamName, std::string& strValue)
	{
		try
		{
			strValue = m_treeConfig.get<std::string>(strParamName);
		}
		CATCH_THROW()
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Write integer-type parameter to internal tree by its name
	 * @param strParamName - in, parameter name
	 * @param iValue - in, parameter value
	 */
	void CConfigurationModule::WriteParameterToTree(const std::string& strParamName, int iValue)
	{
		try
		{
			std::ostringstream stream;
			stream << iValue;
			m_treeConfig.put(strParamName,stream.str());
		}
		CATCH_THROW()
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Write string-type parameter to internal tree by its name
	 * @param strParamName - in, parameter name
	 * @param strValue - in, parameter value
	 */
	void CConfigurationModule::WriteParameterToTree(const std::string& strParamName, std::string& strValue)
	{
		try
		{
			m_treeConfig.put(strParamName,strValue);
		}
		CATCH_THROW()
	}
	//////////////////////////////////////////////////////////////////////

 } // namespace Config
