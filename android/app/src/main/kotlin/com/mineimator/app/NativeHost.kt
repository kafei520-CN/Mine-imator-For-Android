package com.mineimator.app

import android.app.Activity
import android.content.res.AssetManager

/**
 * JNI entry for libmineimator.so.
 *
 * Touch events arrive on the UI thread. The GLSurfaceView thread calls
 * [nativeFrame]. The native side queues touches and applies them on that
 * thread.
 */
object NativeHost {
    const val TOUCH_DOWN = 0
    const val TOUCH_MOVE = 1
    const val TOUCH_UP = 2
    const val TOUCH_CANCEL = 3

    init {
        System.loadLibrary("mineimator")
    }

    @JvmStatic
    external fun nativeStart(
        activity: Activity,
        assets: AssetManager,
        filesDir: String,
        cacheDir: String,
    )

    external fun nativePickResult(path: String)

    @JvmStatic
    external fun nativeCommitText(text: String)

    @JvmStatic
    external fun nativeKey(code: Int)

    @JvmStatic
    external fun nativeSurfaceCreated()

    @JvmStatic
    external fun nativeResize(width: Int, height: Int, density: Float)

    @JvmStatic
    external fun nativeFrame()

    @JvmStatic
    external fun nativeTouch(action: Int, pointerId: Int, x: Float, y: Float)

    @JvmStatic
    external fun nativeStop()
}
