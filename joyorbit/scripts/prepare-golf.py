"""Prepare a pinned, source-built Open Golf Android engine with reviewed replacements."""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
PIN='b9029f1e80c3110ae3d4fc9300854bdaeef96508'
SOURCE=ROOT/'.sources/open-golf'
FONT_PINS={'Archivo-Bold.ttf':'951a0ebab63b1bb0d90a26c27625bda803d570dace3851fa2f1eea65852d8983','Archivo-Regular.ttf':'59fbfd8a9bb41301bfe950e7b097fd42d52af224a07761e2c1b2c959a4c9cf9a'}
def run(*args,**kwargs):subprocess.run([str(x) for x in args],check=True,**kwargs)
def replace(path,before,after,count=None):
    text=path.read_text()
    if before not in text or count is not None and text.count(before)!=count:raise RuntimeError('Pinned source mismatch: '+str(path))
    path.write_text(text.replace(before,after))
def digest(path):return hashlib.file_digest(path.open('rb'),'sha256').hexdigest()
if not SOURCE.exists():
    run('git','clone','https://github.com/PrecisionDataHQ/open-golf.git',SOURCE)
    run('git','-C',SOURCE,'checkout','--detach',PIN)
if subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','HEAD'],text=True).strip()!=PIN or subprocess.check_output(['git','-C',str(SOURCE),'status','--porcelain','--untracked-files=no'],text=True).strip():
    raise SystemExit('Preserve the changed upstream tree; use the clean pinned Open Golf source.')
for name,expected in FONT_PINS.items():
    if digest(ROOT/'theme/assets/fonts'/name)!=expected:raise SystemExit('Use the pinned Archivo fonts prepared for the JoyOrbit library.')
