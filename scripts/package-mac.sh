#!/bin/bash
# =============================================================================
#  package-mac.sh: builds a zip of the AU and VST3 to send to someone
#
#  A Release build for both Intel and Apple Silicon, signed "ad hoc" (no Apple
#  Developer ID yet), zipped with an install note and the licences.
#  Run it from anywhere:  scripts/package-mac.sh
#  Why each step: documentation/plugin-identity-and-macos.md section 6
# =============================================================================
set -euo pipefail                       # stop at the first failing command

cd "$(dirname "$0")/.."                 # the repo's root, wherever this was run from

version=$(sed -n 's/^project(SimpleGain VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
name="simply gain"
build="build-release"                   # its own folder: the everyday Debug build is untouched
artefacts="$build/SimpleGain_artefacts/Release"
stage="$build/package/$name $version"
zip="dist/simply-gain-$version-mac.zip"

# 1. Build: Release, for both kinds of Mac, and don't install over this Mac's own plugins.
cmake -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DSIMPLEGAIN_INSTALL_AFTER_BUILD=OFF
cmake --build "$build" --target SimpleGain_AU SimpleGain_VST3 -j 8

# 2. Gather the plugins, the install note and the licences.
rm -rf "$build/package"
mkdir -p "$stage/Licences"
cp -R "$artefacts/AU/$name.component" "$artefacts/VST3/$name.vst3" "$stage/"
cp "packaging/How to install.txt" "$stage/"
cp LICENSE                                    "$stage/Licences/simply gain (MIT).txt"
cp assets/fonts/Inter-OFL.txt                 "$stage/Licences/Inter font (OFL).txt"
cp assets/fonts/JetBrainsMono-OFL.txt         "$stage/Licences/JetBrains Mono font (OFL).txt"
cp assets/fonts/Sora-OFL.txt                  "$stage/Licences/Sora font (OFL).txt"
cp libs/JUCE/LICENSE.md                       "$stage/Licences/JUCE.md"
cp libs/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt "$stage/Licences/VST3 SDK.txt"
cp libs/JUCE/modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt             "$stage/Licences/Audio Unit SDK.txt"

# 3. Sign ad hoc: Apple Silicon Macs refuse to run code with no signature at all.
#    "-" means no identity, so macOS still treats it as unchecked when it's downloaded.
for bundle in "$stage/$name.component" "$stage/$name.vst3"; do
    codesign --force --sign - "$bundle"
    codesign --verify --strict "$bundle"
done

# 4. Check it's what we meant to send: both chips, and the oldest macOS we promise.
binary="$stage/$name.component/Contents/MacOS/$name"
archs=$(lipo -archs "$binary")
minos=$(otool -l "$binary" | awk '/minos/ { print $2; exit }')
[[ "$archs" == *x86_64* && "$archs" == *arm64* ]] || { echo "Not universal: $archs" >&2; exit 1; }
echo "Architectures: $archs. Oldest macOS: $minos."

# 5. Zip with ditto, which keeps a bundle's links and permissions intact (plain zip can break them).
mkdir -p dist
rm -f "$zip"
ditto -c -k --keepParent "$stage" "$zip"
echo "Made $zip ($(du -h "$zip" | cut -f1 | tr -d ' '))"
