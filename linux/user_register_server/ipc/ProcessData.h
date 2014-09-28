
#ifndef PROCESSDATA_H_
#define PROCESSDATA_H_

//thirdparty
#include <list>
#include <boost/smart_ptr/shared_ptr.hpp>

namespace IPC
{
	//////////////////////////////////////////////////////////////////////////
	typedef  std::pair<std::string, std::string>    Entry;
	typedef  std::list<Entry>            Entries;
	typedef  std::pair<std::string, Entries>   RequestData;
	typedef  std::list<RequestData>           RequestMessage;

	//////////////////////////////////////////////////////////////////////////
	struct requestdata_parser;


	class CProcessData
	{
	private: //permit no copy
		CProcessData(const CProcessData& src);
		CProcessData& operator= (const CProcessData& src);

	public: //methods
		CProcessData();
		virtual ~CProcessData(){;}

		bool Parse(const std::string& strTargetString);
		bool GetValue(const char* szRequestType, const char* szRequestField, std::string& strReturnValue);
		void Clean() { m_Data.clear(); }

	private: //members
		RequestMessage m_Data;
		boost::shared_ptr<requestdata_parser> m_spParser; //  Our parser
	};

} /* namespace IPC */
#endif /* PROCESSDATA_H_ */
