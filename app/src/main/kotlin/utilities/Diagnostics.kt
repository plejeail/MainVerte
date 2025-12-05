//! utilities/Diagnostics.kt --------------------------------------------------
//!
//! ----------------------------------------------------------------- MainVerte
package com.plej.mainverte.utilities

import android.os.SystemClock
import android.util.Log
import com.plej.mainverte.BuildConfig

object Logger {
    const val TAG = "MainVerteLog"

    inline fun d(crossinline message: () -> String) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, message())
        }
    }

    inline fun i(crossinline message: () -> String) {
        if (Log.isLoggable(TAG, Log.INFO)) {
            Log.i(TAG, message())
        }
    }

    inline fun w(crossinline message: () -> String) {
        if (Log.isLoggable(TAG, Log.WARN)) {
            Log.w(TAG, message())
        }
    }

    inline fun e(crossinline message: () -> String) {
        if (Log.isLoggable(TAG, Log.ERROR)) {
            Log.e(TAG, message())
        }
    }
}

inline fun debugOnly(crossinline block: () -> Unit) {
    if (BuildConfig.DEBUG) {
        block()
    }
}

class MainVerteException() : Exception()

inline fun expect(crossinline condition: () -> Boolean) {
    debugOnly {
        if (!condition()) {
            throw AssertionError()
        }
    }
}

fun expectIsInRange(value: Int, min: Int, max: Int) {
    debugOnly {
        if (value < min || value > max) {
            throw AssertionError()
        }
    }
}

fun expectDead() {
    debugOnly {
        throw AssertionError()
    }
}

inline fun measureTime(name: String, block: () -> Unit) {
    val start = SystemClock.elapsedRealtimeNanos()
    block()
    val elapsed = (SystemClock.elapsedRealtimeNanos() - start) / 1_000_000.0
    Logger.i { "%s took %.3f ms".format(name, elapsed) }
}
