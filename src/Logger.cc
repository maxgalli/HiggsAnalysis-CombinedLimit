#include "../interface/Logger.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <streambuf>
#include <utility>
#include <vector>

#include <unistd.h>

namespace {

class LoggingStreambuf : public std::streambuf {
  public:
    LoggingStreambuf(std::streambuf *upstream,
                     std::ostream &passthrough,
                     combine::logging::Logger &logger,
                     combine::logging::Level level,
                     const char *channel)
        : upstream_(upstream),
          passthrough_(passthrough),
          logger_(logger),
          level_(level),
          channel_(channel ? channel : "") {}

  protected:
    int overflow(int ch) override {
        if (ch == traits_type::eof()) {
            flushBuffer();
            return ch;
        }
        buffer_.push_back(static_cast<char>(ch));
        if (ch == '\n') flushBuffer();
        return ch;
    }

    int sync() override {
        flushBuffer();
        return 0;
    }

  private:
    void flushBuffer() {
        if (buffer_.empty()) return;
        std::string message(buffer_.begin(), buffer_.end());
        buffer_.clear();

        if (!logger_.isSuppressed()) {
            logger_.log(level_, message, nullptr, 0, nullptr, channel_.empty() ? nullptr : channel_.c_str());
        } else if (upstream_) {
            upstream_->sputn(message.data(), message.size());
        } else {
            passthrough_.write(message.data(), static_cast<std::streamsize>(message.size()));
        }
    }

    std::streambuf *upstream_;
    std::ostream &passthrough_;
    combine::logging::Logger &logger_;
    combine::logging::Level level_;
    std::string channel_;
    std::vector<char> buffer_;
};

std::string makeTimestamp() {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    auto time = clock::to_time_t(now);
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
    std::tm tm;
    localtime_r(&time, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(6) << micros.count();
    return oss.str();
}

bool consoleSupportsColor(int fd) {
    return ::isatty(fd) == 1;
}

const char *levelToAnsi(combine::logging::Level level) {
    switch (level) {
        case combine::logging::Level::Trace: return "\033[38;5;244m";
        case combine::logging::Level::Debug: return "\033[38;5;39m";
        case combine::logging::Level::Info: return "\033[0m";
        case combine::logging::Level::Warning: return "\033[38;5;214m";
        case combine::logging::Level::Error: return "\033[38;5;196m";
        case combine::logging::Level::Critical: return "\033[1;38;5;196m";
        default: return "\033[0m";
    }
}

} // namespace

namespace combine::logging {

struct Logger::Impl {
    Level level = Level::Info;
    bool consoleColors = true;
    bool consoleColorCapable = true;
    bool initialized = false;
    unsigned suppressionDepth = 0;
    bool includeTimestamp = false;

    std::unique_ptr<std::ostream> consoleOut;
    std::unique_ptr<std::ostream> consoleErr;
    std::streambuf *originalCoutBuf = nullptr;
    std::streambuf *originalCerrBuf = nullptr;

    std::unique_ptr<LoggingStreambuf> coutHook;
    std::unique_ptr<LoggingStreambuf> cerrHook;

    std::ofstream fileStream;
    std::recursive_mutex mutex;

    void ensureConsoleCapability() {
        consoleColorCapable = consoleSupportsColor(fileno(stdout));
    }

    std::ostream &streamForLevel(Level level) {
        switch (level) {
            case Level::Warning:
            case Level::Error:
            case Level::Critical:
                return consoleErr ? *consoleErr : std::cerr;
            default:
                return consoleOut ? *consoleOut : std::cout;
        }
    }
};

Logger::Logger() : impl_(new Impl()) {
    impl_->ensureConsoleCapability();
}

Logger::~Logger() {
    shutdown();
}

Logger &Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::initialize(Level level, const std::string &fileSink, bool enableConsoleColors) {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->level = level;
    impl_->consoleColors = enableConsoleColors;
    if (!fileSink.empty()) {
        setFileSink(fileSink);
    }
    attachStandardStreams();
    impl_->initialized = true;
}

void Logger::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    detachStandardStreams();
    if (impl_->fileStream.is_open()) {
        impl_->fileStream.flush();
        impl_->fileStream.close();
    }
    impl_->initialized = false;
}

void Logger::setLevel(Level level) {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->level = level;
}

Level Logger::level() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    return impl_->level;
}

void Logger::setConsoleColorsEnabled(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->consoleColors = enabled;
}

