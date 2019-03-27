#!/bin/sh
set -eux

if [[ "${TRAVIS_OS_NAME}" == "osx" ]]; then
	# Under OSX, build a bottle:
	set -o pipefail

	PACKAGE_ROOT=.
	PACKAGE_VERSION=$(grep '^set (PACKAGE_VERSION' CMakeLists.txt | sed 's/[^.0-9]//g')
	PACKAGE_HASH=$(git --git-dir .git rev-parse --short HEAD)

	# First, get and link Kronuz Homebrew Tap
	git clone --depth=1 "https://${GH_TOKEN}@github.com/Kronuz/homebrew-tap.git"
	cd homebrew-tap

	mkdir -p /usr/local/Homebrew/Library/Taps/kronuz
	ln -fs "${PWD}" /usr/local/Homebrew/Library/Taps/kronuz

	PACKAGE_URL="https://github.com/Kronuz/Xapiand/archive/v${PACKAGE_VERSION}.tar.gz"
	mkdir -p ~/Library/Caches/Homebrew
	PACKAGE_FILENAME=~/"Library/Caches/Homebrew/xapiand--${PACKAGE_VERSION}.tar.gz"
	test -e "$PACKAGE_FILENAME" || curl --silent -L "${PACKAGE_URL}" -o "${PACKAGE_FILENAME}"
	PACKAGE_SHA256=$(shasum -a 256 "${PACKAGE_FILENAME}" | cut -d' ' -f 1)

	# Configure git
	git config --global user.email || git config --global user.email "travis@travis-ci.org"
	git config --global user.name || git config --global user.name "Travis CI"

	brew update

	# Update Formula
	sed -i '' "s#^  url .*#  url \"${PACKAGE_URL}\"#" Formula/xapiand.rb
	sed -i '' "s#^  sha256 .*#  sha256 \"${PACKAGE_SHA256}\"#" Formula/xapiand.rb

	# Build the Bottle
	HOMEBREW_PACKAGE_HASH=$PACKAGE_HASH brew install --build-bottle --verbose Kronuz/tap/xapiand
	PACKAGE_BOTTLE_SHA256=$(brew bottle --no-rebuild Kronuz/tap/xapiand | grep sha256)
	PACKAGE_BOTTLE_SHA256_NL=$(echo '\'; echo "$PACKAGE_BOTTLE_SHA256")
	PACKAGE_TYPE=$(echo $PACKAGE_BOTTLE_SHA256 | awk '{ print $4 }')
	PACKAGE_TYPE_EXT=$(echo $PACKAGE_TYPE | tr ':' '.')
	PACKAGE_BOTTLE="xapiand-${PACKAGE_VERSION}${PACKAGE_TYPE_EXT}.bottle.tar.gz"

	# The building could have taken several minutes, so we reset the modified
	# file, pull changes and modify again adding the new Bottle
	git checkout Formula/xapiand.rb
	git pull
	sed -i '' "s#^  url .*#  url \"${PACKAGE_URL}\"#" Formula/xapiand.rb
	sed -i '' "s#^  sha256 .*#  sha256 \"${PACKAGE_SHA256}\"#" Formula/xapiand.rb
	grep -v $PACKAGE_TYPE Formula/xapiand.rb > Formula/xapiand.rb.tmp
	sed "s#^    cellar :any#    cellar :any${PACKAGE_BOTTLE_SHA256_NL}#" Formula/xapiand.rb.tmp > Formula/xapiand.rb
	rm -f Formula/xapiand.rb.tmp

	# Commit and push the Formula
	git add -u
	git commit --message "Xapiand updated to v${PACKAGE_VERSION} via Travis build ${TRAVIS_BUILD_NUMBER}"
	git push --quiet origin master

	# Add, commit and push Bottle
	git fetch --depth 1 origin gh-pages:gh-pages
	git checkout gh-pages
	mv "xapiand--${PACKAGE_VERSION}${PACKAGE_TYPE_EXT}.bottle.tar.gz" "${PACKAGE_BOTTLE}"
	git add "${PACKAGE_BOTTLE}"
	git commit --message "${PACKAGE_BOTTLE}"
	git push --quiet origin gh-pages

else
	# Everywhere else build as usual:
	CC=clang-7 && CXX=clang-7 cmake  -S . -B build
	cmake --build build
fi
