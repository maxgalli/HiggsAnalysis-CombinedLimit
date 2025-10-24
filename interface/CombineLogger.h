#ifndef HiggsAnalysis_CombinedLimit_CombineLogger_h
#define HiggsAnalysis_CombinedLimit_CombineLogger_h

#include <cstdlib>
#include <string>

#include "Logger.h"

class CombineLogger
{
	public: 
		static int nLogs;

		static CombineLogger& instance();
		
	static void setName(const char* _fName){
		fName=_fName;
		if (pL) {
			combine::logging::Logger::instance().setFileSink(fName, false);
		}
		::setenv("COMBINE_LOG_FILE", fName, 1);
	};

		void log(const std::string & _file, const int _lineN, const std::string& _logmsg, const std::string& _function);
		void printLog();
		void setVerbosity(combine::logging::Level level);
		combine::logging::Level verbosity() const;

	protected:
		// Static variable for the instance  
		static CombineLogger* pL;

		static const char*  fName;
		combine::logging::Level level_;
		CombineLogger();
		virtual ~CombineLogger();
};
#endif
