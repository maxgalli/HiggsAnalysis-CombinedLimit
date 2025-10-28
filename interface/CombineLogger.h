#ifndef HiggsAnalysis_CombinedLimit_CombineLogger_h
#define HiggsAnalysis_CombinedLimit_CombineLogger_h

#include <cstdlib>
#include <memory>
#include <string>

#include "Logger.h"

struct CombinePipeCapture;

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
		void setCaptureEnabled(bool enabled);
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
		bool captureEnabled_;
		std::unique_ptr<CombinePipeCapture> stdoutCapture_;
		std::unique_ptr<CombinePipeCapture> stderrCapture_;
		void refreshPipeCapture();

		CombineLogger();
		virtual ~CombineLogger();
};
#endif
