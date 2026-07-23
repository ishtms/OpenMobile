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
if [[ "$descriptor_count" -lt 10 ]]; then
	fail "expected at least 10 implemented plugin descriptors, found $descriptor_count"
fi

for descriptor in \
	Foundation/OpenMobileCore/OpenMobileCore.uplugin \
	Foundation/OpenMobilePermissions/OpenMobilePermissions.uplugin \
	Native/OpenMobileSensors/OpenMobileSensors.uplugin
do
	[[ -f "$descriptor" ]] || fail "missing required descriptor $descriptor"
done

if find Foundation Native Services -path '*/Source/*' -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.cs' \) -exec grep -Il . {} + \
	| xargs grep -lE 'GoogleMobileAds|UserMessagingPlatform|play-services-ads' >/dev/null 2>&1; then
	fail "vendor SDK references escaped the provider plugin"
fi

if grep -R -nE '#include[[:space:]]*[<\"](jni\.h|UIKit/|Photos/|GoogleMobileAds/)' \
	Foundation/*/Source/*/Public Native/*/Source/*/Public Services/*/Source/*/Public 2>/dev/null; then
	fail "a public API header leaks native or vendor SDK headers"
fi

if grep -R -n '"UMG"' \
	Foundation/OpenMobileCore/Source \
	Foundation/OpenMobilePermissions/Source \
	Native/OpenMobileSensors/Source/OpenMobileSensors \
	--include='*.Build.cs'; then
	fail "foundation and Sensors runtime modules must not depend on UI"
fi

grep -q '"../../Foundation"' Tests/OpenMobileTestHost/OpenMobileTestHost.uproject \
	|| fail "test host is not wired to the repository plugin directories"

grep -q '"Name": "OpenMobileAds"' Providers/Ads/OpenMobileAdsAdMob/OpenMobileAdsAdMob.uplugin \
	|| fail "AdMob does not declare its OpenMobileAds plugin dependency"

sensors_descriptor="Native/OpenMobileSensors/OpenMobileSensors.uplugin"
sensors_runtime_rules="Native/OpenMobileSensors/Source/OpenMobileSensors/OpenMobileSensors.Build.cs"
sensors_android_rules="Native/OpenMobileSensors/Source/OpenMobileSensorsAndroid/OpenMobileSensorsAndroid.Build.cs"
sensors_ios_rules="Native/OpenMobileSensors/Source/OpenMobileSensorsIOS/OpenMobileSensorsIOS.Build.cs"

for dependency in OpenMobileCore OpenMobilePermissions
do
	grep -q "\"Name\": \"$dependency\"" "$sensors_descriptor" \
		|| fail "Sensors descriptor does not declare $dependency"
	grep -q "\"$dependency\"" "$sensors_runtime_rules" \
		|| fail "Sensors runtime module does not depend on $dependency"
done

grep -q '"Name": "OpenMobileSensorsAndroid"' "$sensors_descriptor" \
	|| fail "Sensors descriptor is missing the Android module"
grep -q '"PlatformAllowList": \["Android"\]' "$sensors_descriptor" \
	|| fail "Sensors Android module is not platform isolated"
grep -q '"Name": "OpenMobileSensorsIOS"' "$sensors_descriptor" \
	|| fail "Sensors descriptor is missing the iOS module"
grep -q '"PlatformAllowList": \["IOS"\]' "$sensors_descriptor" \
	|| fail "Sensors iOS module is not platform isolated"

grep -q 'OpenMobileSensors_Android_UPL.xml' "$sensors_android_rules" \
	|| fail "Sensors Android module does not register its packaging rules"
grep -q '"CoreMotion"' "$sensors_ios_rules" \
	|| fail "Sensors iOS module does not link CoreMotion"
grep -q 'OpenMobileSensorsPrivacy.bundle' "$sensors_ios_rules" \
	|| fail "Sensors iOS module does not package its privacy manifest"

if grep -R -nE 'GoogleMobileAds|play-services|Firebase|IronSource|OpenMobileAds|OpenMobileMedia|OpenMobileHaptics|CLLocationManager' \
	Native/OpenMobileSensors/Source \
	Native/OpenMobileSensors/OpenMobileSensors.uplugin \
	--include='*.h' --include='*.cpp' --include='*.mm' --include='*.cs' --include='*.xml' --include='*.uplugin'; then
	fail "Sensors contains an unrelated SDK or feature dependency"
fi

fixture_root="Tests/OpenMobileSensorsArchitectureFixtures"
for fixture in CoreOnly SensorsOnly SensorsPermissions SensorsActivityProvider BlueprintOnly
do
	[[ -f "$fixture_root/$fixture/OpenMobileSensors${fixture}.uproject" ]] \
		|| fail "missing Sensors architecture fixture $fixture"
done

