#pragma once
#include <string>
#include <string_view>
#include <fstream>
#include <iostream>
#include <memory>
#include <ctime>
#include <sstream>
#include "util/Dev.h"

class MyLogger {
public:
    enum class Level { Debug, Info, Warn, Error };
    std::string name;
    Level log_level = Level::Debug;
    std::ofstream file_stream;
    bool file_enabled = false;

    void init(std::string log_fp_prefix, std::string name="g", Level level=Level::Debug) {
        this->name = name;
        this->log_level = level;
        std::string timestamp = get_timestep();
        std::string log_fp = log_fp_prefix + "_" + timestamp + ".log";
        file_stream.open(log_fp, std::ios::out | std::ios::app);
        file_enabled = file_stream.is_open();
    }

    static std::string get_timestep() {
        auto now = time(0);
        auto _time = localtime(&now);
        char buffer[50];
        strftime(buffer, 50, "%y-%m-%d %H:%M:%S", _time);
        return std::string(buffer);
    }

    void set_level(Level level=Level::Debug) {
        log_level = level;
    }

    template<typename... Args>
    void debug(std::string_view msg, const Args & ... args) {
        log(Level::Debug, msg, args...);
    }
    template<typename... Args>
    void info(std::string_view msg, const Args & ... args) {
        log(Level::Info, msg, args...);
    }
    template<typename... Args>
    void warn(std::string_view msg, const Args & ... args) {
        log(Level::Warn, msg, args...);
    }
    template<typename... Args>
    void error(std::string_view msg, const Args & ... args) {
        log(Level::Error, msg, args...);
    }

    void flush() {
        if (file_enabled) file_stream.flush();
        std::cout.flush();
    }

private:
    template<typename... Args>
    void log(Level level, std::string_view msg, const Args & ... args) {
        if (level < log_level) return;
        std::ostringstream oss;
        oss << "[" << get_timestep() << "] [" << level_to_string(level) << "] " << format(msg, args...);
        std::string out = oss.str();
        std::cout << out << std::endl;
        if (file_enabled) file_stream << out << std::endl;
    }

    static std::string level_to_string(Level level) {
        switch(level) {
            case Level::Debug: return "DEBUG";
            case Level::Info: return "INFO";
            case Level::Warn: return "WARN";
            case Level::Error: return "ERROR";
        }
        return "INFO";
    }

    // Simple formatter: replaces each '{}' in msg with the next arg
    template<typename T, typename... Args>
    std::string format(std::string_view msg, T value, Args... args) {
        std::ostringstream oss;
        size_t pos = msg.find("{}");
        if (pos != std::string_view::npos) {
            oss << msg.substr(0, pos) << value << format(msg.substr(pos + 2), args...);
        } else {
            oss << msg;
        }
        return oss.str();
    }
    std::string format(std::string_view msg) { return std::string(msg); }
};

extern MyLogger g_logger;

#define DEV_DEBUG(msg, ...) ONLYDEV(g_logger.debug(msg, ##__VA_ARGS__);)
#define DEV_INFO(msg, ...) ONLYDEV(g_logger.info(msg, ##__VA_ARGS__);)
#define DEV_WARN(msg, ...) ONLYDEV(g_logger.warn(msg, ##__VA_ARGS__);)
#define DEV_ERROR(msg, ...) ONLYDEV(g_logger.error(msg, ##__VA_ARGS__);)