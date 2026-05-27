#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace player_stats {

struct Stats {
	uint64_t kills = 0;
	uint64_t deaths = 0;
	uint64_t messages = 0;
	uint64_t joins = 0;
	uint64_t leaves = 0;
	uint64_t first_ts = 0;
};

// Starts or refreshes a cached asynchronous lookup as needed and returns the
// full profile when a response for this player has been received.
std::optional<Stats> getStats(const std::string &player);

// Formats the PVP fields consistently for every Luna stats display.
std::wstring formatPvpLine(uint64_t kills, uint64_t deaths);

// Starts or refreshes a cached asynchronous lookup as needed and returns the
// formatted line when a response for this player has been received.
std::optional<std::wstring> getLine(const std::string &player);

} // namespace player_stats
