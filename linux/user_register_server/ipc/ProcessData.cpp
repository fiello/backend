
//native
#include "ProcessData.h"
#include "Logger.h"
//thirdparty
#include <boost/spirit/home/classic.hpp>
#include <boost/algorithm/string.hpp>

//////////////////////////////////////////////////////////////////////////

namespace IPC
{
	//////////////////////////////////////////////////////////////////////////
	using namespace boost::spirit;
	using namespace classic;
	using namespace std;
	using namespace trc;

	//////////////////////////////////////////////////////////////////////////
	struct add_section
	{
		add_section( RequestMessage & data ) : data_(data) {}
		void operator()(const char* p, const char* q) const
		{
			string s(p,q);
			boost::algorithm::trim(s);
			data_.push_back( RequestData( s, Entries() ) );
		}

		RequestMessage & data_;
	};

	//////////////////////////////////////////////////////////////////////////
	struct add_key
	{
		add_key( RequestMessage & data ) : data_(data) {}

		void operator()(const char* p, const char* q) const
		{
			string s(p,q);
			boost::algorithm::trim(s);
			data_.back().second.push_back( Entry( s, string() ) );
		}

		RequestMessage & data_;
	};

	//////////////////////////////////////////////////////////////////////////
	struct add_value
	{
		add_value( RequestMessage & data ) : data_(data) {}
		void operator()(const char* p, const char* q) const
		{
			data_.back().second.back().second.assign(p, q);
		}

		RequestMessage & data_;
	};

	//////////////////////////////////////////////////////////////////////////
	struct append_value
	{
		append_value( RequestMessage & data ) : data_(data) {}
		void operator()(const char* p, const char* q) const
		{
			data_.back().second.back().second.append(p, q);
		}

		void operator()(const char& ch) const
		{
			data_.back().second.back().second += ch;
		}

		RequestMessage & data_;
	};

	//////////////////////////////////////////////////////////////////////////
	struct requestdata_parser : public grammar<requestdata_parser>
	{
		requestdata_parser(RequestMessage & data) : data_(data) {}

		template <typename ScannerT>
		struct definition
		{
			rule<ScannerT> requestdata, section, entryUsr, entryMail, paramName,
						serviceData,  username, mail;
			rule<ScannerT> const& start() const { return requestdata; }

			definition(requestdata_parser const& self)
			{
				requestdata = *section;

				section = *blank_p
						>> serviceData[add_section(self.data_)]
						>> entryUsr
						>> *entryMail;

				entryUsr = paramName
						>> username[add_value(self.data_)]
						>> *blank_p
						>> *ch_p(';');

				entryMail = paramName
						>> mail[add_value(self.data_)]
						>> ch_p('@')[append_value(self.data_)]
						>> mail[append_value(self.data_)]
						>> *blank_p
						>> *ch_p(';');

				paramName = *blank_p
						>> serviceData[add_key(self.data_)]
						>> *blank_p
						>> ch_p('=')
						>> *blank_p;

				serviceData = +(alpha_p);
				username  = +(alnum_p | chset<>(" .") );
				mail = +(alnum_p | chset<>("-_.") );
			}
		};

		RequestMessage & data_;
	};

	//////////////////////////////////////////////////////////////////////////
	struct first_is
	{
		first_is(std::string const& s) : s_(s) {}
		template< class Pair >
		bool operator()(Pair const& p) const { return p.first == s_; }
		string const& s_;
	};

	//////////////////////////////////////////////////////////////////////////
	bool find_value( RequestMessage const& msg, string const& s, string const& p, string & res )
	{
		RequestMessage::const_iterator sit = find_if(msg.begin(), msg.end(), first_is(s));
		if (sit == msg.end())
			return false;

		Entries::const_iterator it = find_if(sit->second.begin(), sit->second.end(), first_is(p));
		if (it == sit->second.end())
			return false;

		res = it->second;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	CProcessData::CProcessData()
	{
		try
		{
			m_spParser.reset(new requestdata_parser(m_Data));
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	bool CProcessData::Parse(const std::string& strTargetString)
	{
		try
		{
			parse_info<> info = parse(strTargetString.c_str(), *m_spParser.get(), nothing_p);
			if (!info.hit)
			{
				LOGWARN << "Error parsing input string";
			}
			else
				return true;
		}
		CATCH
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CProcessData::GetValue(const char* szRequestType, const char* szRequestField, std::string& strReturnValue)
	{
		try
		{
			if (find_value(m_Data,szRequestType,szRequestField,strReturnValue) )
				return true;
			else
				return false;
		}
		CATCH
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
} /* namespace IPC */
