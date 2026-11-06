package cctes.beta;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;

// Thin shell around NativeActivity: the game itself is libCubyzig.so, whose
// android_main (via native_app_glue) owns the window and the GLES surface.
//
// Implements Runnable rather than posting an anonymous one: build tooling here
// is javac + d8, and d8 8.2.2 crashes on this class's anonymous inner classes.
public class MainActivity extends NativeActivity implements Runnable {
	private volatile boolean keyboardVisible;
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		// Must run before super.onCreate(): that call spawns the native thread,
		// and the engine opens "assets/cubyz/..." from the start. getFilesDir()
		// is the same directory the C layer uses as HOME and cwd.
		AssetExtractor.ensure(this, getFilesDir());

		super.onCreate(savedInstanceState);
		// Keep the screen on and go edge-to-edge; the renderer clears full-bleed.
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		// The default theme draws an opaque window background, which shows as a
		// black band on the side the letterboxed cutout sits on before the first
		// frame. Make it transparent so only the surface is ever visible.
		getWindow().setBackgroundDrawable(null);
		if (Build.VERSION.SDK_INT >= 28) {
			// Without this the system letterboxes the display cutout (the notch
			// edge), and in landscape that is a black stripe down one side.
			WindowManager.LayoutParams attrs = getWindow().getAttributes();
			attrs.layoutInDisplayCutoutMode =
				WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
			getWindow().setAttributes(attrs);
		}
		if (Build.VERSION.SDK_INT >= 30) {
			// API 30 replaced the systemUiVisibility flags with this; both are
			// set so every supported version goes edge-to-edge.
			getWindow().setDecorFitsSystemWindows(false);
		}
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus) {
		super.onWindowFocusChanged(hasFocus);
		if (!hasFocus) return;
		final View decor = getWindow().getDecorView();
		decor.setSystemUiVisibility(
			View.SYSTEM_UI_FLAG_LAYOUT_STABLE
			| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
			| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
			| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
			| View.SYSTEM_UI_FLAG_FULLSCREEN
			| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
	}

	// Called from the native side (cubyz_android_set_soft_keyboard) whenever a
	// text field gains or loses focus. A NativeActivity has no EditText for the
	// system to key the IME off, so the keyboard has to be driven by hand; the
	// IMM calls belong on the UI thread, hence the post.
	public void setSoftKeyboard(boolean show) {
		keyboardVisible = show;
		runOnUiThread(this);
	}

	@Override
	public void run() {
		final View decor = getWindow().getDecorView();
		final InputMethodManager imm =
			(InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
		if (imm == null) return;
		if (keyboardVisible) {
			decor.requestFocus();
			imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, 0);
		} else {
			imm.hideSoftInputFromWindow(decor.getWindowToken(), 0);
		}
	}
}
