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
			fName = _fName;
			if (pL && pL->fileSinkEnabled_) {
				combine::logging::Logger::instance().setFileSink(fName, false);
				::setenv("COMBINE_LOG_FILE", fName.c_str(), 1);
			}
		};

		void log(const std::string & _file, const int _lineN, const std::string& _logmsg, const std::string& _function);
		void printLog();
		void setVerbosity(combine::logging::Level level);
		combine::logging::Level verbosity() const;
		void enableFileSink(const std::string &path = std::string(), bool append = false);
		void disableFileSink();
		bool fileSinkEnabled() const { return fileSinkEnabled_; }

	protected:
		// Static variable for the instance  
		static CombineLogger* pL;

		static std::string  fName;
		combine::logging::Level level_;
		bool fileSinkEnabled_;
		CombineLogger();
		virtual ~CombineLogger();
};
#endif