grep -q '"Name": "OpenMobileSensors"' \
	"$fixture_root/SensorsOnly/OpenMobileSensorsSensorsOnly.uproject" \
	|| fail "Sensors-only fixture does not enable Sensors"
if grep -q '"Name": "OpenMobilePermissions"' \
	"$fixture_root/SensorsOnly/OpenMobileSensorsSensorsOnly.uproject"; then
	fail "Sensors-only fixture must rely on descriptor dependencies"
fi
grep -q '"Name": "OpenMobilePermissions"' \
	"$fixture_root/SensorsPermissions/OpenMobileSensorsSensorsPermissions.uproject" \
	|| fail "Sensors plus Permissions fixture is not explicit"
grep -q '"Name": "OpenMobileSensorsActivityProviderFixture"' \
	"$fixture_root/SensorsActivityProvider/OpenMobileSensorsSensorsActivityProvider.uproject" \
	|| fail "Sensors activity-provider fixture is not enabled"
grep -q 'IOpenMobileMotionActivityProvider' \
	"$fixture_root/SensorsActivityProvider/Plugins/OpenMobileSensorsActivityProviderFixture/Source/OpenMobileSensorsActivityProviderFixture/Private/OpenMobileSensorsActivityProviderFixtureModule.cpp" \
	|| fail "Sensors activity-provider fixture does not compile the public provider SPI"

if find "$fixture_root/BlueprintOnly" \
	\( -name Binaries -o -name Build -o -name Intermediate -o -name Saved \) -prune -o \
	-type f \( -name '*.cpp' -o -name '*.h' -o -name '*.Build.cs' -o -name '*.Target.cs' \) -print -quit | grep -q .; then
	fail "Blueprint-only Sensors fixture contains native consumer code"
fi

haptics_descriptor="Native/OpenMobileHaptics/OpenMobileHaptics.uplugin"
haptics_runtime_rules="Native/OpenMobileHaptics/Source/OpenMobileHaptics/OpenMobileHaptics.Build.cs"

[[ -f "$haptics_descriptor" ]] || fail "missing Haptics descriptor"
grep -q '"Name": "OpenMobileCore"' "$haptics_descriptor" \
	|| fail "Haptics descriptor does not declare OpenMobileCore"
grep -q '"OpenMobileCore"' "$haptics_runtime_rules" \
	|| fail "Haptics runtime module does not depend on OpenMobileCore"

for platform in Android IOS
do
	grep -q '"Name": "OpenMobileHaptics'"$platform"'"' "$haptics_descriptor" \
		|| fail "Haptics descriptor is missing the $platform module"
	grep -q '"PlatformAllowList": \["'"$platform"'"\]' "$haptics_descriptor" \
		|| fail "Haptics $platform module is not platform isolated"
done

if grep -R -nE 'AndroidJNI|jni\.h|CoreHaptics/|UIKit/|CHHaptic' \
	Native/OpenMobileHaptics/Source/OpenMobileHaptics/Public \
	--include='*.h'; then
	fail "a public Haptics header leaks platform APIs"
fi

for dependency in UMG GameplayAbilities MovieScene OpenMobileDevice OpenMobileSensors
do
	if grep -q '"'"$dependency"'"' "$haptics_runtime_rules"; then
		fail "base Haptics runtime depends on optional module $dependency"
	fi
done

for integration in OpenMobileHapticsUMG OpenMobileHapticsGameplayAbilities OpenMobileHapticsSequencer
do
	integration_descriptor="Native/$integration/$integration.uplugin"
	[[ -f "$integration_descriptor" ]] || fail "missing Haptics integration $integration"
	grep -q '"Name": "OpenMobileHaptics"' "$integration_descriptor" \
		|| fail "$integration does not declare its Haptics dependency"
done

if grep -R -n 'OpenMobileHaptics' \
	Foundation/OpenMobileCore/Source \
	Foundation/OpenMobileCore/OpenMobileCore.uplugin \
	--include='*.h' --include='*.cpp' --include='*.cs' --include='*.uplugin'; then
	fail "OpenMobileCore depends on Haptics"
fi

haptics_blueprint_fixture="Tests/OpenMobileHapticsArchitectureFixtures/BlueprintOnly"
[[ -f "$haptics_blueprint_fixture/OpenMobileHapticsBlueprintOnly.uproject" ]] \
	|| fail "missing Blueprint-only Haptics fixture"
if find "$haptics_blueprint_fixture" \
	\( -name Binaries -o -name Build -o -name Intermediate -o -name Saved \) -prune -o \
	-type f \( -name '*.cpp' -o -name '*.h' -o -name '*.Build.cs' -o -name '*.Target.cs' \) -print -quit | grep -q .; then
	fail "Blueprint-only Haptics fixture contains native consumer code"
fi

echo "OpenMobile architecture validation passed ($descriptor_count plugins)."
