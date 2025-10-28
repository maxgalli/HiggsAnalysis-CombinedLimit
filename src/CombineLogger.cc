#include "../interface/CombineLogger.h"
#include "../interface/CloseCoutSentry.h"

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

struct CombinePipeCapture {
    int fd = -1;
    int originalFd = -1;
    int pipeRead = -1;
    combine::logging::Level level = combine::logging::Level::Info;
    std::string channel;
    std::thread worker;
    std::atomic<bool> running{false};
};

namespace {

using PipeCapture = CombinePipeCapture;

void writeAll(int fd, const char *data, size_t size) {
	while (size > 0) {
		ssize_t written = ::write(fd, data, size);
		if (written < 0) {
			if (errno == EINTR) continue;
			break;
		}
		data += written;
		size -= static_cast<size_t>(written);
	}
}

bool shouldMirrorToLog(const std::string &line) {
	size_t start = 0;
	while (start < line.size() && line[start] == '\033') {
		size_t escEnd = line.find('m', start);
		if (escEnd == std::string::npos) break;
		start = escEnd + 1;
	}
    std::string prefixCandidate = line.substr(start);
    static const std::array<const char *, 6> prefixes{{
        "[INFO]", "[WARN]", "[ERROR]", "[DEBUG]", "[TRACE]", "[CRITICAL]"}};
	for (const char *prefix : prefixes) {
		if (prefixCandidate.rfind(prefix, 0) == 0) return false;
	}
	return true;
}

void emitLine(PipeCapture &pipe, combine::logging::Logger &logger, const std::string &line) {
	if (pipe.originalFd >= 0) {
		std::string payload = line;
		payload.push_back('\n');
		writeAll(pipe.originalFd, payload.c_str(), payload.size());
	}
if (shouldMirrorToLog(line)) {
	logger.log(
	    pipe.level,
	    line,
	    nullptr,
	    0,
	    nullptr,
	    pipe.channel.empty() ? nullptr : pipe.channel.c_str());
}
}

void emitRemainder(PipeCapture &pipe, combine::logging::Logger &logger, std::string &buffer) {
	if (buffer.empty()) return;
	if (pipe.originalFd >= 0) {
		writeAll(pipe.originalFd, buffer.c_str(), buffer.size());
	}
if (shouldMirrorToLog(buffer)) {
	logger.log(
	    pipe.level,
	    buffer,
	    nullptr,
	    0,
	    nullptr,
	    pipe.channel.empty() ? nullptr : pipe.channel.c_str());
}
	buffer.clear();
}

void startPipeCapture(PipeCapture &pipe,
                      combine::logging::Logger &logger,
                      int fd,
                      FILE *stream,
                      combine::logging::Level level,
                      const char *channel) {
	if (pipe.running.load()) return;

	int ends[2];
	if (::pipe(ends) != 0) {
		return;
	}
#ifdef FD_CLOEXEC
	::fcntl(ends[0], F_SETFD, FD_CLOEXEC);
	::fcntl(ends[1], F_SETFD, FD_CLOEXEC);
#endif

	pipe.pipeRead = ends[0];
	int pipeWrite = ends[1];
	pipe.originalFd = ::dup(fd);
	if (pipe.originalFd < 0) {
		::close(pipe.pipeRead);
		::close(pipeWrite);
		pipe.pipeRead = -1;
		return;
	}

	pipe.fd = fd;
	pipe.level = level;
	pipe.channel = channel ? channel : "";
	pipe.running.store(true);

	::setvbuf(stream, nullptr, _IONBF, 0);
	::fflush(stream);
	::dup2(pipeWrite, fd);
	::close(pipeWrite);

	pipe.worker = std::thread([&pipe, &logger]() {
		std::array<char, 512> chunk{};
		std::string buffer;
		while (pipe.running.load()) {
			ssize_t count = ::read(pipe.pipeRead, chunk.data(), chunk.size());
			if (count > 0) {
				buffer.append(chunk.data(), static_cast<size_t>(count));
				size_t pos = 0;
				while ((pos = buffer.find('\n')) != std::string::npos) {
					std::string line = buffer.substr(0, pos);
					buffer.erase(0, pos + 1);
					if (!line.empty()) emitLine(pipe, logger, line);
				}
			} else if (count == 0) {
				break;
			} else if (errno != EINTR) {
				break;
			}
		}
		emitRemainder(pipe, logger, buffer);
		if (pipe.pipeRead >= 0) {
			::close(pipe.pipeRead);
			pipe.pipeRead = -1;
		}
	});
}

void stopPipeCapture(PipeCapture &pipe) {
	if (!pipe.running.load()) return;

	pipe.running.store(false);
	if (pipe.fd == STDOUT_FILENO) {
		::fflush(stdout);
	} else if (pipe.fd == STDERR_FILENO) {
		::fflush(stderr);
	}
	if (pipe.originalFd >= 0 && pipe.fd >= 0) {
		::dup2(pipe.originalFd, pipe.fd);
	}
	if (pipe.originalFd >= 0) {
		::close(pipe.originalFd);
		pipe.originalFd = -1;
	}
	if (pipe.worker.joinable()) {
		pipe.worker.join();
	}
	if (pipe.pipeRead >= 0) {
		::close(pipe.pipeRead);
		pipe.pipeRead = -1;
	}
	pipe.fd = -1;
	pipe.channel.clear();
}

} // namespace
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
	fileSinkEnabled_(false),
	captureEnabled_(false),
	stdoutCapture_(new CombinePipeCapture()),
	stderrCapture_(new CombinePipeCapture()) {
	auto &logger = combine::logging::Logger::instance();
	logger.initialize(level_, std::string(), true);
}

