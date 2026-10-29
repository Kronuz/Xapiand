// Commit-shaped flush patterns on macOS: N data files, a directory, a manifest temporary, a rename.
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
using clk = std::chrono::steady_clock;
static void full(int fd) { if (fcntl(fd, F_FULLFSYNC) < 0) { perror("full"); exit(1); } }
static void barrier(int fd) { if (fcntl(fd, F_BARRIERFSYNC) < 0) { perror("barrier"); exit(1); } }
static void wr(int fd, size_t n) { std::string b(n, 'x'); if (write(fd, b.data(), n) != (ssize_t)n) { perror("write"); exit(1); } }
// pattern: 0 current(4 full), 1 fold dir (3 full), 2 +tmp barrier (2 full+1 barrier), 3 one ordering point (1 full + 1 barrier), 4 = 3 with full at end replaced by nothing (lower bound, not durable)
static double run(const std::string& dir, int pattern, int round, int nfiles, size_t sz) {
	int dfd = open(dir.c_str(), O_RDONLY | O_DIRECTORY);
	auto t0 = clk::now();
	std::vector<int> fds;
	for (int i = 0; i < nfiles; ++i) {
		std::string p = dir + "/d" + std::to_string(round) + "_" + std::to_string(i);
		int fd = open(p.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600); wr(fd, sz); fds.push_back(fd);
	}
	for (int fd : fds) fsync(fd);
	if (pattern == 0) { full(fds[0]); fsync(dfd); full(dfd); }
	else { fsync(dfd); if (pattern <= 2) full(fds[0]); }
	std::string tmp = dir + "/m" + std::to_string(round) + ".tmp";
	int t = open(tmp.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600); wr(t, 4096);
	if (pattern <= 1) full(t); else if (pattern == 2) barrier(t); else { fsync(t); barrier(dfd); }
	rename(tmp.c_str(), (dir + "/m" + std::to_string(round)).c_str());
	fsync(dfd); full(dfd);
	double ms = std::chrono::duration<double, std::milli>(clk::now() - t0).count();
	for (int fd : fds) close(fd); close(t); close(dfd);
	return ms;
}
int main(int argc, char** argv) {
	std::string dir = argv[1]; int nfiles = atoi(argv[2]); size_t sz = atoi(argv[3]); int rounds = atoi(argv[4]);
	mkdir(dir.c_str(), 0700);
	const char* names[] = {"P0 current 4 full", "P1 fold dir 3 full", "P2 +tmp barrier 2 full 1 bar", "P3 one ordering point 1 full 1 bar"};
	std::vector<double> v[4];
	for (int r = 0; r < rounds; ++r) for (int p = 0; p < 4; ++p) v[p].push_back(run(dir, p, r * 4 + p, nfiles, sz));
	for (int p = 0; p < 4; ++p) { std::sort(v[p].begin(), v[p].end()); printf("%-36s median %.1f ms  min %.1f  max %.1f\n", names[p], v[p][v[p].size()/2], v[p].front(), v[p].back()); }
	system(("rm -rf " + dir).c_str());
}
