set -e
W=$(mktemp -d "$HOME/.scratch/luacmp.XXXXXX")
git clone -q --depth 1 --branch v5.4.7 https://github.com/lua/lua.git "$W/git"
echo "tag commit: $(git -C "$W/git" rev-parse HEAD)"
curl -sSL -m 120 https://www.lua.org/ftp/lua-5.4.7.tar.gz -o "$W/t.tgz"
tar xzf "$W/t.tgz" -C "$W"
n=0; bad=0
for f in "$W"/lua-5.4.7/src/*; do
  b=$(basename "$f")
  case "$b" in *.c|*.h|*.hpp) ;; *) continue;; esac
  n=$((n+1))
  if ! cmp -s "$f" "$W/git/$b"; then echo "DIFF $b"; bad=$((bad+1)); fi
done
echo "compared $n files, $bad differ"
echo "in git not in tarball src (.c/.h):"
for f in "$W"/git/*.c "$W"/git/*.h; do [ -e "$W/lua-5.4.7/src/$(basename $f)" ] || echo "  $(basename $f)"; done
ls "$W/git" | tr '\n' ' '; echo
rm -rf "$W"
