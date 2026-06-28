#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

fail()
{
	echo "architecture validation failed: $1" >&2
	exit 1
}

if find . -maxdepth 1 -name '*.uplugin' -print -quit | grep -q .; then
	fail "the OpenMobile family root must not contain a .uplugin"
fi

descriptor_count="$(find Foundation Native Services Providers -name '*.uplugin' -type f | wc -l | tr -d ' ')"
if [[ "$descriptor_count" -ne 8 ]]; then
	fail "expected 8 implemented plugin descriptors, found $descriptor_count"
fi

if find Foundation Native Services -path '*/Source/*' -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.cs' \) -exec grep -Il . {} + \
	| xargs grep -lE 'GoogleMobileAds|UserMessagingPlatform|play-services-ads' >/dev/null 2>&1; then
	fail "vendor SDK references escaped the provider plugin"
fi

if grep -R -nE '#include[[:space:]]*[<\"](jni\.h|UIKit/|Photos/|GoogleMobileAds/)' \
	Foundation/*/Source/*/Public Native/*/Source/*/Public Services/*/Source/*/Public 2>/dev/null; then
	fail "a public API header leaks native or vendor SDK headers"
fi

if grep -R -n '"UMG"' Foundation Native Services Providers --include='*.Build.cs'; then
	fail "runtime plugins must not depend on sample UI"
fi

grep -q '"../../Foundation"' Tests/OpenMobileTestHost/OpenMobileTestHost.uproject \
	|| fail "test host is not wired to the repository plugin directories"

grep -q '"Name": "OpenMobileAds"' Providers/Ads/OpenMobileAdsAdMob/OpenMobileAdsAdMob.uplugin \
	|| fail "AdMob does not declare its OpenMobileAds plugin dependency"

echo "OpenMobile architecture validation passed ($descriptor_count plugins)."
