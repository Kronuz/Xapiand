# Homebrew formula for Xapiand (build-from-source).
#
# This is the canonical source for the formula. To publish it, copy it into the
# tap repository the docs point at (github.com/Kronuz/homebrew-tap) as
# Formula/xapiand.rb, so `brew install Kronuz/tap/xapiand` resolves.
#
# This formula is intentionally head-only: it builds from source with no pinned
# stable version, so nothing here ever goes stale and there is no url/sha256 to
# bump. Install it locally with:
#
#     brew install --HEAD ./contrib/homebrew/xapiand.rb
#
# At release time the bottling workflow (.github/actions/build-bottle) copies this
# formula and injects the tag being built as a stable `url` + a fresh source
# `sha256`, so a release's bottles always come from that release's source. Nothing
# to bump by hand, ever.
class Xapiand < Formula
  desc "RESTful search engine"
  homepage "https://github.com/Kronuz/Xapiand"
  license "MIT"
  head "https://github.com/Kronuz/Xapiand.git", branch: "master"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build
  depends_on "tcl-tk" => :build
  depends_on "asio"
  depends_on "icu4c"
  depends_on "zstd"

  def install
    # icu4c is keg-only on Homebrew; point pkg-config (and thus CMake's ICU probe)
    # at it so the build finds ICU.
    ENV.prepend_path "PKG_CONFIG_PATH", Formula["icu4c"].opt_lib/"pkgconfig"
    # This formula fetches its Kronuz-family deps via FetchContent, which Homebrew
    # traps by default. HOMEBREW_ALLOW_FETCHCONTENT is Homebrew's sanctioned opt-out
    # (the trap returns early when it's ON). Fine for a personal tap.
    system "cmake", "-S", ".", "-B", "build", "-GNinja",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DCMAKE_CXX_STANDARD=20",
           "-DCMAKE_CXX_STANDARD_REQUIRED=ON",
           "-DLTO=OFF",
           "-DASIO_INCLUDE_DIR=#{Formula["asio"].opt_include}",
           "-DHOMEBREW_ALLOW_FETCHCONTENT=ON",
           *std_cmake_args
    # LTO is off for the bottle build: its per-TU bitcode makes the heavy TUs
    # (search_views.cc) memory-hungry enough to OOM the smaller macOS runners and
    # crawls on Intel. Bounding parallelism to Homebrew's job count is a further
    # safety net without changing the produced binaries.
    system "cmake", "--build", "build", "--parallel", ENV.make_jobs.to_s
    system "cmake", "--install", "build"
  end

  test do
    assert_match "Xapiand", shell_output("#{bin}/xapiand --version")
  end
end
