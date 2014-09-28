
//native
#include "Logger.h"
#include "CompiledDefinitions.h"
//thirdparty
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <stdio.h>
#include <boost/thread/mutex.hpp>

using namespace std;
boost::mutex g_Access;

//////////////////////////////////////////////////////////////////////////

namespace trc
{
	CErrorLevel::etLogLevel CLogger::m_eLogLevel = CErrorLevel::Warn; // default log level
	bool CLogger::m_bIsDaemonModeOn = false;
	std::string CLogger::m_strLogFilePath = "";
	#define TRC_FILE_NAME SERVER_NAME
	#define TRC_FILE_EXT ".log"
	#define DELIMER "\t"

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Retireves date and timestamp in the format requested
	 * @param szFormat - in, format of the timestamp required
	 * @return std::string - resulting string with the timestamp
	 */
	std::string CLogger::GetDateTimeStamp(const char *szFormat)
	{
		char buffer[80];
		try
		{
			time_t rawtime;
			struct tm * timeinfo;

			time (&rawtime);
			timeinfo = localtime(&rawtime);
			strftime (buffer, 80, szFormat, timeinfo);
		}
		catch(...){}
		return buffer;
	}

	////////////////////////////////////////////////////////////////////
	/**
	 * Removes unnecessary parts from function signature (return value/params)
	 * @param szFunctionName - in, function signature to be corrected
	 * @return std::string - corrected string name
	 */
	std::string CLogger::StripFunctionName(const char *szFunctionName)
	{
		std::string strFName("");
		try
		{
			strFName.assign(szFunctionName);
			size_t pos = strFName.rfind('(');
			if(pos != std::string::npos)
			{
				strFName = strFName.substr(0,pos);
				pos = strFName.rfind(' ');
				if(pos != std::string::npos)
				{
					strFName = strFName.substr(pos+1);
				}
			}
		}
		catch(...){}
		return strFName;
	}

	////////////////////////////////////////////////////////////////////
	/**
	 * Formats the messag according to the error level
	 * @param szFunctionName - in, function signature
	 * @param eErrorLevel - in, error (log) level of the message
	 * @param szMsg - in, the message itself
	 * @return std::string - complete string with all parts joined together
	 */
	std::string CLogger::Format(const char *szFunctionName, CErrorLevel::etLogLevel eErrorLevel, const char *szMsg)
	{
		if (eErrorLevel >= CErrorLevel::Fatal_Safe)
			return szMsg;

		stringstream ss;
		try
		{
			ss << GetDateTimeStamp("%X %x") << DELIMER;
			ss << CErrorLevel::GetName(eErrorLevel) << DELIMER;
			if(szFunctionName)
				ss << StripFunctionName(szFunctionName) << DELIMER;
			ss << szMsg;
		}
		catch(...){}

		return ss.str();
	}

	////////////////////////////////////////////////////////////////////
	void CLogger::SetLogDir(const std::string& strLogDir)
	{
		try
		{
			m_strLogFilePath = strLogDir;
			if (m_strLogFilePath.at(m_strLogFilePath.length()-1) != '/')
				m_strLogFilePath += '/';
			m_strLogFilePath.append(TRC_FILE_NAME).append(TRC_FILE_EXT);
		}
		catch(...){}
	}

	////////////////////////////////////////////////////////////////////
	/**
	 * Prints out composed log message to the standard output (or error output)
	 * @param szFunctionName - in, function signature
	 * @param eErrorLevel - in, error (log) level of the message
	 * @param szMsg - in, the message itself
	 */
	void CLogger::Out(const char *szFunctionName, CErrorLevel::etLogLevel eErrorLevel, const char *szMsg)
	{
		try
		{
			if(eErrorLevel < m_eLogLevel)
				return;

			string StrOut = Format(szFunctionName, eErrorLevel, szMsg);

			if (!m_bIsDaemonModeOn)
			{
				//no console print out in daemon mode since descriptors are closed
				if(eErrorLevel == CErrorLevel::Debug || eErrorLevel == CErrorLevel::None)
				{
					std::cout << StrOut << std::endl;
				}
				else
					std::cerr << StrOut << std::endl;
			}

			if (m_strLogFilePath.empty())
				return;

			boost::mutex::scoped_lock lock(g_Access);
			std::ofstream file(m_strLogFilePath, std::ios_base::app);
			if(file.good())
				file << StrOut << std::endl;
		}
		catch(...){}
	}

	////////////////////////////////////////////////////////////////////

} //namespace trc
