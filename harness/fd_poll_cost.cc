// fd_poll_cost: the cost of system.cc's get_open_files_per_proc() (poll over every descriptor up to the limit,
// 10,240 pollfds per call) at several limits; the loop that serves GET /:metrics runs it once per scrape.
//   c++ -std=c++17 -O2 fd_poll_cost.cc -o fd_poll_cost && fd_poll_cost
#include <poll.h>
#include <chrono>
#include <cstdio>
#include <vector>

static std::size_t count_open(std::size_t limit) {
	constexpr std::size_t kOpenMax = 10240;
	std::vector<pollfd> fds(kOpenMax);
	std::size_t cnt = 0, off = 0;
	while (limit) {
		const std::size_t n = limit > kOpenMax ? kOpenMax : limit;
		for (std::size_t i = 0; i < n; ++i) { fds[i] = {static_cast<int>(i + off), POLLIN | POLLOUT | POLLPRI, 0}; }
		if (poll(fds.data(), static_cast<nfds_t>(n), 0) != -1) {
			for (std::size_t i = 0; i < n; ++i) { if (!(fds[i].revents & POLLNVAL)) { ++cnt; } }
		}
		limit -= n;
		off += kOpenMax;
	}
	return cnt;
}

int main() {
	for (std::size_t limit : {1024ul, 10240ul, 65536ul, 262144ul, 1048576ul}) {
		double best = 1e18;
		std::size_t cnt = 0;
		for (int r = 0; r < 20; ++r) {
			const auto t = std::chrono::steady_clock::now();
			cnt = count_open(limit);
			best = std::min(best, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count());
		}
		std::printf("limit %8zu: %zu open, best of 20: %8.3f ms\n", limit, cnt, best);
	}
}
