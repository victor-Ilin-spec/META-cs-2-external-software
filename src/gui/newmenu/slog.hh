#pragma once

namespace slog {
	namespace log {
		template <typename... Args>
		inline void info(const char* fmt, Args&&... args)
		{
			LOGF(INFO, fmt, std::forward<Args>(args)...);
		}

		template <typename... Args>
		inline void error(const char* fmt, Args&&... args)
		{
			LOGF(FATAL, fmt, std::forward<Args>(args)...);
		}

		template <typename... Args>
		inline void success(const char* fmt, Args&&... args)
		{
			LOGF(INFO, fmt, std::forward<Args>(args)...);
		}

		template <typename... Args>
		inline void debug(const char* fmt, Args&&... args)
		{
			LOGF(VERBOSE, fmt, std::forward<Args>(args)...);
		}
	}
}
