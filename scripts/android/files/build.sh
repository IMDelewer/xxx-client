#!/bin/bash
set -e

COLOR_RED="\e[1;31m"
COLOR_RESET="\e[0m"

if [ -z ${1+x} ]; then
        printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass APK name to build script"
        exit 1
fi

if [ -z ${2+x} ]; then
        printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass package name to build script"
        exit 1
fi

if [ -z ${3+x} ]; then
        printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass build type to build script: Debug, Release"
        exit 1
fi

APK_BASENAME="$1"
APK_PACKAGE_NAME="$2"
APK_BUILD_TYPE="$3"

if [[ "${APK_BUILD_TYPE}" == "Debug" ]]; then
        RELEASE_TYPE_NAME=debug
elif [[ "${APK_BUILD_TYPE}" == "Release" ]]; then
        RELEASE_TYPE_NAME=release
else
        printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass build type to build script: Debug, Release"
        exit 1
fi

APK_PACKAGE_FOLDER=$(echo "$APK_PACKAGE_NAME" | sed 's/\./\//g')

sed -i "s/DDNet/${APK_BASENAME}/g" settings.gradle

sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" build.gradle

sed -i "s/TW_VERSION_CODE/${TW_VERSION_CODE}/g" build.gradle
sed -i "s/TW_VERSION_NAME/${TW_VERSION_NAME}/g" build.gradle

for f in $(find src/main/res -name 'strings.xml'); do
        sed -i "s/DDNet/${APK_BASENAME}/g" "$f"
done

sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" src/main/res/xml/shortcuts.xml

sed -i "s/\"DDNet\"/\"${APK_BASENAME}\"/g" src/main/AndroidManifest.xml
sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" src/main/AndroidManifest.xml

if [ "${APK_PACKAGE_FOLDER}" != "org/ddnet/client" ]; then
        # Intermediate directories must exist before mv, otherwise the java package
        # is left under org/ddnet/client and the manifest points to a missing activity.
        rm -rf "src/main/java/${APK_PACKAGE_FOLDER}"
        mkdir -p "src/main/java/${APK_PACKAGE_FOLDER%/*}"
        mv src/main/java/org/ddnet/client "src/main/java/${APK_PACKAGE_FOLDER}"
fi

sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" src/main/java/"${APK_PACKAGE_FOLDER}"/ClientActivity.java
sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" src/main/java/"${APK_PACKAGE_FOLDER}"/ServerService.java
# SDLActivity loads the client library via getLibraries(), which upstream hardcodes
# to "DDNet". CMake outputs lib${GAME_NAME}.so (= lib${APK_BASENAME}.so), so the
# quoted string must match it. Only ClientActivity is touched: ServerService keeps
# loading "DDNet-Server" (quoted "DDNet" never matches "DDNet-Server").
sed -i "s/\"DDNet\"/\"${APK_BASENAME}\"/g" src/main/java/"${APK_PACKAGE_FOLDER}"/ClientActivity.java
sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" proguard-rules.pro

# Disable HID manager as we compile SDL without hidapi and libusb support
sed -i "s/mHIDDeviceManager = HIDDeviceManager.acquire(this);/mHIDDeviceManager=null;/g" src/main/java/org/libsdl/app/SDLActivity.java
if ! grep -q 'mHIDDeviceManager=null;' src/main/java/org/libsdl/app/SDLActivity.java; then
        printf "${COLOR_RED}%s${COLOR_RESET}\n" "WARNING: HID device manager sed did not match SDLActivity.java, hidapi may crash the app"
fi

if [[ "${APK_BUILD_TYPE}" == "Debug" ]]; then
        sed -i "s/android.enableR8.fullMode=true/android.enableR8.fullMode=false/g" gradle.properties
fi

if [[ "${APK_BUILD_TYPE}" == "Debug" ]]; then
        ./gradlew --no-daemon --warning-mode all assembleDebug
else
        ./gradlew --no-daemon --warning-mode all assembleRelease
fi
APK_SUFFIX="$RELEASE_TYPE_NAME"
if [[ "$APK_BUILD_TYPE" == "Release" && "${TW_UNSIGNED_RELEASE:-}" == "1" ]]; then
        APK_SUFFIX=release-unsigned
fi
cp "build/outputs/apk/$RELEASE_TYPE_NAME/$APK_BASENAME-$APK_SUFFIX.apk" "$APK_BASENAME.apk"
