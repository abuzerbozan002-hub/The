package cctes.beta;

import android.app.NativeActivity;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;

// Thin shell around NativeActivity: the game itself is libCubyzig.so, whose
// android_main (via native_app_glue) owns the window and the GLES surface.
public class MainActivity extends NativeActivity {
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		// Must run before super.onCreate(): that call spawns the native thread,
		// and the engine opens "assets/cubyz/..." from the start. getFilesDir()
		// is the same directory the C layer uses as HOME and cwd.
		AssetExtractor.ensure(this, getFilesDir());

		super.onCreate(savedInstanceState);
		// Keep the screen on and go edge-to-edge; the renderer clears full-bleed.
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
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
}
