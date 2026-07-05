# Homebrew formula for Xapiand (build-from-source).
#
# This is the canonical source for the formula. To publish it, copy it into the
# tap repository the docs point at (github.com/Kronuz/homebrew-tap) as
# Formula/xapiand.rb, so `brew install Kronuz/tap/xapiand` resolves.
#
# At release time, set `url` to the pushed tag's source tarball and fill `sha256`:
#   curl -sL https://github.com/Kronuz/Xapiand/archive/refs/tags/v1.0.0-alpha.1.tar.gz | shasum -a 256
#
# Pre-built bottles are a later step (brew test-bot / a bottling workflow); this
# formula builds from source.
class Xapiand < Formula
  desc "RESTful search engine"
  homepage "https://github.com/Kronuz/Xapiand"
  url "https://github.com/Kronuz/Xapiand/archive/refs/tags/v1.0.0-alpha.1.tar.gz"
  sha256 "bf85efc4dbfe3a8520a23b6e929ec59150289b54a1708ed5f90ac50f81c6805a"
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
    system "cmake", "-S", ".", "-B", "build", "-GNinja",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DASIO_INCLUDE_DIR=#{Formula["asio"].opt_include}",
           *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match "Xapiand", shell_output("#{bin}/xapiand --version")
  end
end
