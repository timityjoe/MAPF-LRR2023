#include "util/MyLogger.h"

int main() {
    g_logger.init("logs/test");
    
    g_logger.error("this is error");
    g_logger.warn("this is warn");
    g_logger.info("this is info");
    DEV_DEBUG("this is debug");

    g_logger.info("this is simple formatted info: hello, {}", "world");
    g_logger.info("this is formatted info: hello, {}", "world");

    g_logger.info("this is formatted info: hello, {} and {}", "world", 42);

    DEV_DEBUG("this is macro debug: hello, world");
    DEV_ERROR("this is macro error: hello, {}", "world");
}