bool Logger::consoleColorsEnabled() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    return impl_->consoleColors;
}

void Logger::setFileSink(const std::string &filePath, bool append) {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->fileStream.is_open()) impl_->fileStream.close();
    impl_->fileStream.clear();
    impl_->fileStream.open(filePath.c_str(), append ? std::ios::app : std::ios::trunc);
}

void Logger::clearFileSink() {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->fileStream.is_open()) {
        impl_->fileStream.flush();
        impl_->fileStream.close();
    }
}

void Logger::setIncludeTimestamp(bool include) {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->includeTimestamp = include;
}

void Logger::log(Level level, const std::string &message, const char *file, int line,
                 const char *function, const char *channel, bool skipConsole) {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (level < impl_->level || impl_->suppressionDepth > 0) return;

    std::ostringstream formatted;
    if (impl_->includeTimestamp) {
        formatted << "[" << makeTimestamp() << "] ";
    }
    formatted << "[" << levelToString(level) << "]";
    if (channel && channel[0]) formatted << "[" << channel << "]";
    if (file && line > 0) {
        formatted << "[" << file << ":" << line << "]";
    }
    if (function) formatted << "[" << function << "]";
    formatted << " " << message;

    std::string baseText = formatted.str();

    if (!skipConsole) {
        std::string consolePayload = baseText;
        if (consolePayload.empty() || consolePayload.back() != '\n') {
            consolePayload.push_back('\n');
        }
        if (impl_->consoleColors && impl_->consoleColorCapable) {
            consolePayload = std::string(levelToAnsi(level)) + consolePayload + "\033[0m";
        }

        auto &out = impl_->streamForLevel(level);
        out << consolePayload;
        out.flush();
    }

    if (impl_->fileStream.is_open()) {
        std::string filePayload = baseText;
        if (filePayload.empty() || filePayload.back() != '\n') filePayload.push_back('\n');
        impl_->fileStream << filePayload;
        impl_->fileStream.flush();
    }
}

void Logger::attachStandardStreams() {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->coutHook || impl_->cerrHook) return;

    impl_->originalCoutBuf = std::cout.rdbuf();
    impl_->originalCerrBuf = std::cerr.rdbuf();
    impl_->consoleOut = std::make_unique<std::ostream>(impl_->originalCoutBuf);
    impl_->consoleErr = std::make_unique<std::ostream>(impl_->originalCerrBuf);

    impl_->coutHook = std::make_unique<LoggingStreambuf>(impl_->originalCoutBuf,
                                                         *impl_->consoleOut,
                                                         *this,
                                                         Level::Info,
                                                         "stdout");
    impl_->cerrHook = std::make_unique<LoggingStreambuf>(impl_->originalCerrBuf,
                                                         *impl_->consoleErr,
                                                         *this,
                                                         Level::Warning,
                                                         "stderr");

    std::cout.rdbuf(impl_->coutHook.get());
    std::cerr.rdbuf(impl_->cerrHook.get());
}

void Logger::detachStandardStreams() {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->originalCoutBuf) {
        std::cout.rdbuf(impl_->originalCoutBuf);
        impl_->originalCoutBuf = nullptr;
    }
    if (impl_->originalCerrBuf) {
        std::cerr.rdbuf(impl_->originalCerrBuf);
        impl_->originalCerrBuf = nullptr;
    }
    impl_->consoleOut.reset();
    impl_->consoleErr.reset();
    impl_->coutHook.reset();
    impl_->cerrHook.reset();
}

void Logger::pushSuppression() {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    ++impl_->suppressionDepth;
}

void Logger::popSuppression() {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->suppressionDepth > 0) --impl_->suppressionDepth;
}

bool Logger::isSuppressed() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    return impl_->suppressionDepth > 0;
}

const char *levelToString(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warning: return "WARN";
        case Level::Error: return "ERROR";
        case Level::Critical: return "CRITICAL";
        default: return "OFF";
    }
}

Level stringToLevel(const std::string &name) {
    if (name == "trace" || name == "TRACE") return Level::Trace;
    if (name == "debug" || name == "DEBUG") return Level::Debug;
    if (name == "info"  || name == "INFO")  return Level::Info;
    if (name == "warn"  || name == "warning" || name == "WARN" || name == "WARNING")
        return Level::Warning;
    if (name == "error" || name == "ERROR") return Level::Error;
    if (name == "critical" || name == "CRITICAL") return Level::Critical;
    return Level::Info;
}

} // namespace combine::logging
