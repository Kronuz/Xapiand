# Homebrew Formula for Xapiand
# Usage: brew install --HEAD https://github.com/Kronuz/Xapiand/raw/master/contrib/homebrew/xapiand.rb

class Xapiand < Formula
  desc "Xapiand: A RESTful Search Engine"
  homepage "http://kronuz.io/Xapiand"
  url "https://github.com/Kronuz/Xapiand/archive/v1.0.0.tar.gz"
  sha256 "c2aba1b5a0c677193bb4fcb2013b6f87f2fe43d0117f7cfcc0e885bf109a7676"
  head "https://github.com/Kronuz/Xapiand.git"

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "xapian"

  def install
    mkdir "build" do
      system "cmake", "..", "-DCCACHE_FOUND=CCACHE_FOUND-NOTFOUND", *std_cmake_args
      system "make", "install"
    end
  end
end
