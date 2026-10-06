package com.mineimator.app

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/** Full-screen GLES 3 surface. The context is owned here; drawing is native. */
class MineImatorView(context: Context) : GLSurfaceView(context) {
    init {
        setEGLContextClientVersion(3)
        // Keep shaders and textures when the activity is paused. Dropping the
        // context makes the next frame draw into a dead EGL context, then the process restarts.
        setPreserveEGLContextOnPause(true)
        setRenderer(FrameRenderer())
        renderMode = RENDERMODE_CONTINUOUSLY
        isFocusable = true
        isFocusableInTouchMode = false
    }

    override fun onCheckIsTextEditor(): Boolean = true

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        outAttrs.inputType = EditorInfo.TYPE_CLASS_TEXT
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_ACTION_DONE
        return MiInput(this)
    }

    private class MiInput(view: View) : BaseInputConnection(view, true) {
        private var composing = ""

        override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
            composing = ""
            if (!text.isNullOrEmpty()) {
                NativeHost.nativeCommitText(text.toString())
            }
            return true
        }

        override fun setComposingText(text: CharSequence?, newCursorPosition: Int): Boolean {
            composing = text?.toString().orEmpty()
            return true
        }

        override fun finishComposingText(): Boolean {
            if (composing.isNotEmpty()) {
                NativeHost.nativeCommitText(composing)
                composing = ""
            }
            return true
        }

        override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
            if (composing.isNotEmpty()) {
                val drop = beforeLength.coerceAtMost(composing.length)
                composing = composing.dropLast(drop)
                return true
            }
            repeat(beforeLength) { NativeHost.nativeKey(8) }
            return true
        }

        override fun sendKeyEvent(event: KeyEvent?): Boolean {
            if (event?.action != KeyEvent.ACTION_DOWN) {
                return true
            }
            when (event.keyCode) {
                KeyEvent.KEYCODE_DEL -> {
                    if (composing.isNotEmpty()) {
                        composing = composing.dropLast(1)
                    } else {
                        NativeHost.nativeKey(8)
                    }
                }
                KeyEvent.KEYCODE_ENTER -> {
                    finishComposingText()
                    NativeHost.nativeKey(13)
                }
            }
            return true
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val masked = event.actionMasked
        val nativeAction = when (masked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> NativeHost.TOUCH_DOWN
            MotionEvent.ACTION_MOVE -> NativeHost.TOUCH_MOVE
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> NativeHost.TOUCH_UP
            MotionEvent.ACTION_CANCEL -> NativeHost.TOUCH_CANCEL
            else -> return false
        }
        if (masked == MotionEvent.ACTION_MOVE) {
            for (index in 0 until event.pointerCount) {
                NativeHost.nativeTouch(
                    NativeHost.TOUCH_MOVE,
                    event.getPointerId(index),
                    event.getX(index),
                    event.getY(index),
                )
            }
        } else {
            val index = event.actionIndex
            NativeHost.nativeTouch(
                nativeAction,
                event.getPointerId(index),
                event.getX(index),
                event.getY(index),
            )
        }
        return true
    }

    private inner class FrameRenderer : Renderer {
        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            NativeHost.nativeSurfaceCreated()
        }

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            NativeHost.nativeResize(width, height, resources.displayMetrics.density)
        }

        override fun onDrawFrame(gl: GL10?) {
            NativeHost.nativeFrame()
        }
    }
}
