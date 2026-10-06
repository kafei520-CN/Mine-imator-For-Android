package com.mineimator.app

import android.app.Activity
import android.view.View
import android.view.inputmethod.InputMethodManager

/** Shows the soft keyboard on the GL view while a Mine-imator text field is focused. */
object Keyboard {
    private var target: View? = null

    fun attach(view: View) {
        target = view
    }

    @JvmStatic
    fun setVisible(activity: Activity, show: Boolean) {
        activity.runOnUiThread {
            val view = target ?: return@runOnUiThread
            val imm = activity.getSystemService(InputMethodManager::class.java) ?: return@runOnUiThread
            if (show) {
                view.isFocusableInTouchMode = true
                view.requestFocus()
                imm.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT)
            } else {
                imm.hideSoftInputFromWindow(view.windowToken, 0)
                view.clearFocus()
                view.isFocusableInTouchMode = false
            }
        }
    }
}
