#!/usr/bin/env bash
# Package libtfs-deps-<version>-<platform>.tar.gz: the transitive static libs
# consumers link against (DEPS_LIBS) plus a curated include/ tree (header
# dirs/files and the transitive boost closure), so libtfs + libtfs-deps is
# self-contained — no vcpkg downstream. Fails loudly when an expected lib or
# header is missing.
#
# Invoked by .github/workflows/release.yml on POSIX legs:
#   PLATFORM=<platform> bash .github/scripts/package-deps.sh
# and inside the musl-arm64 docker container (paths are container-relative).
#
# Inputs (env):
#   PLATFORM            required (e.g. linux-gnu-x86_64)
#   PKG_VERSION         required (e.g. 0.12.9) — NOT "VERSION": on Windows the
#                       runner merges env case-insensitively and a step-level
#                       VERSION collides with the GITHUB_ENV-provided lowercase
#                       "version" (the step value is silently dropped)
#   DEPS_LIBS / DEPS_SHARES / DEPS_HEADER_DIRS / DEPS_HEADER_FILES (workflow env)
#   SHIP_OPENSSL        "1" to include libcrypto/libssl + openssl headers (non-macOS)
#   SHIP_TOOLCHAIN_RT   "1" to include libstdc++/libgcc/libgcc_eh archives (musl)
#   WORKDIR             repo root (default: $PWD)

set -euo pipefail

PLATFORM="${PLATFORM:?required}"
PKG_VERSION="${PKG_VERSION:?required}"
WORKDIR="${WORKDIR:-$PWD}"
SHIP_OPENSSL="${SHIP_OPENSSL:-1}"
SHIP_TOOLCHAIN_RT="${SHIP_TOOLCHAIN_RT:-0}"

cd "$WORKDIR"

