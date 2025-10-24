#include "../interface/Logger.h"

#include <atomic>
#include <array>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <streambuf>
#include <thread>
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

    std::unique_ptr<std::ostream> consoleOut;
    std::unique_ptr<std::ostream> consoleErr;
    std::streambuf *originalCoutBuf = nullptr;
    std::streambuf *originalCerrBuf = nullptr;

    std::unique_ptr<LoggingStreambuf> coutHook;
    std::unique_ptr<LoggingStreambuf> cerrHook;

    struct Pipe {
        int readFd = -1;
        int dupFd = -1;
        std::thread reader;
        std::string buffer;
        std::atomic<bool> running{false};
        Level level = Level::Info;
        std::string channel;

        void reset() {
            buffer.clear();
            running.store(false);
            if (reader.joinable()) reader.join();
            if (readFd != -1) {
                ::close(readFd);
                readFd = -1;
            }
            if (dupFd != -1) {
                ::close(dupFd);
                dupFd = -1;
            }
        }
    };

    Pipe stdoutPipe;
    Pipe stderrPipe;
    bool pipesAttached = false;

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

static void writeAll(int fd, const char *data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        ssize_t written = ::write(fd, data + offset, size - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            break;
        }
        offset += static_cast<size_t>(written);
    }
}

static void startPipeCapture(Logger &logger, Logger::Impl &impl, int fd, FILE *stream, Logger::Impl::Pipe &pipe, Level level, const char *channel) {
    if (pipe.running.load()) return;

    int fds[2];
    if (::pipe(fds) != 0) return;

    pipe.dupFd = ::dup(fd);
    if (pipe.dupFd == -1) {
        ::close(fds[0]);
        ::close(fds[1]);
        return;
    }

    if (stream) {
        ::fflush(stream);
    }

    if (::dup2(fds[1], fd) != 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        ::close(pipe.dupFd);
        pipe.dupFd = -1;
        return;
    }

    ::close(fds[1]);

    pipe.readFd = fds[0];
    pipe.level = level;
    pipe.channel = channel ? channel : "";
    pipe.buffer.clear();
    pipe.running.store(true);

    if (stream) {
        if (fd == STDOUT_FILENO) {
            setvbuf(stream, nullptr, _IOLBF, 0);
        } else if (fd == STDERR_FILENO) {
            setvbuf(stream, nullptr, _IONBF, 0);
        }
    }

    pipe.reader = std::thread([&logger, &pipe]() {
        std::array<char, 512> buf{};
        while (pipe.running.load()) {
            ssize_t n = ::read(pipe.readFd, buf.data(), buf.size());
            if (n <= 0) {
                if (n == -1 && errno == EINTR) continue;
                break;
            }

            bool suppressed = logger.isSuppressed();

            if (!suppressed && pipe.dupFd != -1) {
                writeAll(pipe.dupFd, buf.data(), static_cast<size_t>(n));
            }

            if (suppressed) {
                pipe.buffer.clear();
                continue;
            }

            pipe.buffer.append(buf.data(), static_cast<size_t>(n));
            size_t pos;
            while ((pos = pipe.buffer.find('\n')) != std::string::npos) {
                std::string line = pipe.buffer.substr(0, pos);
                pipe.buffer.erase(0, pos + 1);
                if (!line.empty()) {
                    logger.logFromPipe(pipe.level, line, pipe.channel.c_str());
                }
            }
        }

        if (!pipe.buffer.empty()) {
            logger.logFromPipe(pipe.level, pipe.buffer, pipe.channel.c_str());
            pipe.buffer.clear();
        }
    });
}

static void stopPipeCapture(int fd, Logger::Impl::Pipe &pipe) {
    bool wasRunning = pipe.running.exchange(false);
    if (pipe.readFd != -1) {
        ::close(pipe.readFd);
        pipe.readFd = -1;
    }
    if (wasRunning && pipe.reader.joinable()) {
        pipe.reader.join();
    } else if (pipe.reader.joinable()) {
        pipe.reader.join();
    }
    if (pipe.dupFd != -1) {
        ::dup2(pipe.dupFd, fd);
        ::close(pipe.dupFd);
        pipe.dupFd = -1;
    }
    pipe.channel.clear();
    pipe.buffer.clear();
}

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

void Logger::log(Level level, const std::string &message, const char *file, int line,
                 const char *function, const char *channel, bool skipConsole) {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (level < impl_->level || impl_->suppressionDepth > 0) return;

    std::ostringstream formatted;
    formatted << "[" << makeTimestamp() << "] ";
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

        bool wrote = false;
        if (impl_->pipesAttached) {
            int targetFd = (level >= Level::Warning) ? impl_->stderrPipe.dupFd : impl_->stdoutPipe.dupFd;
            if (targetFd != -1) {
                writeAll(targetFd, consolePayload.data(), consolePayload.size());
                wrote = true;
            }
        }

        if (!wrote) {
            auto &out = impl_->streamForLevel(level);
            out << consolePayload;
            out.flush();
        }
    }

    if (impl_->fileStream.is_open()) {
        std::string filePayload = baseText;
        if (filePayload.empty() || filePayload.back() != '\n') filePayload.push_back('\n');
        impl_->fileStream << filePayload;
        impl_->fileStream.flush();
    }
}

void Logger::logFromPipe(Level level, const std::string &message, const char *channel) {
    if (message.empty()) return;
    log(level, message, nullptr, 0, nullptr, channel, true);
}

void Logger::attachStandardStreams() {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->coutHook || impl_->cerrHook) return;

    startPipeCapture(*this, *impl_, STDOUT_FILENO, stdout, impl_->stdoutPipe, Level::Info, "stdout");
    startPipeCapture(*this, *impl_, STDERR_FILENO, stderr, impl_->stderrPipe, Level::Warning, "stderr");
    impl_->pipesAttached = impl_->stdoutPipe.running.load() || impl_->stderrPipe.running.load();

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
    if (impl_->pipesAttached) {
        stopPipeCapture(STDOUT_FILENO, impl_->stdoutPipe);
        stopPipeCapture(STDERR_FILENO, impl_->stderrPipe);
        impl_->pipesAttached = false;
    }
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
