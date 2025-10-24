#include "../interface/CombineLogger.h"

#include <sstream>
int CombineLogger::nLogs = 0;

const char *CombineLogger::fName = "combine_logger.out";

CombineLogger *CombineLogger::pL = nullptr;

CombineLogger &CombineLogger::instance() {
	if (pL == nullptr) {
		pL = new CombineLogger();
	}
	return *pL;
}

CombineLogger::CombineLogger() : level_(combine::logging::Level::Info) {
	::setenv("COMBINE_LOG_FILE", fName, 1);
	auto &logger = combine::logging::Logger::instance();
	logger.initialize(level_, fName, true);
}

void CombineLogger::log(const std::string &_file, const int _lineN, const std::string &_logmsg, const std::string &_function) {
	auto &logger = combine::logging::Logger::instance();
	auto detectLevel = [&](const std::string &message) {
		if (message.find("[CRITICAL]") != std::string::npos) return combine::logging::Level::Critical;
		if (message.find("[ERROR]") != std::string::npos) return combine::logging::Level::Error;
		if (message.find("[WARNING]") != std::string::npos || message.find("[WARN]") != std::string::npos)
			return combine::logging::Level::Warning;
		if (message.find("[DEBUG]") != std::string::npos) return combine::logging::Level::Debug;
		return level_;
	};
	logger.log(detectLevel(_logmsg), _logmsg, _file.c_str(), _lineN, _function.c_str(), "combine");
	++nLogs;
}

void CombineLogger::printLog() {
	auto &logger = combine::logging::Logger::instance();
	std::ostringstream oss;
	oss << nLogs << " log messages saved to " << fName;
	logger.log(combine::logging::Level::Info, oss.str(), __FILE__, __LINE__, __func__, "combine");
}

void CombineLogger::setVerbosity(combine::logging::Level level) {
	level_ = level;
	combine::logging::Logger::instance().setLevel(level);
}

combine::logging::Level CombineLogger::verbosity() const {
	return level_;
}

CombineLogger::~CombineLogger() {
	CombineLogger::pL = nullptr;
	combine::logging::Logger::instance().shutdown();
}