VILIB=$(ls -d build/vcpkg_installed/*/lib | head -1)
VIROOT=$(dirname "$VILIB")
LIBS="$DEPS_LIBS"
SHARES="$DEPS_SHARES"
HEADER_DIRS="$DEPS_HEADER_DIRS"
if [ "$SHIP_OPENSSL" = "1" ]; then
  LIBS="$LIBS libcrypto.a libssl.a"
  SHARES="$SHARES openssl"
  # Ship the matching OpenSSL headers with the archives: tebako's ruby build
  # (ext/openssl) configures against the triplet include dir; without them it
  # fell back to system headers whose opaque structs mismatched the shipped
  # 3.x libs and broke the build.
  HEADER_DIRS="$HEADER_DIRS openssl"
fi

rm -rf deps-stage
mkdir -p deps-stage/lib deps-stage/share

if [ "$SHIP_TOOLCHAIN_RT" = "1" ]; then
  # Ship the build's C++/C runtime archives so tebako's musl link
  # (-l:libstdc++.a, -lgcc_eh, -l:libgcc.a) resolves the dep set's references
  # (e.g. _M_replace_cold from libstdc++ >= 13) on containers with an older
  # toolchain (tebako alpine-3.17, gcc-12).
  for tl in libstdc++.a libgcc.a libgcc_eh.a; do
    src=$(g++ -print-file-name=$tl)
    if [ ! -f "$src" ]; then
      echo "ERROR: expected toolchain archive missing: $tl" >&2
      exit 1
    fi
    cp "$src" deps-stage/lib/
  done
fi

resolve() {
  local base="$1" stem f
  stem="${base%.a}"
  case "$base" in
    libz.a)   cand="$VILIB/libz.a $VILIB/libzlib.a" ;;
    libbz2.a) cand="$VILIB/libbz2.a $VILIB/libbzip2.a" ;;
    *)        cand="$VILIB/$base" ;;
  esac
  for f in $cand; do [ -f "$f" ] && { echo "$f"; return 0; }; done
  for f in "$VILIB/${stem}-mt.a" "$VILIB/${stem}"*.a; do
    [ -f "$f" ] && { echo "$f"; return 0; }
  done
  return 1
}

for l in $LIBS; do
  src=$(resolve "$l") || { echo "ERROR: expected dep lib missing: $VILIB/$l (incl. variants)" >&2; exit 1; }
  cp "$src" deps-stage/lib/
done

for s in $SHARES; do
  if [ -d "$VIROOT/share/$s" ]; then
    cp -R "$VIROOT/share/$s" deps-stage/share/
  fi
  if [ -d "$VILIB/cmake/$s" ]; then
    mkdir -p deps-stage/lib/cmake
    cp -R "$VILIB/cmake/$s" deps-stage/lib/cmake/
  fi
done

# Curated headers (DEPS_HEADER_DIRS / DEPS_HEADER_FILES) staged under include/
# so consumers can also compile against the shipped archives (gem native
# extensions need e.g. <brotli/encode.h>).
VIINC="$VIROOT/include"
mkdir -p deps-stage/include
for d in $HEADER_DIRS; do
  if [ ! -d "$VIINC/$d" ]; then
    echo "ERROR: expected dep header dir missing: $VIINC/$d" >&2
    exit 1
  fi
  cp -R "$VIINC/$d" deps-stage/include/
done
for h in $DEPS_HEADER_FILES; do
  if [ ! -f "$VIINC/$h" ]; then
    echo "ERROR: expected dep header missing: $VIINC/$h" >&2
    exit 1
  fi
  cp "$VIINC/$h" deps-stage/include/
done

# boost: stage only the transitive #include closure of boost_filesystem +
# boost_chrono — the whole boost/ tree is ~100 MB of headers consumers never
# include. MPL's preprocessed headers are seeded explicitly: they are pulled
# in via macro-computed includes (BOOST_PP_STRINGIZE(...)) that textual
# scanning cannot follow.
normalize_boost() {
  local p="$1"
  while [[ "$p" == *"/./"* || "$p" == *"/../"* || "$p" == ../* || "$p" == ./* ]]; do
    p=$(printf '%s' "$p" | sed -E -e 's|(^|/)\./|\1|g' -e 's|[^/]+/\.\./||' -e 's|^\.\./||')
  done
  printf '%s' "$p"
}
stage_boost() {
  local rel="$1" src dst inc sub
  src="$VIINC/$rel"
  dst="deps-stage/include/$rel"
  if [ -d "$src" ]; then
    for sub in "$src"/*; do
      [ -e "$sub" ] || continue
      stage_boost "$rel/$(basename "$sub")"
    done
  elif [ -f "$src" ] && [ ! -f "$dst" ]; then
    mkdir -p "$(dirname "$dst")"
    cp "$src" "$dst"
    sed -n -E 's|^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](boost/[^">]+)[">].*|\1|p' "$dst" |
      while read -r inc; do stage_boost "$inc"; done
    sed -n -E 's|^[[:space:]]*#[[:space:]]*include[[:space:]]*"([^">]+)".*|\1|p' "$dst" |
      while read -r inc; do
        case "$inc" in
          boost/*) : ;;
          *) inc=$(normalize_boost "$(dirname "$rel")/$inc") ;;
        esac
        [ -f "$VIINC/$inc" ] && stage_boost "$inc"
      done
  fi
}
for seed in boost/filesystem.hpp boost/filesystem boost/chrono.hpp boost/chrono boost/mpl/aux_/preprocessed; do
  if [ ! -e "$VIINC/$seed" ]; then
    echo "ERROR: expected boost seed missing: $VIINC/$seed" >&2
    exit 1
  fi
  stage_boost "$seed"
done

du -sh deps-stage/include deps-stage/include/boost || true
mkdir -p artifacts
tar -C deps-stage -czf "artifacts/libtfs-deps-${PKG_VERSION}-${PLATFORM}.tar.gz" lib share include
tar -tzf "artifacts/libtfs-deps-${PKG_VERSION}-${PLATFORM}.tar.gz" > /dev/null
