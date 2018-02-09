# Homebrew Formula for Xapiand
# Usage: brew install --HEAD https://github.com/Kronuz/Xapiand/raw/master/contrib/homebrew/xapiand.rb

require 'formula'

class Xapiand < Formula
  head 'git://github.com/Kronuz/Xapiand.git', :using => :git
  homepage 'https://github.com/Kronuz/Xapiand'

  depends_on 'cmake'

  def install
    mkdir "build" do
      system "cmake", "..", *std_cmake_args
      system "make", "install"
    end
  end
end
