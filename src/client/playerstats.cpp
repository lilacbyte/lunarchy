#include "playerstats.h"

#include "httpfetch.h"
#include "util/string.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace player_stats {
namespace {

using Clock = std::chrono::steady_clock;

struct CachedStats {
	std::optional<Stats> stats;
	Clock::time_point last_request{};
	Clock::time_point last_access{};
	bool pending = false;
};

std::unordered_map<std::string, CachedStats> g_cache;
std::unordered_map<u64, std::string> g_requests;
u64 g_caller = HTTPFETCH_DISCARD;
u64 g_next_request_id = 1;
Clock::time_point g_last_poll{};
Clock::time_point g_last_prune{};

constexpr size_t MAX_RESULTS_PER_POLL = 4;
constexpr auto POLL_INTERVAL = std::chrono::milliseconds(16);
constexpr auto CACHE_PRUNE_INTERVAL = std::chrono::minutes(1);
constexpr auto CACHE_MAX_IDLE = std::chrono::minutes(5);

std::string keyForPlayer(const std::string &player)
{
	std::string key = player;
	std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return key;
}

void pollResults()
{
	if (g_caller == HTTPFETCH_DISCARD)
		return;

	const auto now = Clock::now();
	if (g_last_poll != Clock::time_point{} && now - g_last_poll < POLL_INTERVAL)
		return;
	g_last_poll = now;

	HTTPFetchResult result;
	size_t processed = 0;
	while (processed < MAX_RESULTS_PER_POLL && httpfetch_async_get(g_caller, result)) {
		++processed;
		const auto request = g_requests.find(result.request_id);
		if (request == g_requests.end())
			continue;

		CachedStats &entry = g_cache[request->second];
		entry.pending = false;
		g_requests.erase(request);

		if (!result.succeeded || result.response_code != 200)
			continue;

		try {
			const Json::Value data = str_to_json(result.data);
			if (!data.get("ok", false).asBool() ||
					!data["kills"].isNumeric() || !data["deaths"].isNumeric() ||
					!data["messages"].isNumeric() || !data["joins"].isNumeric() ||
					!data["leaves"].isNumeric() || !data["first_ts"].isNumeric())
				continue;
			Stats stats;
			stats.kills = data["kills"].asUInt64();
			stats.deaths = data["deaths"].asUInt64();
			stats.messages = data["messages"].asUInt64();
			stats.joins = data["joins"].asUInt64();
			stats.leaves = data["leaves"].asUInt64();
			stats.first_ts = data["first_ts"].asUInt64();
			entry.stats = stats;
		} catch (const std::exception &) {
			// Keep the previous value until a later successful refresh.
		}
	}
}

void pruneCache(const Clock::time_point now)
{
	if (g_last_prune != Clock::time_point{} && now - g_last_prune < CACHE_PRUNE_INTERVAL)
		return;
	g_last_prune = now;

	for (auto it = g_cache.begin(); it != g_cache.end();) {
		const CachedStats &entry = it->second;
		if (!entry.pending && entry.last_access != Clock::time_point{} &&
				now - entry.last_access >= CACHE_MAX_IDLE) {
			it = g_cache.erase(it);
		} else {
			++it;
		}
	}
}

void requestStats(const std::string &player, const std::string &key, CachedStats &entry)
{
	if (g_caller == HTTPFETCH_DISCARD)
		g_caller = httpfetch_caller_alloc();

	HTTPFetchRequest request;
	request.caller = g_caller;
	request.request_id = g_next_request_id++;
	request.url = "https://luna-api.miaaa.dev/v1/stats/player?player=" + urlencode(player);
	httpfetch_async(request);

	entry.pending = true;
	entry.last_request = Clock::now();
	g_requests[request.request_id] = key;
}

} // namespace

std::wstring formatPvpLine(uint64_t kills, uint64_t deaths)
{
	std::ostringstream line;
	line << "k:" << kills << "/d:" << deaths;
	return utf8_to_wide(line.str());
}

std::optional<Stats> getStats(const std::string &player)
{
	pollResults();
	const std::string key = keyForPlayer(player);
	if (key == "lunabot")
		return std::nullopt;

	const auto now = Clock::now();
	pruneCache(now);
	CachedStats &entry = g_cache[key];
	entry.last_access = now;
	const auto elapsed = now - entry.last_request;
	const auto retry_after = entry.stats ? std::chrono::seconds(60) : std::chrono::seconds(10);

	if (!entry.pending && (entry.last_request == Clock::time_point{} || elapsed >= retry_after))
		requestStats(player, key, entry);

	return entry.stats;
}

std::optional<std::wstring> getLine(const std::string &player)
{
	const auto stats = getStats(player);
	if (!stats)
		return std::nullopt;
	return formatPvpLine(stats->kills, stats->deaths);
}

} // namespace player_stats
