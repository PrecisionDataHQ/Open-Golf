"""Package the fixed NativeActivity/arm64 library, with a post-strip admission stamp."""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, zipfile
ROOT=Path(__file__).resolve().parents[1]
binary=ROOT/'build/golf/android/libjoyorbit_golf.so'
stamp=json.loads((binary.parent/'source.json').read_text())
if stamp.get('source')!='b9029f1e80c3110ae3d4fc9300854bdaeef96508' or stamp.get('elf_sha256')!=hashlib.file_digest(binary.open('rb'),'sha256').hexdigest():raise SystemExit('Build the pinned golf source first.')
module=ROOT/'build/golf-embedded';libs=module/'jniLibs/arm64-v8a';libs.mkdir(parents=True,exist_ok=True)
shutil.copy2(binary,libs/binary.name)
java=module/'java/io/precisiondata/joyorbit/golf';java.mkdir(parents=True,exist_ok=True)
shutil.copy2(ROOT/'android/golf/JoyOrbitGolfActivity.java',java)
assets=module/'assets/joyorbit/golf';assets.mkdir(parents=True,exist_ok=True)
(assets/'source.json').write_text(json.dumps(stamp))
template=ROOT/'build/android-apk'
shutil.copytree(template/'gradle',module/'gradle',dirs_exist_ok=True);shutil.copy2(template/'gradlew',module/'gradlew')
(module/'settings.gradle').write_text("pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }\ndependencyResolutionManagement { repositories { google(); mavenCentral() } }\nrootProject.name='golf-embedded'\n")
(module/'local.properties').write_text('sdk.dir='+os.environ['ANDROID_SDK_ROOT']+'\n')
(module/'AndroidManifest.xml').write_text('''<manifest xmlns:android="http://schemas.android.com/apk/res/android"><application>
<activity android:name="io.precisiondata.joyorbit.golf.JoyOrbitGolfActivity" android:process=":golf" android:exported="true" android:launchMode="singleInstance" android:screenOrientation="landscape" android:enableOnBackInvokedCallback="false" android:configChanges="orientation|keyboardHidden|keyboard|screenSize|smallestScreenSize|screenLayout|uiMode|navigation" android:theme="@android:style/Theme.NoTitleBar.Fullscreen">
<meta-data android:name="android.app.lib_name" android:value="joyorbit_golf"/>
</activity></application></manifest>''')
(module/'build.gradle').write_text('''plugins { id 'com.android.library' version '9.1.0' }
android {
 namespace 'io.precisiondata.joyorbit.golf'
 compileSdk 36
 ndkVersion '29.0.14206865'
 defaultConfig { minSdk 24 }
 sourceSets { main { manifest.srcFile 'AndroidManifest.xml'; java.srcDirs=['java']; jniLibs.srcDirs=['jniLibs']; assets.srcDirs=['assets'] } }
}''')
subprocess.run(['sh',str(module/'gradlew'),'-p',str(module),'assembleDebug','--no-daemon'],check=True)
output=ROOT/'build/android-apk/libs/joyorbit-golf.aar'
with zipfile.ZipFile(module/'build/outputs/aar/golf-embedded-debug.aar') as source,zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as archive:
    compiled=source.read('jni/arm64-v8a/libjoyorbit_golf.so');stamp['binary_sha256']=hashlib.sha256(compiled).hexdigest()
    for entry in source.infolist():archive.writestr(entry,json.dumps(stamp).encode() if entry.filename=='assets/joyorbit/golf/source.json' else source.read(entry.filename))
print('Open Golf packaged locally; verify actual controls, hole recovery and both return paths.')
