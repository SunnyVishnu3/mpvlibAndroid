package `is`.xyz.mpv

import android.content.Context
import android.graphics.Bitmap
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.min

object FastThumbnails {
    private val initialized = AtomicBoolean(false)
    private const val MAX_PARALLEL_THUMBNAILS = 4
    
    /**
     * Initialize the fast thumbnail system.
     * Call this once before generating thumbnails (typically in Application.onCreate).
     * 
     * @param context Application context
     */
    @JvmStatic
    fun initialize(context: Context) {
        if (initialized.compareAndSet(false, true)) {
            MPVLib.setThumbnailJavaVM(context.applicationContext)
        }
    }
    
    /**
     * Check if initialized.
     */
    @JvmStatic
    fun isInitialized(): Boolean = initialized.get()
    
    /**
     * Clear internal codec cache and hardware context.
     * Call this when you want to free up memory (e.g., onLowMemory callback).
     * The cache will be rebuilt automatically on next thumbnail generation.
     * 
     * Note: Clearing the cache will make the next thumbnail generation slightly slower
     * as codecs need to be re-initialized, but subsequent calls will be fast again.
     */
    @JvmStatic
    fun clearCache() {
        if (initialized.get()) {
            MPVLib.clearThumbnailCache()
        }
    }
    
    /**
     * Generate thumbnail using fast FFmpeg direct API
     * 
     * @param path File path or URL to the video
     * @param position Time position in seconds (default: 0.0)
     * @param dimension Max dimension for longest side (width or height) in pixels (default: 512)
     * @param useHwDec Whether to use hardware acceleration if available (default: true)
     * @return Bitmap thumbnail, or null if generation fails
     * @throws IllegalStateException if not initialized
     */
    @JvmStatic
    @JvmOverloads
    fun generate(
        path: String,
        position: Double = 0.0,
        dimension: Int = 512,
        useHwDec: Boolean = true
    ): Bitmap? {
        check(initialized.get()) {
            "FastThumbnails not initialized. Call initialize(context) first."
        }
        
        require(dimension in 1..4096) {
            "Dimension must be between 1 and 4096 (got $dimension)"
        }

        require(position.isFinite() && position >= 0.0) {
            "Position must be finite and non-negative (got $position)"
        }

        require(path.isNotEmpty()) {
            "Path must not be empty"
        }
        
        return try {
            MPVLib.grabThumbnailFast(path, position, dimension, useHwDec)
        } catch (e: Exception) {
            e.printStackTrace()
            null
        }
    }
    
    /**
     * Generate thumbnail asynchronously (IO dispatcher).
     * 
     * @param path File path or URL
     * @param position Time position in seconds (default: 0.0)
     * @param dimension Max dimension for longest side (width or height) in pixels (default: 512)
     * @param useHwDec Whether to use hardware acceleration if available (default: true)
     * @return Bitmap thumbnail, or null
     */
    suspend fun generateAsync(
        path: String,
        position: Double = 0.0,
        dimension: Int = 512,
        useHwDec: Boolean = true
    ): Bitmap? = withContext(Dispatchers.IO) {
        generate(path, position, dimension, useHwDec)
    }
    
    /**
     * Generate multiple thumbnails at different positions.
     * 
     * @param path File path
     * @param positions List of time positions
     * @param dimension Max dimension for longest side (width or height) in pixels (default: 512)
     * @param useHwDec Whether to use hardware acceleration if available (default: true)
     * @return List of bitmaps (may contain nulls)
     */
    @JvmStatic
    @JvmOverloads
    fun generateMultiple(
        path: String,
        positions: List<Double>,
        dimension: Int = 512,
        useHwDec: Boolean = true
    ): List<Bitmap?> {
        return positions.map { position ->
            generate(path, position, dimension, useHwDec)
        }
    }
    
    /**
     * Generate multiple thumbnails asynchronously.
     * 
     * @param path File path
     * @param positions List of positions
     * @param dimension Max dimension for longest side (width or height) in pixels (default: 512)
     * @param useHwDec Whether to use hardware acceleration if available (default: true)
     * @return List of bitmaps
     */
    suspend fun generateMultipleAsync(
        path: String,
        positions: List<Double>,
        dimension: Int = 512,
        useHwDec: Boolean = true
    ): List<Bitmap?> = coroutineScope {
        val parallelism = min(MAX_PARALLEL_THUMBNAILS, positions.size.coerceAtLeast(1))
        positions.chunked(parallelism).flatMap { chunk ->
            chunk.map { position ->
                async(Dispatchers.IO) {
                    generate(path, position, dimension, useHwDec)
                }
            }.awaitAll()
        }
    }
    
    /**
     * Performance benchmark helper.
     * Generates a thumbnail and measures time taken.
     * 
     * @param path File path
     * @param position Time position in seconds (default: 0.0)
     * @param dimension Max dimension for longest side (width or height) in pixels (default: 512)
     * @param useHwDec Whether to use hardware acceleration if available (default: true)
     * @return Pair of (bitmap, time in milliseconds)
     */
    @JvmStatic
    @JvmOverloads
    fun benchmark(
        path: String,
        position: Double = 0.0,
        dimension: Int = 512,
        useHwDec: Boolean = true
    ): Pair<Bitmap?, Long> {
        val start = System.currentTimeMillis()
        val bitmap = generate(path, position, dimension, useHwDec)
        val elapsed = System.currentTimeMillis() - start
        return Pair(bitmap, elapsed)
    }
}