void CombineLogger::log(const std::string &_file, const int _lineN, const std::string &_logmsg, const std::string &_function) {
	auto &logger = combine::logging::Logger::instance();
	const bool wasSuppressed = logger.isSuppressed();
	if (wasSuppressed) logger.popSuppression();
	auto detectLevel = [&](const std::string &message) {
		if (message.find("[CRITICAL]") != std::string::npos) return combine::logging::Level::Critical;
		if (message.find("[ERROR]") != std::string::npos) return combine::logging::Level::Error;
		if (message.find("[WARNING]") != std::string::npos || message.find("[WARN]") != std::string::npos)
			return combine::logging::Level::Warning;
		if (message.find("[DEBUG]") != std::string::npos) return combine::logging::Level::Debug;
		return level_;
	};
	std::ostringstream formatted;
	if (!_file.empty()) {
		formatted << _file;
		if (_lineN > 0) formatted << "[" << _lineN << "]";
	}
	if (!_function.empty()) {
		if (formatted.tellp() > 0) formatted << " : ";
		formatted << "(in function: " << _function << ")";
	}
	if (!_logmsg.empty()) {
		if (formatted.tellp() > 0) formatted << " - ";
		formatted << _logmsg;
	}
	std::string payload = formatted.str();
	if (payload.empty()) payload = _logmsg;

	const auto level = detectLevel(_logmsg);
	if (wasSuppressed) {
		logger.log(level, payload, nullptr, 0, nullptr, nullptr, /*skipConsole=*/true);
		if (!payload.empty()) {
			FILE *realOut = CloseCoutSentry::trueStdOutGlobal();
			if (!realOut) realOut = stdout;
			std::fprintf(realOut, "%s\n", payload.c_str());
			std::fflush(realOut);
		}
		logger.pushSuppression();
	} else {
		logger.log(level, payload, nullptr, 0, nullptr, nullptr);
	}
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
	refreshPipeCapture();
}

void CombineLogger::setCaptureEnabled(bool enabled) {
	if (captureEnabled_ == enabled) return;
	captureEnabled_ = enabled;
	refreshPipeCapture();
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
	refreshPipeCapture();
}

void CombineLogger::disableFileSink() {
	if (!fileSinkEnabled_) return;
	combine::logging::Logger::instance().clearFileSink();
	fileSinkEnabled_ = false;
	::unsetenv("COMBINE_LOG_FILE");
	refreshPipeCapture();
}

void CombineLogger::refreshPipeCapture() {
	auto &logger = combine::logging::Logger::instance();
	const bool capture = fileSinkEnabled_ && captureEnabled_;
	if (capture) {
		auto stdoutLevel = (level_ == combine::logging::Level::Trace)
		                            ? combine::logging::Level::Trace
		                            : combine::logging::Level::Debug;
		if (stdoutCapture_) {
			if (stdoutCapture_->running.load()) {
				stdoutCapture_->level = stdoutLevel;
				stdoutCapture_->channel = "stdout";
			} else {
				startPipeCapture(*stdoutCapture_, logger, STDOUT_FILENO, stdout, stdoutLevel, "stdout");
			}
		}
		if (stderrCapture_) {
			if (stderrCapture_->running.load()) {
				stderrCapture_->level = combine::logging::Level::Warning;
				stderrCapture_->channel = "stderr";
			} else {
				startPipeCapture(*stderrCapture_, logger, STDERR_FILENO, stderr, combine::logging::Level::Warning, "stderr");
			}
		}
	} else {
		if (stdoutCapture_) stopPipeCapture(*stdoutCapture_);
		if (stderrCapture_) stopPipeCapture(*stderrCapture_);
	}
}

CombineLogger::~CombineLogger() {
	if (stdoutCapture_) stopPipeCapture(*stdoutCapture_);
	if (stderrCapture_) stopPipeCapture(*stderrCapture_);
	CombineLogger::pL = nullptr;
	combine::logging::Logger::instance().shutdown();
}
