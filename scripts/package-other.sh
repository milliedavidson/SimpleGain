#!/bin/bash
# =============================================================================
#  package-other.sh: zips up the Windows or Linux build to attach to a release
#
#  Run it from the CI workflow (.github/workflows/release.yml) after a Release build
#  into build-release/:   scripts/package-other.sh windows    or    scripts/package-other.sh linux
#  The Mac zip has its own script: scripts/package-mac.sh
#  Why each step: documentation/ci-and-github-actions.md section 4
# =============================================================================
set -euo pipefail                       # stop at the first failing command

platform=${1:?usage: package-other.sh windows|linux}

cd "$(dirname "$0")/.."                 # the repo's root, wherever this was run from

version=$(sed -n 's/^project(SimpleGain VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
name="simply gain"
artefacts="build-release/SimpleGain_artefacts/Release"
stage="dist/stage/$name $version"       # inside the repo: Windows' cmake can't read Git Bash's /tmp

case "$platform" in
    windows)
        standalone="$name.exe"
        note="packaging/How to install - Windows.txt"
        out="simply-gain-$version-windows.zip"
        format="--format=zip" ;;
    linux)
        standalone="$name"
        note="packaging/How to install - Linux.txt"
        out="simply-gain-$version-linux.tar.gz"
        format="" ;;
    *)
        echo "Unknown platform: $platform" >&2; exit 1 ;;
esac

# 1. Gather the VST3, the standalone app, the install note and the licences.
rm -rf dist/stage
mkdir -p "$stage/Licences"
cp -R "$artefacts/VST3/$name.vst3" "$stage/"
cp "$artefacts/Standalone/$standalone" "$stage/"
cp "$note" "$stage/How to install.txt"
cp LICENSE                                    "$stage/Licences/simply gain (MIT).txt"
cp assets/fonts/Inter-OFL.txt                 "$stage/Licences/Inter font (OFL).txt"
cp assets/fonts/JetBrainsMono-OFL.txt         "$stage/Licences/JetBrains Mono font (OFL).txt"
cp assets/fonts/Sora-OFL.txt                  "$stage/Licences/Sora font (OFL).txt"
cp libs/JUCE/LICENSE.md                       "$stage/Licences/JUCE.md"
cp libs/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt "$stage/Licences/VST3 SDK.txt"

# 2. Archive it. `cmake -E tar` makes the same zip or tar.gz on every platform, with no extra tools.
mkdir -p dist
rm -f "dist/$out"
(cd dist/stage && cmake -E tar cf "../$out" $format "$name $version")
rm -rf dist/stage
echo "Made dist/$out"
