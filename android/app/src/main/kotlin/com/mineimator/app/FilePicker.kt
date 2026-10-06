package com.mineimator.app

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import kotlin.concurrent.thread

/** System document picker. The chosen file is copied into the app imports directory. */
object FilePicker {
    const val REQUEST_OPEN = 41

    @Volatile
    var picking: Boolean = false

    @JvmStatic
    fun open(activity: Activity) {
        picking = true
        activity.runOnUiThread {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            try {
                activity.startActivityForResult(intent, REQUEST_OPEN)
            } catch (error: RuntimeException) {
                picking = false
                NativeHost.nativePickResult("")
            }
        }
    }

    fun onResult(activity: Activity, resultCode: Int, data: Intent?) {
        picking = false
        val uri = data?.data
        if (resultCode != Activity.RESULT_OK || uri == null) {
            NativeHost.nativePickResult("")
            return
        }
        thread(name = "mineimator-import") {
            val copied = try {
                copyIntoImports(activity, uri)
            } catch (error: Exception) {
                ""
            }
            NativeHost.nativePickResult(copied)
        }
    }

    private fun copyIntoImports(activity: Activity, uri: Uri): String {
        val name = displayName(activity, uri).ifBlank { "import.bin" }
        val dest = File(activity.filesDir, "imports/$name")
        dest.parentFile?.mkdirs()
        activity.contentResolver.openInputStream(uri).use { input ->
            if (input == null) {
                return ""
            }
            dest.outputStream().use { output -> input.copyTo(output) }
        }
        return dest.absolutePath
    }

    private fun displayName(activity: Activity, uri: Uri): String {
        activity.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
            ?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    if (index >= 0) {
                        return cursor.getString(index) ?: ""
                    }
                }
            }
        return ""
    }
}