recipe_files=['prepare-golf.py','golf-font-atlas.c','golf-sounds.py','golf-controls.c','golf-controls.h','golf-vorbis-check.c']
recipe=hashlib.sha256(''.join(digest(ROOT/'scripts'/name) for name in recipe_files).encode()).hexdigest()
stage=ROOT/'build'/('open-golf-source-'+recipe[:12])
work=ROOT/'build/golf';work.mkdir(parents=True,exist_ok=True)
stamp=stage/'joyorbit-source.json'
if not stamp.exists():
    if stage.exists():raise SystemExit('Incomplete generated tree exists; preserve it before retrying.')
    shutil.copytree(SOURCE,stage,ignore=shutil.ignore_patterns('.git','.DS_Store','out','Castle-Rock.ttf*','FiraSans-Bold.ttf*','fa-solid-900.ttf*'))
    (stage/'out').mkdir(exist_ok=True)
    third=stage/'src/3rd_party'
    compiler=work/'font-atlas'
    run('clang','-O2','-I',third,ROOT/'scripts/golf-font-atlas.c',third/'parson/parson.c','-lm','-o',compiler)
    for name in FONT_PINS:
        font=stage/'data/font'/name
        shutil.copy2(ROOT/'theme/assets/fonts'/name,font)
        run(compiler,font,str(font)+'.golf_data')
    for directory in ['data/config','data/ui']:
        for file in (stage/directory).rglob('*'):
            if file.is_file() and file.suffix in ['.cfg','.ui']:
                content=file.read_text().replace('data/font/FiraSans-Bold.ttf','data/font/Archivo-Bold.ttf').replace('data/font/Castle-Rock.ttf','data/font/Archivo-Regular.ttf')
                file.write_text(content)
    # Source audio has no individual provenance notice. Preserve it outside the
    # shipped data, then generate our own preferred-source WAV and runtime Oggs.
    audio=stage/'data/audio';audio.rename(stage/'legacy-source-audio');audio.mkdir()
    run(sys.executable,ROOT/'scripts/golf-sounds.py',audio)
    # Valid modern Vorbis setup packets can contain empty lookup tables. The old
    # decoder returned NULL for a zero-byte allocation and mistook that for OOM.
    replace(third/'stb/stb_vorbis.h','return sz ? malloc(sz) : NULL;','return malloc(sz ? sz : 1);',1)
    decoder=work/'effect-decoder-check'
    run('clang','-O2','-I',third,ROOT/'scripts/golf-vorbis-check.c','-lm','-o',decoder)
    run(decoder,*sorted(audio.glob('*.ogg')))
    data_source=stage/'src/common/data.c'
    replace(data_source,'    golf_audio_t *audio = (golf_audio_t*)ptr;','    golf_audio_t *audio = (golf_audio_t*)ptr;\n    audio->stb_vorbis_stream = NULL;',1)
    replace(data_source,'    memcpy(data_copy, data, data_len);','    if (!data_copy) return false;\n    memcpy(data_copy, data, data_len);',1)
    replace(data_source,'        golf_log_error("stb_vorbis_open_memory returned null");','        golf_log_error("stb_vorbis_open_memory returned null");\n        free(data_copy);\n        return false;',1)
    replace(stage/'src/common/audio.c','    golf_audio_t *audio = golf_data_get_audio(path);','    golf_audio_t *audio = golf_data_get_audio(path);\n    if (!audio || !audio->stb_vorbis_stream) return;',1)
    shutil.copy2(ROOT/'theme/licenses/Archivo-OFL.txt',stage/'data/font/Archivo-OFL.txt')
    (audio/'LICENSE.txt').write_text((stage/'LICENSE').read_text().replace('Copyright (c) 2020 Michael Gerdes','Copyright (c) 2026 Precision Data')+'\nOriginal synthesized effects; preferred source: scripts/golf-sounds.py and generated WAV files. No source game audio is bundled.\n')
    # Build cembed from preferred source; do not execute the repository's old
    # prebuilt host tool. Other shader/geometry caches retain their GLSL/OBJ source.
    tool=stage/'tools/cembed/osx/cembed'
    tool.rename(tool.with_name('cembed-legacy'))
    run('clang','-O2',stage/'tools/cembed/cembed.c','-o',tool)
    storage=stage/'src/common/storage.c'
    replace(storage,'void golf_storage_save(void) {','void golf_storage_save(void) {\n    if (!_storage_json_val) return;',1)
    replace(storage,'"%s/storage.json", internal_path','"%s/joyorbit-golf/storage.json", internal_path',2)
    replace(storage,'    // Create the internal storage directory if it doesn\'t exist yet','    char golf_directory[1024];\n    snprintf(golf_directory,sizeof(golf_directory),"%s/joyorbit-golf",internal_path);\n    mkdir(golf_directory,0700);\n\n    // Create the internal storage directory if it doesn\'t exist yet',1)
    replace(storage,'''    FILE *f = fopen(file, "wb");    
    fwrite(buf, buf_size, 1, f);
    fclose(f);''','''    char temporary[1100];
    if (snprintf(temporary,sizeof(temporary),"%s.new",file)>=(int)sizeof(temporary)) return;
    FILE *f=fopen(temporary,"wb");
    if (!f) return;
    size_t written=fwrite(buf,1,(size_t)buf_size,f);
    int closed=fclose(f);
    if (written==(size_t)buf_size && closed==0) rename(temporary,file);''',1)
    replace(storage,'    int num_bytes = ftell(f);','    long length=ftell(f);\n    if (length<0 || length>1024*1024) { fclose(f);return false; }\n    int num_bytes=(int)length;',1)
    replace(storage,'    if (ret == -1) {','    if (ret != num_bytes) {',1)
    replace(storage,'    *num = (float)json_value_get_number(json_val);\n    return true;','    *num = (float)json_value_get_number(json_val);\n    return isfinite(*num);',1)
    storage.write_text('#include <math.h>\n'+storage.read_text())
    replace(storage,'static JSON_Value *_storage_json_val;','''static JSON_Value *_storage_json_val;
static bool joyorbit_storage_readonly;
static void joyorbit_preserve_invalid_storage(void) {
#if GOLF_PLATFORM_ANDROID
    char path[1024], legacy[1100];
    const ANativeActivity *activity=(const ANativeActivity*)sapp_android_get_native_activity();
    snprintf(path,sizeof(path),"%s/joyorbit-golf/storage.json",activity->internalDataPath);
    struct stat info;
    if (stat(path,&info)!=0) return;
    for (unsigned i=0;i<10000;i++) {
        snprintf(legacy,sizeof(legacy),"%s.legacy-invalid-%u",path,i);
        if (stat(legacy,&info)==0) continue;
        if (rename(path,legacy)!=0) joyorbit_storage_readonly=true;
        return;
    }
    joyorbit_storage_readonly=true;
#endif
}''',1)
    replace(storage,'    if (!_storage_json_val) return;','    if (!_storage_json_val || joyorbit_storage_readonly) return;',1)
    replace(storage,'        if (!obj) {\n            _storage_json_val = json_value_init_object();','        if (!obj) {\n            joyorbit_preserve_invalid_storage();\n            json_value_free(_storage_json_val);\n            _storage_json_val = json_value_init_object();',1)
    replace(storage,'    else {\n        _storage_json_val = json_value_init_object();','    else {\n        joyorbit_preserve_invalid_storage();\n        _storage_json_val = json_value_init_object();',1)
    shutil.copy2(ROOT/'scripts/golf-controls.c',stage/'src/common/joyorbit_golf_controls.c')
    shutil.copy2(ROOT/'scripts/golf-controls.h',stage/'src/common/joyorbit_golf_controls.h')
    main=stage/'src/golf/main.c'
    main.write_text('#include "common/joyorbit_golf_controls.h"\n'+main.read_text())
    replace(main,'    golf_graphics_begin_frame(dt);','    if (joyorbit_golf_control_frame(dt)) return;\n    golf_graphics_begin_frame(dt);',1)
    replace(main,'    golf_update(dt);','    golf_update(joyorbit_golf_is_paused() ? 0 : dt);',1)
    replace(main,'    golf_draw();','    golf_draw();\n    joyorbit_golf_control_draw();',1)
    replace(main,'    golf_inputs_handle_event(event);','    joyorbit_golf_control_event(event);\n    golf_inputs_handle_event(event);',1)
    replace(main,'static void cleanup(void) {','static void cleanup(void) {\n    joyorbit_golf_save();',1)
    graphics=stage/'src/common/graphics.c'
    replace(graphics,'V2((float)sapp_width(), (float)sapp_width())','V2((float)sapp_width(), (float)sapp_height())',2)
    sokol=stage/'src/3rd_party/sokol/sokol_app.h'
    replace(sokol,'#include <android/native_activity.h>','#include <android/native_activity.h>\n#include "common/joyorbit_golf_controls.h"',1)
    replace(sokol,'''    int32_t action_idx = AMotionEvent_getAction(e);''','''    if ((AInputEvent_getSource(e) & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK) {
        joyorbit_golf_pad_axes(AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_X, 0), AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_Y, 0),
            AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_Z, 0), AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_RZ, 0),
            AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_HAT_X, 0), AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_HAT_Y, 0));
        return true;
    }
    int32_t action_idx = AMotionEvent_getAction(e);''',1)
    replace(sokol,'    if (AKeyEvent_getKeyCode(e) == AKEYCODE_BACK) {','    if (joyorbit_golf_pad_key(AKeyEvent_getKeyCode(e), AKeyEvent_getAction(e) == AKEY_EVENT_ACTION_DOWN, (AKeyEvent_getFlags(e) & AKEY_EVENT_FLAG_LONG_PRESS) != 0)) return true;\n    if (AKeyEvent_getKeyCode(e) == AKEYCODE_BACK) {',1)
    replace(sokol,'    eglSwapBuffers(_sapp.android.display, _sapp.android.surface);','    eglSwapBuffers(_sapp.android.display, _sapp.android.surface);\n    if (_sapp.quit_ordered) _sapp_android_shutdown();',1)
    cmake=stage/'CMakeLists.txt'
    cmake.write_text(cmake.read_text()+'\ntarget_sources(golf PRIVATE ${CMAKE_SOURCE_DIR}/src/common/joyorbit_golf_controls.c)\nset_target_properties(golf PROPERTIES OUTPUT_NAME joyorbit_golf)\ntarget_link_options(golf PRIVATE -Wl,-z,max-page-size=16384)\n')
    stamp.write_text(json.dumps({'source':PIN,'recipe':recipe,'fonts':FONT_PINS,'changes':'Archivo fonts; unused source fonts and all source audio excluded; own synthesized WAV/Ogg effects; source-built cembed; private atomic Android save path; 16KiB page alignment'},indent=2)+'\n')
ndk=Path(os.environ['ANDROID_SDK_ROOT'])/'ndk/29.0.14206865'
cmake=Path(os.environ['ANDROID_SDK_ROOT'])/'cmake/3.22.1/bin/cmake'
build=work/('android-'+recipe[:12])
run(cmake,'-S',stage,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-24','-DANDROID_STL=c++_shared','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'))
run(cmake,'--build',build,'--target','golf','-j','6')
output=work/'android';output.mkdir(exist_ok=True)
binary=output/'libjoyorbit_golf.so';shutil.copy2(build/'src/golf/libjoyorbit_golf.so',binary)
data=json.loads(stamp.read_text());data['elf_sha256']=digest(binary);(output/'source.json').write_text(json.dumps(data,indent=2)+'\n')
print('Open Golf source build complete; controller, runtime and save acceptance remain required.')
