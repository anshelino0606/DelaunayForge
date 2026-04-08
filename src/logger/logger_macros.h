#ifndef LOGGER_MACROS
#define LOGGER_MACROS

#include "logger_global.h"
#include "logger_category.h"
#include "logger.h"

#ifndef LOGGER_FILE
#  if defined(__FILE_NAME__)
#    define LOGGER_FILE __FILE_NAME__
#  else
#    define LOGGER_FILE __FILE__
#  endif
#endif


#ifndef LOG_CATEGORY
#define LOG_CATEGORY ::logger::Category{"APP"}
#endif

#define DEFINE_LOG_CATEGORY(Name) \
    inline constexpr ::logger::Category Name{#Name}

#define DEFINE_LOG_CATEGORY_NAMED(Name, Pretty) \
    inline constexpr ::logger::Category Name{(Pretty)}


#define LOG_LEVEL_DEBUG ::logger::Level::Debug
#define LOG_LEVEL_INFO  ::logger::Level::Info
#define LOG_LEVEL_WARN  ::logger::Level::Warn
#define LOG_LEVEL_ERROR ::logger::Level::Error

#define LOG(Category, LevelTok, fmt, ...) \
    do { \
        if (::logger::alive_flag().load(std::memory_order_acquire)) { \
            ::logger::global().logf( \
                LOG_LEVEL_##LevelTok, \
                ::logger::Site{(Category).name, LOGGER_FILE, __func__, (uint32_t)__LINE__}, \
                (fmt), ##__VA_ARGS__); \
        } \
    } while (0)

#define LOG_MSG(Category, LevelTok, msg_literal) \
    LOG(Category, LevelTok, "%s", (msg_literal))


#define LOG_DEBUG(fmt, ...) LOG(LOG_CATEGORY, DEBUG, (fmt), ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG(LOG_CATEGORY, INFO,  (fmt), ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG(LOG_CATEGORY, WARN,  (fmt), ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(LOG_CATEGORY, ERROR, (fmt), ##__VA_ARGS__)

#define LOGT_DEBUG(Category, fmt, ...) LOG((Category), DEBUG, (fmt), ##__VA_ARGS__)
#define LOGT_INFO(Category, fmt, ...)  LOG((Category), INFO,  (fmt), ##__VA_ARGS__)
#define LOGT_WARN(Category, fmt, ...)  LOG((Category), WARN,  (fmt), ##__VA_ARGS__)
#define LOGT_ERROR(Category, fmt, ...) LOG((Category), ERROR, (fmt), ##__VA_ARGS__)

#endif
