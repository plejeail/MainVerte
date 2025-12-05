package com.plej.mainverte.utilities

import android.content.Context
import android.net.Uri
import android.os.Environment
import androidx.core.net.toUri
import java.io.File

fun photoExists(context: Context, uriString: String?): Boolean {
    if (uriString.isNullOrBlank()) return false
    val uri = uriString.toUri()

    return try {
        context.contentResolver.openInputStream(uri)?.use { true } ?: false
    } catch (_: Exception) {
        false
    }
}


fun deleteSpecimenPhoto(context: Context, uriString: String?) {
    if (uriString.isNullOrBlank()) return

    val uri = uriString.toUri()

    val picturesDir = context.getExternalFilesDir(Environment.DIRECTORY_PICTURES) ?: return
    val specimensDir = File(picturesDir, "specimens")

    val last = uri.lastPathSegment ?: return
    val decoded = Uri.decode(last)
    val fileName = decoded.substringAfterLast('/')

    val file = File(specimensDir, fileName)

    if (file.exists()) {
        file.delete()
    }
}