package io.precisiondata.joyorbit.golf;

import android.app.NativeActivity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;

public final class JoyOrbitGolfActivity extends NativeActivity {
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean backHeld, backReturned;
    private static native void nativeBackCommand(int command);
    private final Runnable longBack = () -> {
        if (backHeld) backReturned = true;
    };
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() != KeyEvent.KEYCODE_BACK) return super.dispatchKeyEvent(event);
        if (event.getAction() == KeyEvent.ACTION_DOWN && event.isLongPress()) {
            backHeld = true; backReturned = true;
            handler.removeCallbacks(longBack);
        } else if (event.getAction() == KeyEvent.ACTION_DOWN && !backHeld) {
            backHeld = true; backReturned = false;
            handler.postDelayed(longBack, 650);
        } else if (event.getAction() == KeyEvent.ACTION_UP) {
            handler.removeCallbacks(longBack);
            if (backHeld && !event.isCanceled()) nativeBackCommand(backReturned ? 1 : 2);
            backHeld = false;
        }
        return true;
    }
    @Override public void onBackPressed() { nativeBackCommand(2); }
    @Override public void onDestroy() {
        handler.removeCallbacks(longBack);
        super.onDestroy();
    }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        immersive();
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused) immersive();
    }
    private void immersive() {
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }
}
