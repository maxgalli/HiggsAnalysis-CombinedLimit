#include "../interface/CombineLogger.h"

#include <sstream>
int CombineLogger::nLogs = 0;

std::string CombineLogger::fName = "combine_logger.out";

CombineLogger *CombineLogger::pL = nullptr;

CombineLogger &CombineLogger::instance() {
	if (pL == nullptr) {
		pL = new CombineLogger();
	}
	return *pL;
}

CombineLogger::CombineLogger() :
	level_(combine::logging::Level::Info),
	fileSinkEnabled_(false) {
	auto &logger = combine::logging::Logger::instance();
	logger.initialize(level_, std::string(), true);
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
	if (fileSinkEnabled_) ++nLogs;
}

void CombineLogger::printLog() {
	if (!fileSinkEnabled_) return;
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

void CombineLogger::enableFileSink(const std::string &path, bool append) {
	const std::string previousPath = fName;
	if (!path.empty()) {
		fName = path;
	}
	::setenv("COMBINE_LOG_FILE", fName.c_str(), 1);
	const bool needReopen = !fileSinkEnabled_ || append || fName != previousPath;
	if (needReopen) {
		combine::logging::Logger::instance().setFileSink(fName, append);
	}
	if (!fileSinkEnabled_ || fName != previousPath) nLogs = 0;
	fileSinkEnabled_ = true;
}

void CombineLogger::disableFileSink() {
	if (!fileSinkEnabled_) return;
	combine::logging::Logger::instance().clearFileSink();
	fileSinkEnabled_ = false;
	::unsetenv("COMBINE_LOG_FILE");
}

CombineLogger::~CombineLogger() {
	CombineLogger::pL = nullptr;
	combine::logging::Logger::instance().shutdown();
}
