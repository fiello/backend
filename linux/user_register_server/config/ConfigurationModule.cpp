
// native
#include "ConfigurationModule.h"
#include "IPCModule.h"
#include "IniParser.h"
// third-party
#include <iostream>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <unistd.h>
#include <sys/types.h>

namespace Config
{
	//////////////////////////////////////////////////////////////////////////
	CConfigurationModule* CConfigurationModule::m_pSelf = NULL;
	boost::mutex CConfigurationModule::m_configAccess;
	std::string CConfigurationModule::m_strInitialStartDir = "";
	namespace bfs = boost::filesystem;

	static const char* g_strParameterNames[] =
	{
		"daemon",
		"tcp_if",
		"tcp_port",
		"udp_if",
		"udp_port",
		"maint",
		"datafile",
		"sleep",
		"loglevel",
		"kill",
		"threadpool"
	};

	//min size of the thread pool (min number of worker threads)
	const size_t g_iMinPoolSize = 2;
	//max size of the thread pool (max number of worker threads)
	const size_t g_iMaxPoolSize = 20;
	//hardcode max IP port
	const int g_iMaxIPport = 65535;

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Constructor of configuration module
	 * Identifies and saves config paths and autostart
	 */
	CConfigurationModule::CConfigurationModule():
		m_strConfigFile(""),
		m_bIsFirstLaunch(false)
	{
		try
		{
			InitParamMap();
		}
		catch(...)
		{
			LOGFATAL << "Unable to start configuration module.";
			throw false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Method to access singleton instance
	 * @return CConfigurationModule& - reference to the singleton object
	 */
	CConfigurationModule& CConfigurationModule::Instance()
	{
		try
		{
			if ( m_pSelf == NULL )
			{
				boost::lock_guard<boost::mutex> lock(m_configAccess);
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
			m_pSelf = NULL;
			delete m_pSelf;
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Propagate and validate options received from the command line
	 * @param bIsFirstLaunch - in, flag whether this is the first launch
	 * @param argc - in, number of parameters to parse
	 * @param argv - in, input options to parse
	 * @return - false if some vital settings are missed or user was just checking
	 * 		some basic options like help. True is is returned otherwise.
	 */
	bool CConfigurationModule::ProcessServerOptions(bool bIsFirstLaunch, int argc, char *argv[])
	{
		try
		{
			m_bIsFirstLaunch = bIsFirstLaunch;
			if (bIsFirstLaunch)
			{
				//work with config file only if this is the first launch of application. Otherwise the only aim
				//of this process will be to pass command line options to the existing process
				//compose absolete path to the config file
				std::string strPathFile = GetExecutablePath();
				bfs::path pathCurrentDirectory = bfs::system_complete(bfs::path(strPathFile));
				m_strConfigFile = pathCurrentDirectory.branch_path().string();
				if ( m_strConfigFile.at(m_strConfigFile.length()-1) != '/')
					m_strConfigFile += '/';
				LOG_SETLOG_DIR(m_strConfigFile);
				m_strConfigFile += CONFIG_FILE;
				if (!bfs::exists(m_strConfigFile))
				{
					LOGFATAL << "Configuration file is missed: " << m_strConfigFile;
					return false;
				}

				//from now on let's decide which settings we have and what are supressed by the command line options
				ReadSettingsFromFile();
			}

			// preparing and parsing command line options
			po::options_description hidden("Advanced options");
			po::options_description generic("Generic options");
			po::options_description config("Configuration options");
			po::options_description cmd;
			po::options_description visible("Allowed options for the '" + std::string(SERVER_NAME) + "'");
			generic.add_options()
				("help","produce help message")
				("version","print server version");
			config.add_options()
				("datafile",po::value<std::string>(),"specify file with user data (default is data.txt). This option is ignored in non-maintenance mode")
				("sleep",po::value<int>(),"amount of time before responding to client (0..xxxx in milliseconds, default is 1000)")
				("maint",po::value<int>(),"switch server to maintenance mode")
				("loglevel",po::value<int>(),"specify server log level (0=Debug, 1=Warning, 2=Error, 3=Fatal)")
				("kill","terminate instance of process if any is running in daemon mode")
				("daemon","run process in daemon mode");
			hidden.add_options()
				("cmd","print this extended help message")
				("threadpool",po::value<int>(),"specify maximum number of threads available in thread pool");

			cmd.add(generic).add(config).add(hidden);
			visible.add(generic).add(config);
			try
			{
				po::store(po::parse_command_line(argc,argv,cmd), m_poOptionsMap);
				m_poOptionsMap.notify();
			}
			catch( const std::exception &ex)
			{
				LOGERROR << "Error while parsing input options: " << ex.what();
				m_poOptionsMap.clear();
			}

			//print help message
			if (m_poOptionsMap.count("help"))
			{
				LOGEMPTY << visible;
				return false;
			}

			//print version of the program
			if (m_poOptionsMap.count("version"))
			{
				LOGEMPTY << "'" << SERVER_PRODUCT_NAME << "' product version: " << SERVER_VERSION;
				return false;
			}

			//print all the options including advanced
			if (m_poOptionsMap.count("cmd"))
			{
				LOGEMPTY << cmd;
				return false;
			}

			//force process to be launched as a daemon
			if (m_poOptionsMap.count("daemon"))
			{
				if (bIsFirstLaunch)
				{
					int iValue = 1;
					SetProgramOption(eCONFIG_DAEMON_MODE,iValue);
				}
				else
					return false;
			}

			//attempt will be made to open an IPC queue and post a special message there, so that
			//remote process (if any) will read it and stops running
			if (m_poOptionsMap.count("kill"))
			{
				using namespace boost::interprocess;
				int iSignal = eCONFIG_KILL_PROCESS;
				try
				{
					message_queue mqueue(open_only,SERVER_MSG_QUEUE);
					mqueue.send(&iSignal,sizeof(iSignal),1);
				}
				catch(const interprocess_exception& ex)
				{
					//no queue means another process is dead already or we have no permissions - skip silently
				}
				return false;
			}

			//final parameters validation and storing into internal containers
			if (bIsFirstLaunch)
			{
				for (int iParamIndex = eCONFIG_DAEMON_MODE; iParamIndex < eCONFIG_COUNT; ++iParamIndex)
				{
					CheckServerParameters(iParamIndex, bIsFirstLaunch);
				}
			}

			return true;
		}
		CATCH
		return false;
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
			LOGFATAL << "Exception while reading setting file: " << ex.what();
		}
		catch (...)
		{
			LOGFATAL << "Exception while reading setting file";
		}
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Method to check if config file is present
	 * @return bool - true if config file is in place and can be read by the server, false otherwise
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
			for (int i = eCONFIG_DAEMON_MODE; i < eCONFIG_COUNT; ++i )
			{
				m_mapParamNames[i].assign(g_strParameterNames[i-eCONFIG_DAEMON_MODE]);
			}

			//fulfill some data for curable params
			m_mapDefaultSettings[eCONFIG_TCP_IF] = std::string("127.0.0.1");
			m_mapDefaultSettings[eCONFIG_TCP_PORT] = 0;
			m_mapDefaultSettings[eCONFIG_UDP_IF] = std::string("127.0.0.1");
			m_mapDefaultSettings[eCONFIG_UDP_PORT] = 0;
			m_mapDefaultSettings[eCONFIG_DATA_FILE] = std::string("data.txt");
			m_mapDefaultSettings[eCONFIG_SLEEP] = 1000;
			m_mapDefaultSettings[eCONFIG_MAINT] = 0;
			m_mapDefaultSettings[eCONFIG_LOG_LEVEL] = 2;
			m_mapDefaultSettings[eCONFIG_THREAD_POOL] = 10;
		}
		catch(...)
		{
			LOGFATAL << "Unable to setup parameter map";
			throw false;
		}
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Linux-specific function to get full absolute directory path to the binary file being executed
	 * @return std::string - string with the path to current binary, empty string if something failed
	 * during the path retrieval
	 *
	 */
	std::string CConfigurationModule::GetExecutablePath()
	{
		std::string strTemp("");
		try
		{
			int iPathChunk = 512;
			int iBytesRead = iPathChunk;

			pid_t pid = getpid();
			std::string symPath = "/proc/";
			std::ostringstream ss;
			ss << pid;
			symPath.append(ss.str()).append("/exe");
			bool bExit = false;

			while (!bExit)
			{
				boost::scoped_array<char> strBuf(new char[iPathChunk]);
				memset(strBuf.get(),0,iPathChunk);
				iBytesRead = readlink(symPath.c_str(),strBuf.get(),iPathChunk);
				if (iBytesRead == iPathChunk)
				{
					//size is not enough - let's double path chunk
					iPathChunk += iPathChunk;
				}
				else
				{
					bExit = true;
					strTemp.assign(strBuf.get());
				}
			}
		}
		CATCH
		return strTemp;
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
	 * Function to check, validate and then partially apply parameters from internal containers.
	 * It perform data healing and is being executed in two cases:
	 *  - during the initial launch of application
	 *  - when accepting commands from another process
	 * @param iParamIndex - index of the param to be validated
	 * @param bIsFirstLaunch - indicator of the first launch of application
	 */
	void CConfigurationModule::CheckServerParameters(int iParamIndex, bool bIsFirstLaunch)
	{
		try
		{
			IPC::CIPCModule& ipcModule = IPC::CIPCModule::Instance();
			//Let's find out if anything usefull came from the command line
			switch (iParamIndex)
			{
				case eCONFIG_TCP_PORT:
				case eCONFIG_UDP_PORT:
				{
					int iValue = 0;
					int iDefaultValue = 0;
					GetDefaultValue<int>(iParamIndex,iDefaultValue);
					if (!CureParameter<int>(iParamIndex,iValue) || iValue <= 0 || iValue > g_iMaxIPport)
						iValue = iDefaultValue;

					SetSetting<int>(iParamIndex,iValue);
					break;
				}
				case eCONFIG_KILL_PROCESS:
					break;
				case eCONFIG_TCP_IF:
				case eCONFIG_UDP_IF:
				case eCONFIG_DATA_FILE:
				{
					std::string strValue("");
					std::string strDefaultValue("");
					GetDefaultValue<std::string>(iParamIndex,strDefaultValue);
					if (!CureParameter<std::string>(iParamIndex,strValue))
						strValue = strDefaultValue;

					if (!strValue.empty() && (iParamIndex == eCONFIG_DATA_FILE) && (strValue.at(0) != '/'))
					{
						//assume that this is not an absolute path - build relative string
						//based on current directory structure
						std::string strPath = GetExecutablePath();
						bfs::path pathCurrentDirectory = bfs::system_complete(bfs::path(strPath));
						strPath = pathCurrentDirectory.branch_path().string();
						if (strPath.at(strPath.length()-1) != '/')
							strPath += "/";
						strValue = strPath + strValue;
					}

					SetSetting<std::string>(iParamIndex,strValue);
					if (iParamIndex == eCONFIG_DATA_FILE && !bIsFirstLaunch )
					{
						//permit changing data file path only in maintenance mode
						int iMaintenanceMode;
						GetProgramOption(eCONFIG_MAINT,iMaintenanceMode);
						if (iMaintenanceMode)
							ipcModule.SetDataPath(strValue);
					}

					break;
				}
				case eCONFIG_DAEMON_MODE:
				case eCONFIG_MAINT:
				{
					int iValue = 0;
					int iDefaultValue = 0;
					GetDefaultValue<int>(iParamIndex,iDefaultValue);
					if (!CureParameter<int>(iParamIndex,iValue) || iValue > 1 || iValue < 0)
						iValue = iDefaultValue;

					SetSetting<int>(iParamIndex,iValue);

					if (iParamIndex == eCONFIG_MAINT && !bIsFirstLaunch)
					{
						ipcModule.SetMaintenanceMode(iValue);
					}

					break;
				}
				case eCONFIG_SLEEP:
				{
					int iValue = 0;
					int iDefaultValue = 0;
					GetDefaultValue<int>(iParamIndex,iDefaultValue);
					if (!CureParameter<int>(iParamIndex,iValue) || iValue > 9999 || iValue < 0)
						iValue = iDefaultValue;

					SetSetting<int>(iParamIndex,iValue);
					if (!bIsFirstLaunch)
					{
						ipcModule.SetSendTimeout(iValue);
					}
					break;
				}
				case eCONFIG_LOG_LEVEL:
				{
					int iValue = 0;
					int iDefaultValue = 0;
					GetDefaultValue<int>(iParamIndex,iDefaultValue);
					if (!CureParameter<int>(iParamIndex,iValue) || iValue > CErrorLevel::Fatal || iValue < CErrorLevel::Debug)
						iValue = iDefaultValue;

					SetSetting<int>(iParamIndex,iValue);
					LOG_SETLEVEL(static_cast<CErrorLevel::etLogLevel>(iValue));
					break;
				}
				case eCONFIG_THREAD_POOL:
				{
					int iValue = 0;
					int iDefaultValue = 0;
					GetDefaultValue<int>(iParamIndex,iDefaultValue);
					if (!CureParameter<int>(iParamIndex,iValue) || iValue > (int)g_iMaxPoolSize || iValue < (int)g_iMinPoolSize)
						iValue = iDefaultValue;

					SetSetting<int>(iParamIndex,iValue);
					break;
				}
				default:
					LOGERROR << "Unknown parameter id while checking parameters: "<<iParamIndex;
			}
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Check if parameter with the given parameter name is present in the program options (command
	 * line arguments passed to the program during the launch).
	 * @param strParamName - in, string with the parameter name
	 * @return - true if parameter was found in the container, false otherwise
	 */
	bool CConfigurationModule::IsParameterPresent(const std::string& strParamName)
	{
		try
		{
			if (m_poOptionsMap.count(strParamName))
				return true;
		}
		CATCH
		return false;
	}

	//////////////////////////////////////////////////////////////////////
	/*
	 * Overloaded function to find parameter by it's ID
	 * @param iParamID - in, ID of the parameter to be found
	 * @return - true if parameter was found in internal container, false otherwise
	 */
	bool CConfigurationModule::IsParameterPresent(int iParamID)
	{
		try
		{
			std::string strParamName = GetParameterName(iParamID);
			if (!strParamName.empty())
				return IsParameterPresent(strParamName.c_str());
		}
		CATCH
		return false;
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
	void CConfigurationModule::WriteParameterToTree(const std::string& strParamName, const std::string& strValue)
	{
		try
		{
			m_treeConfig.put(strParamName,strValue);
		}
		CATCH_THROW()
	}
	//////////////////////////////////////////////////////////////////////

 } // namespace Config
