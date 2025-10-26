#ifndef HiggsAnalysis_CombinedLimit_Logger_h
#define HiggsAnalysis_CombinedLimit_Logger_h

#include <cstdio>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>

namespace combine::logging {

enum class Level {
    Trace = 0,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
    Off
};

const char *levelToString(Level level);
Level stringToLevel(const std::string &name);

class Logger {
  public:
    static Logger &instance();

    void initialize(Level level = Level::Info,
                    const std::string &fileSink = std::string(),
                    bool enableConsoleColors = true);
    void shutdown();

    void setLevel(Level level);
    Level level() const;

    void setConsoleColorsEnabled(bool enabled);
    bool consoleColorsEnabled() const;

    void setFileSink(const std::string &filePath, bool append = false);
    void clearFileSink();

    void log(Level level, const std::string &message,
             const char *file = nullptr,
             int line = 0,
             const char *function = nullptr,
             const char *channel = nullptr,
             bool skipConsole = false);

    void attachStandardStreams();
    void detachStandardStreams();

    void pushSuppression();
    void popSuppression();
    bool isSuppressed() const;

  private:
    Logger();
    ~Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    struct Pipe;
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void startPipeCapture(int fd, FILE *stream, Pipe &pipe, Level level, const char *channel);
    void stopPipeCapture(int fd, Pipe &pipe);
    void logFromPipe(Level level, const std::string &message, const char *channel);
};

} // namespace combine::logging

#define COMBINE_LOG(level, message) \
    ::combine::logging::Logger::instance().log(level, message, __FILE__, __LINE__, __func__)
#define COMBINE_LOG_TRACE(message) COMBINE_LOG(::combine::logging::Level::Trace, message)
#define COMBINE_LOG_DEBUG(message) COMBINE_LOG(::combine::logging::Level::Debug, message)
#define COMBINE_LOG_INFO(message)  COMBINE_LOG(::combine::logging::Level::Info, message)
#define COMBINE_LOG_WARN(message)  COMBINE_LOG(::combine::logging::Level::Warning, message)
#define COMBINE_LOG_ERROR(message) COMBINE_LOG(::combine::logging::Level::Error, message)
#define COMBINE_LOG_CRITICAL(message) COMBINE_LOG(::combine::logging::Level::Critical, message)

#endif
