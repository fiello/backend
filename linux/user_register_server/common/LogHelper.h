
#ifndef LOG_HELPER_H_
#define LOG_HELPER_H_

//thirdparty
#include <sstream>
#include "boost/scoped_array.hpp"
#include <string.h>
#include <stdio.h>

namespace trc
{
	//////////////////////////////////////////////////////////////////////////
	/**
	 * CErrorLevel class
	 * Auxiliary class to provide configurable error level
	 */
	class CErrorLevel
	{
	public:
		typedef enum
		{
			Debug = 0,
			Warn,
			Error,
			Fatal,
			Fatal_Safe,
			None
		} etLogLevel;

		static std::string GetName(etLogLevel eLevel)
		{
			switch(eLevel)
			{
				case Debug:
					return "Debug";
				case Warn:
					return "Warn";
				case Error:
					return "Error";
				case Fatal:
				case Fatal_Safe:
					return "Fatal";
				default:
					return "";
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////
	/**
	 * CLogHelper class
	 * Auxiliary template class, used as a temporary object in each log message
	 */
	template<class TLogger>
	class CLogHelper
	{

	public:
		template <typename T>
		CLogHelper& operator<<(T const &t)
		{
			m_ossBuffer << t;
			return *this;
		}

		CLogHelper& operator<<(const wchar_t *wszIn)
		{
			boost::scoped_array<char> saBuf(new char[wcslen(wszIn)+1]);
			sprintf(saBuf.get(), "%ls", wszIn);
			m_ossBuffer << saBuf.get();
			return *this;
		}

		CLogHelper& operator<<(wchar_t *wszIn)
		{
			*this << static_cast<const wchar_t *>(wszIn);
			return *this;
		}

		CLogHelper& operator<<(CErrorLevel::etLogLevel eErrorLevel)
		{
			m_eErrorLevel = eErrorLevel;
			return *this;
		}

		CLogHelper& operator<<(std::ostream&(*fn)(std::ostream&))
		{
			fn(m_ossBuffer);
			return *this;
		}

		static void SetLevel(CErrorLevel::etLogLevel _eLogLevel)
		{
			TLogger::SetLevel(_eLogLevel);
		}

		static void SetDaemonMode(bool bIsDaemonModeOn)
		{
			TLogger::SetDaemonMode(bIsDaemonModeOn);
		}

		static void SetLogDir(const std::string& strLogPath)
		{
			TLogger::SetLogDir(strLogPath);
		}


		CLogHelper(const char * szFunctionName = NULL): m_eErrorLevel(CErrorLevel::Error),
				m_szFunctionName(szFunctionName)
		{
		}

		virtual ~CLogHelper()
		{
			TLogger::Out(m_szFunctionName, m_eErrorLevel, m_ossBuffer.str().c_str());
		}

	private:
		CLogHelper(const CLogHelper &src);
		const CLogHelper& operator=(const CLogHelper &src);
		CErrorLevel::etLogLevel m_eErrorLevel;
		std::ostringstream m_ossBuffer;
		const char *m_szFunctionName;
	};

	//////////////////////////////////////////////////////////////////////////

	#define LOGMSG(...) \
		trc::CLogHelper<TRC_LOGGER>(__VA_ARGS__)

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Defines for common log messages to be used in application
	 */
	#define LOGDEBUG \
			LOGMSG(__PRETTY_FUNCTION__) << trc::CErrorLevel::Debug

	#define LOGWARN \
			LOGMSG(__PRETTY_FUNCTION__) << trc::CErrorLevel::Warn

	#define LOGERROR \
			LOGMSG(__PRETTY_FUNCTION__) << trc::CErrorLevel::Error

	#define LOGFATAL \
			LOGMSG(__PRETTY_FUNCTION__) << trc::CErrorLevel::Fatal

	#define LOGFATAL_SAFE \
			LOGMSG(__PRETTY_FUNCTION__) << trc::CErrorLevel::Fatal_Safe

	#define LOGEMPTY \
			LOGMSG(__PRETTY_FUNCTION__) << trc::CErrorLevel::None

	#define LOG_SETLEVEL(l) \
			trc::CLogHelper<TRC_LOGGER>::SetLevel(l)

	#define LOG_SETDAEMON_MODE(mode) \
			trc::CLogHelper<TRC_LOGGER>::SetDaemonMode(mode)

	#define LOG_SETLOG_DIR(dir) \
			trc::CLogHelper<TRC_LOGGER>::SetLogDir(dir)

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Defines for common catch blocks to be used in application
	 */
	#define CATCH_RETURN(Exec) \
		catch(...)\
		{\
			LOGERROR << "Unexpected exception";\
			return Exec; \
		}

	#define CATCH_THROW(Exec) \
		catch(...)\
		{\
			LOGERROR << "Unexpected exception";\
			throw Exec; \
		}

	#define CATCH \
		catch(...)\
		{\
			LOGERROR << "Unexpected exception";\
		}

	/////////////////////////////////////////////////////////////////////////

} //namespace trc

#endif // #ifndef LOG_HELPER_H_

