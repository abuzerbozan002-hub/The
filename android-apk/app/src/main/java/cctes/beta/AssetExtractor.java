package cctes.beta;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipFile;
import java.util.zip.ZipEntry;
import java.util.Enumeration;
import java.util.List;
import java.util.ArrayList;

/**
 * Extracts the APK's bundled assets onto the filesystem.
 *
 * The engine reads every asset through relative paths ("assets/cubyz/...") using
 * ordinary file IO, but assets packaged in an APK are only reachable through
 * AssetManager. The simplest fix is to unpack them once, on first launch, into a
 * directory the game can see.
 *
 * APK entries under assets/ are stored uncompressed and aligned by aapt2, so the
 * ZipFile path below can stream them straight out without an intermediate copy.
 */
final class AssetExtractor {
	private static final String TAG = "CctesAssets";
	private static final String ASSET_ROOT = "assets";
	private static final String STAMP = ".extraction-version";
	// Bump when the packaged assets change so an installed copy re-extracts.
	private static final int VERSION = 2;

	private AssetExtractor() {}

	/**
	 * Extract assets into {@code destRoot}, which must be the game's working
	 * directory: the engine opens them as "assets/cubyz/...", so the archive's
	 * own "assets/" prefix is preserved rather than stripped.
	 */
	static void ensure(Context ctx, File destRoot) {
		File stamp = new File(destRoot, STAMP);
		if (stamp.isFile() && readFile(stamp).trim().equals(Integer.toString(VERSION))) {
			return;
		}

		Log.i(TAG, "extracting assets into " + destRoot + " (version " + VERSION + ")");

		deleteRecursively(new File(destRoot, ASSET_ROOT));
		if (!destRoot.isDirectory() && !destRoot.mkdirs()) {
			Log.e(TAG, "could not create " + destRoot);
			return;
		}

		boolean ok = extractViaZip(ctx, destRoot);
		if (!ok) {
			// Fallback for setups where the APK is not a plain file on disk
			// (e.g. split APKs delivered by an app bundle).
			Log.w(TAG, "ZipFile extraction failed; falling back to AssetManager");
			deleteRecursively(new File(destRoot, ASSET_ROOT));
			extractViaManager(ctx.getAssets(), ASSET_ROOT, new File(destRoot, ASSET_ROOT));
		}

		writeFile(stamp, Integer.toString(VERSION));
		Log.i(TAG, "asset extraction done");
	}

	/** Stream entries out of the APK zip. Returns false if the zip is unusable. */
	private static boolean extractViaZip(Context ctx, File destRoot) {
		String apkPath = ctx.getApplicationInfo().sourceDir;
		try (ZipFile zip = new ZipFile(apkPath)) {
			Enumeration<? extends ZipEntry> entries = zip.entries();
			byte[] buf = new byte[64 * 1024];
			int count = 0;
			while (entries.hasMoreElements()) {
				ZipEntry e = entries.nextElement();
				String name = e.getName();
				if (e.isDirectory() || !name.startsWith(ASSET_ROOT + "/")) continue;

				File out = new File(destRoot, name);
				File parent = out.getParentFile();
				if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
					Log.e(TAG, "mkdirs failed for " + parent);
					return false;
				}
				try (InputStream in = zip.getInputStream(e);
				     OutputStream os = new FileOutputStream(out)) {
					int n;
					while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
				}
				count++;
			}
			Log.i(TAG, "extracted " + count + " files via ZipFile");
			return count > 0;
		} catch (IOException ex) {
			Log.w(TAG, "ZipFile path failed: " + ex);
			return false;
		}
	}

	/** Depth-first copy through AssetManager. */
	private static void extractViaManager(AssetManager am, String assetPath, File dest) {
		String[] children = null;
		try {
			children = am.list(assetPath);
		} catch (IOException ex) {
			Log.w(TAG, "list(" + assetPath + ") failed: " + ex);
		}
		if (children == null || children.length == 0) {
			copyFile(am, assetPath, dest);
			return;
		}
		if (!dest.isDirectory() && !dest.mkdirs()) {
			Log.e(TAG, "mkdirs failed for " + dest);
			return;
		}
		for (String child : children) {
			extractViaManager(am, assetPath + "/" + child, new File(dest, child));
		}
	}

	private static void copyFile(AssetManager am, String assetPath, File out) {
		try (InputStream in = am.open(assetPath);
		     OutputStream os = new FileOutputStream(out)) {
			byte[] buf = new byte[64 * 1024];
			int n;
			while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
		} catch (IOException ex) {
			Log.w(TAG, "copy " + assetPath + " failed: " + ex);
		}
	}

	private static void deleteRecursively(File f) {
		if (f == null || !f.exists()) return;
		if (f.isDirectory()) {
			File[] kids = f.listFiles();
			if (kids != null) for (File k : kids) deleteRecursively(k);
		}
		if (!f.delete()) Log.w(TAG, "could not delete " + f);
	}

	private static String readFile(File f) {
		try (InputStream in = new java.io.FileInputStream(f)) {
			byte[] b = new byte[(int) f.length()];
			int off = 0, n;
			while (off < b.length && (n = in.read(b, off, b.length - off)) > 0) off += n;
			return new String(b, 0, off, java.nio.charset.StandardCharsets.UTF_8);
		} catch (IOException ex) {
			return "";
		}
	}

	private static void writeFile(File f, String s) {
		try (OutputStream os = new FileOutputStream(f)) {
			os.write(s.getBytes(java.nio.charset.StandardCharsets.UTF_8));
		} catch (IOException ex) {
			Log.w(TAG, "write " + f + " failed: " + ex);
		}
	}
}
