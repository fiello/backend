
#ifndef LOGGER_H_
#define LOGGER_H_

#define TRC_LOGGER trc::CLogger
#include "LogHelper.h"

namespace trc
{
	//////////////////////////////////////////////////////////////////////////
	/**
	 * CLogger class
	 * Auxiliary class to provide flexible and tunable console logging in the application
	 */
	class CLogger
	{
	public:
		static void Out(const char *szFunctionName, CErrorLevel::etLogLevel eErrorLevel, const char *szMsg);
		static std::string Format(const char *szFunctionName, CErrorLevel::etLogLevel eErrorLevel, const char *szMsg);
		static std::string StripFunctionName(const char *szFunctionName);
		static std::string GetDateTimeStamp(const char *szFormat);
		static void SetLevel(CErrorLevel::etLogLevel level) { m_eLogLevel = level; }
		static void SetDaemonMode(bool bIsDaemonModeOn) { m_bIsDaemonModeOn = bIsDaemonModeOn; }
		static void SetLogDir(const std::string& strLogDir);
	protected:
		static CErrorLevel::etLogLevel m_eLogLevel; // Current log level
		static bool m_bIsDaemonModeOn;
		static std::string m_strLogFilePath;
	};

	//////////////////////////////////////////////////////////////////////////
}

#endif /* LOGGER_H_ */
