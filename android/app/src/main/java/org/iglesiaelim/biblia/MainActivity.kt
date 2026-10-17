package org.iglesiaelim.biblia

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.content.res.ColorStateList
import android.database.sqlite.SQLiteDatabase
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.RippleDrawable
import android.os.Build
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.TextView
import android.widget.Toast
import android.util.Xml
import org.xmlpull.v1.XmlPullParser
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import android.text.style.StyleSpan
import java.io.File
import java.io.FileOutputStream

class MainActivity : Activity() {
    private lateinit var modules: List<BibleModule>
    private lateinit var modulePicker: Spinner
    private lateinit var bookPicker: Spinner
    private lateinit var chapterLabel: TextView
    private lateinit var passage: TextView
    private var chapter = 1
    private var books: List<BibleBook> = emptyList()
    private var currentModule: BibleModule? = null
    private var readingMode = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        try {
            seedBundledBibles()
            refreshModules()
            showReader()
        } catch (error: Exception) {
            showFatal(error.message ?: "No se pudo abrir la Biblia")
        }
    }

    @Suppress("DEPRECATION")
    private fun showReader() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(8), dp(18), dp(8))
            setBackgroundColor(CANVAS)
            setOnApplyWindowInsetsListener { view, insets ->
                val top: Int
                val bottom: Int
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    val bars = insets.getInsets(WindowInsets.Type.systemBars())
                    top = bars.top
                    bottom = bars.bottom
                } else {
                    top = insets.systemWindowInsetTop
                    bottom = insets.systemWindowInsetBottom
                }
                view.setPadding(dp(18), dp(8) + top, dp(18), dp(8) + bottom)
                insets
            }
        }
        val title = TextView(this).apply {
            text = "Biblia Elim"
            textSize = 25f
            typeface = Typeface.DEFAULT_BOLD
            setTextColor(INK)
        }
        root.addView(title)
        root.addView(View(this), LinearLayout.LayoutParams(1, dp(8)))

        val moduleRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(10), dp(4), dp(8), dp(4))
            background = roundedBackground(SURFACE, OUTLINE, 16f)
        }
        modulePicker = Spinner(this).apply {
            setPadding(dp(4), 0, dp(4), 0)
        }
        moduleRow.addView(modulePicker, LinearLayout.LayoutParams(0, dp(46), 1f))
        moduleRow.addView(Button(this).apply {
            text = "Importar"
            isAllCaps = false
            textSize = 14f
            minHeight = dp(40)
            minWidth = dp(82)
            setPadding(dp(10), 0, dp(10), 0)
            background = roundedRipple(PALE_GREEN, 0x22385A48, 12f)
            setTextColor(FOREST)
            stateListAnimator = null
            setOnClickListener { chooseModule() }
        })
        root.addView(moduleRow, spacedParams(bottom = 10))

        val navigation = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(8), dp(4), dp(8), dp(4))
            background = roundedBackground(SURFACE, OUTLINE, 16f)
        }
        bookPicker = Spinner(this).apply { setPadding(dp(2), 0, dp(2), 0) }
        navigation.addView(bookPicker, LinearLayout.LayoutParams(0, dp(46), 1f))
        navigation.addView(Button(this).apply {
            text = "‹"
            isAllCaps = false
            textSize = 20f
            minWidth = dp(40)
            minHeight = dp(40)
            setPadding(0, 0, 0, 0)
            background = roundedRipple(PALE_GREEN, 0x22385A48, 11f)
            setTextColor(FOREST)
            stateListAnimator = null
            contentDescription = "Capítulo anterior"
            setOnClickListener { changeChapter(-1) }
        })
        chapterLabel = TextView(this).apply {
            textSize = 16f
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            setTextColor(Color.WHITE)
            minWidth = dp(40)
            minHeight = dp(40)
            background = roundedBackground(FOREST, FOREST, 12f)
        }
        navigation.addView(chapterLabel)
        navigation.addView(Button(this).apply {
            text = "›"
            isAllCaps = false
            textSize = 20f
            minWidth = dp(40)
            minHeight = dp(40)
            setPadding(0, 0, 0, 0)
            background = roundedRipple(PALE_GREEN, 0x22385A48, 11f)
            setTextColor(FOREST)
            stateListAnimator = null
            contentDescription = "Capítulo siguiente"
            setOnClickListener { changeChapter(1) }
        })
        root.addView(navigation, spacedParams(bottom = 10))
        val readingButton = Button(this).apply {
            text = if (readingMode) "Salir del modo lectura · Aa" else "Modo lectura · Aa"
            isAllCaps = false
            setTextColor(FOREST)
            background = roundedRipple(PALE_GREEN, 0x22385A48, 12f)
            setOnClickListener {
                readingMode = !readingMode
                passage.textSize = if (readingMode) 24f else 19f
                moduleRow.visibility = if (readingMode) View.GONE else View.VISIBLE
                navigation.visibility = if (readingMode) View.GONE else View.VISIBLE
                root.getChildAt(5).visibility = if (readingMode) View.GONE else View.VISIBLE
                root.getChildAt(root.childCount - 1).visibility = if (readingMode) View.GONE else View.VISIBLE
                text = if (readingMode) "Salir del modo lectura · Aa" else "Modo lectura · Aa"
            }
        }
        root.addView(readingButton, spacedParams(bottom = 8))
        val searchButton = Button(this).apply {
            text = "Buscar en esta Biblia"
            isAllCaps = false
            textSize = 14f
            minHeight = dp(46)
            background = roundedRipple(FOREST, 0x33FFFFFF, 12f)
            setTextColor(Color.WHITE)
            stateListAnimator = null
            setOnClickListener { search() }
        }
        root.addView(searchButton, spacedParams(bottom = 8))
        moduleRow.visibility = if (readingMode) View.GONE else View.VISIBLE
        navigation.visibility = if (readingMode) View.GONE else View.VISIBLE
        searchButton.visibility = if (readingMode) View.GONE else View.VISIBLE

        val scroll = ScrollView(this)
        passage = TextView(this).apply {
            textSize = if (readingMode) 24f else if (resources.configuration.smallestScreenWidthDp >= 600) 20f else 19f
            typeface = Typeface.SERIF
            letterSpacing = 0.002f
            setTextColor(INK)
            setLineSpacing(dp(4).toFloat(), 1.03f)
            setPadding(dp(4), dp(8), dp(4), dp(24))
            setTextIsSelectable(true)
        }
        scroll.addView(passage)
        root.addView(scroll, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f
        ))
        root.addView(bottomNavigation())
        setContentView(root)
        configureSystemBars()

        modulePicker.onItemSelectedListener = SelectionListener { position ->
            currentModule = modules.getOrNull(position)
            chapter = 1
            loadBooks()
        }
        bookPicker.onItemSelectedListener = SelectionListener { position ->
            chapter = 1
            renderChapter()
        }
        updateModulePicker()
    }

    private fun bottomNavigation(): View {
        val bar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            setPadding(0, dp(4), 0, dp(2))
            background = roundedBackground(SURFACE, OUTLINE, 18f)
        }
        listOf("Biblia", "Hoy", "Nube", "Diccionario").forEach { label ->
            bar.addView(Button(this).apply {
                text = label
                isAllCaps = false
                textSize = 12f
                minWidth = 0
                setPadding(dp(5), 0, dp(5), 0)
                setTextColor(if (label == "Biblia") FOREST else MUTED)
                background = roundedRipple(SURFACE, 0x22385A48, 12f)
                setOnClickListener {
                    when (label) {
                        "Biblia" -> showReader()
                        "Hoy" -> showToday()
                        "Nube" -> showWordCloud()
                        else -> showDictionary()
                    }
                }
            }, LinearLayout.LayoutParams(0, dp(46), 1f))
        }
        return bar
    }

    private fun page(titleText: String): Pair<LinearLayout, LinearLayout> {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(18), dp(18), dp(8))
            setBackgroundColor(CANVAS)
        }
        val heading = TextView(this).apply {
            text = titleText
            textSize = 25f
            typeface = Typeface.DEFAULT_BOLD
            setTextColor(INK)
            setPadding(0, 0, 0, dp(12))
        }
        root.addView(heading)
        val content = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val scroll = ScrollView(this).apply { addView(content) }
        root.addView(scroll, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        root.addView(bottomNavigation())
        setContentView(root)
        configureSystemBars()
        return root to content
    }

    private fun showToday() {
        val (_, content) = page("Lectura de hoy")
        val module = currentModule ?: return
        val prefs = getSharedPreferences("reading", MODE_PRIVATE)
        val todayEpoch = java.util.concurrent.TimeUnit.MILLISECONDS.toDays(System.currentTimeMillis())
        val startEpoch = prefs.getLong("plan-start-day", todayEpoch).also {
            if (!prefs.contains("plan-start-day")) prefs.edit().putLong("plan-start-day", it).apply()
        }
        val day = (todayEpoch - startEpoch).toInt().coerceAtLeast(0) + 1
        val chapterIndex = (day - 1) % books.sumOf { module.maxChapter(it.id).coerceAtLeast(1) }
        var remaining = chapterIndex
        var book = books.first()
        var todayChapter = 1
        for (candidate in books) {
            val chapterCount = module.maxChapter(candidate.id).coerceAtLeast(1)
            if (remaining < chapterCount) {
                book = candidate
                todayChapter = remaining + 1
                break
            }
            remaining -= chapterCount
        }
        val verses = module.chapter(book.id, todayChapter)
        val key = java.text.SimpleDateFormat("yyyy-MM-dd", java.util.Locale.US).format(java.util.Date())
        val isDone = prefs.getBoolean(key, false)
        content.addView(cardTitle("Plan de lectura · Día $day"))
        content.addView(bodyText("Lee ${book.name} ${todayChapter}. Una lectura breve para mantener el hábito diario."))
        content.addView(cardTitle("Versículo para meditar"))
        content.addView(bodyText(verses.firstOrNull()?.let { "${book.name} ${todayChapter}:${it.first}\n\n${it.second}" }
            ?: "Abre la lectura de hoy para comenzar."))
        content.addView(Button(this).apply {
            text = if (isDone) "Lectura completada ✓" else "Marcar lectura como completada"
            isAllCaps = false
            setTextColor(Color.WHITE)
            background = roundedRipple(FOREST, 0x33FFFFFF, 12f)
            setOnClickListener {
                prefs.edit().putBoolean(key, !prefs.getBoolean(key, false)).apply()
                showToday()
            }
        }, spacedParams(12))
        content.addView(Button(this).apply {
            text = "Leer ${book.name} ${todayChapter}"
            isAllCaps = false
            setOnClickListener {
                bookPicker.setSelection(books.indexOf(book)); chapter = todayChapter; showReader()
            }
        })
    }

    private fun showDictionary(query: String = "") {
        val (_, content) = page("Diccionario bíblico")
        val input = EditText(this).apply { hint = "Busca un término"; setSingleLine(); setText(query) }
        content.addView(input, spacedParams(10))
        val results = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        fun filter() {
            results.removeAllViews()
            val q = normalize(input.text.toString())
            dictionaryEntries().filter { q.isBlank() || normalize(it.title + " " + it.keys).contains(q) }
                .take(50).forEach { entry ->
                    val button = Button(this).apply {
                        text = entry.title
                        isAllCaps = false
                        gravity = Gravity.START or Gravity.CENTER_VERTICAL
                        setTextColor(FOREST)
                        background = roundedRipple(SURFACE, 0x22385A48, 12f)
                        setOnClickListener {
                            val details = buildString {
                                append(entry.definition)
                                if (entry.refs.isNotBlank()) append("\n\nReferencias\n${entry.refs}")
                                if (entry.study.isNotBlank()) append("\n\n${entry.study}")
                            }
                            AlertDialog.Builder(this@MainActivity).setTitle(entry.title).setMessage(details)
                                .setPositiveButton("Cerrar", null).show()
                        }
                    }
                    results.addView(button, spacedParams(5))
                }
        }
        content.addView(Button(this).apply { text = "Buscar"; isAllCaps = false; setOnClickListener { filter() } })
        content.addView(results)
        filter()
    }

    private fun showWordCloud() {
        val (_, content) = page("Nube de palabras")
        val module = currentModule ?: return
        val picker = Spinner(this)
        picker.adapter = ArrayAdapter(this, android.R.layout.simple_spinner_dropdown_item, books.map { it.name })
        content.addView(picker, spacedParams(8))
        val cloud = FlowLayout(this)
        content.addView(cloud)
        fun update() {
            cloud.removeAllViews()
            val book = books.getOrNull(picker.selectedItemPosition) ?: return
            Thread {
                val words = module.wordCounts(book.id)
                runOnUiThread {
                    if (isFinishing) return@runOnUiThread
                    val max = words.values.maxOrNull()?.coerceAtLeast(1) ?: 1
                    words.entries.take(70).forEach { (word, n) ->
                        val size = (15 + 13f * n / max).coerceIn(15f, 28f)
                        cloud.addView(TextView(this).apply {
                            text = word
                            textSize = size
                            typeface = if (size > 21) Typeface.DEFAULT_BOLD else Typeface.DEFAULT
                            setTextColor(if (size > 21) FOREST else INK)
                            setPadding(dp(7), dp(4), dp(7), dp(4))
                            setOnClickListener { searchForWord(word) }
                        })
                    }
                }
            }.start()
        }
        picker.onItemSelectedListener = SelectionListener { update() }
        content.addView(bodyText("Palabras más frecuentes; toca una para buscarla en la Biblia."))
    }

    private fun searchForWord(word: String) {
        showReader()
        showSearchResults(word)
    }

    private fun bodyText(text: String) = TextView(this).apply {
        this.text = text
        textSize = 18f
        setTextColor(INK)
        typeface = Typeface.SERIF
        setLineSpacing(dp(4).toFloat(), 1.05f)
        setPadding(dp(4), dp(8), dp(4), dp(16))
    }

    private fun cardTitle(text: String) = TextView(this).apply {
        this.text = text
        textSize = 18f
        typeface = Typeface.DEFAULT_BOLD
        setTextColor(FOREST)
        setPadding(dp(14), dp(12), dp(14), dp(4))
        background = roundedBackground(SURFACE, OUTLINE, 14f)
    }

    private fun dictionaryEntries(): List<DictionaryEntry> {
        val parser = Xml.newPullParser()
        assets.open("diccionario-elim.xml").use { parser.setInput(it, "UTF-8"); return parseDictionary(parser) }
    }

    private fun parseDictionary(parser: XmlPullParser): List<DictionaryEntry> {
        val entries = mutableListOf<DictionaryEntry>()
        var event = parser.eventType
        var title = ""; var keys = ""; var definition = ""; var refs = ""; var study = ""; var field = ""
        while (event != XmlPullParser.END_DOCUMENT) {
            when (event) {
                XmlPullParser.START_TAG -> {
                    field = parser.name
                    if (field == "entrada") { keys = parser.getAttributeValue(null, "claves") ?: ""; title = ""; definition = ""; refs = ""; study = "" }
                    if (field == "estudio") field = "estudio"
                }
                XmlPullParser.TEXT -> when (field) {
                    "titulo" -> title += parser.text
                    "definicion" -> definition += parser.text
                    "referencias" -> refs += parser.text
                    "estudio" -> study += parser.text
                }
                XmlPullParser.END_TAG -> {
                    if (parser.name == "entrada") entries.add(DictionaryEntry(title, keys, definition, refs, study))
                    field = ""
                }
            }
            event = parser.next()
        }
        return entries
    }

    private fun normalize(text: String) = java.text.Normalizer.normalize(text.lowercase(), java.text.Normalizer.Form.NFD)
        .replace(Regex("\\p{M}+"), "").trim()

    private fun seedBundledBibles() {
        val directory = File(filesDir, "modules").apply { mkdirs() }
        val bundledModules = assets.list("modules").orEmpty().filter { it.endsWith(".sqlite") }
        check(bundledModules.isNotEmpty()) { "El APK no contiene módulos SQLite" }
        val bundledHashes = assets.open("modules/manifest.sha256").bufferedReader().useLines { lines ->
            lines.mapNotNull { line ->
                val parts = line.trim().split(Regex("\\s+"), limit = 2)
                if (parts.size == 2) parts[1] to parts[0] else null
            }.toMap()
        }
        for (assetName in bundledModules) {
            val expectedHash = bundledHashes[assetName]
                ?: error("Falta el hash de verificación de $assetName")
            val safeName = assetName.removeSuffix(".sqlite")
            val target = File(directory, "$safeName.sqlite")
            if (target.isFile && sha256(target) == expectedHash) continue
            val staging = File.createTempFile("seed-", ".sqlite", directory)
            try {
                assets.open("modules/$assetName").use { input ->
                    FileOutputStream(staging).use { output -> input.copyTo(output) }
                }
                check(sha256(staging) == expectedHash) { "La copia de $assetName no coincide con el APK" }
                val validation = BibleModule.open(staging)
                val id = try { validation.id } finally { validation.close() }
                require(id.isNotBlank() && !id.contains('/') && !id.contains('\\')) {
                    "ID de módulo incluido no válido"
                }
                val safeId = id.replace(Regex("[^A-Za-z0-9._-]"), "_")
                require("$safeId.sqlite" == assetName) { "El archivo $assetName no coincide con su ID $id" }
                android.system.Os.rename(staging.absolutePath, target.absolutePath)
            } finally {
                staging.delete()
            }
        }
    }

    private fun sha256(file: File): String = file.inputStream().use { input ->
        val digest = java.security.MessageDigest.getInstance("SHA-256")
        val buffer = ByteArray(64 * 1024)
        while (true) {
            val count = input.read(buffer)
            if (count < 0) break
            digest.update(buffer, 0, count)
        }
        digest.digest().joinToString("") { "%02x".format(it) }
    }

    private fun refreshModules() {
        val directory = File(filesDir, "modules").apply { mkdirs() }
        if (this::modules.isInitialized) modules.forEach { it.close() }
        modules = directory.listFiles { file -> file.extension.equals("sqlite", true) }
            .orEmpty().mapNotNull { file ->
                try {
                    BibleModule.open(file)
                } catch (_: Exception) {
                    null
                }
            }.sortedBy { it.name }
        check(modules.isNotEmpty()) { "No hay módulos SQLite válidos" }
    }

    private fun updateModulePicker(select: Int = 0) {
        modulePicker.adapter = ArrayAdapter(
            this, android.R.layout.simple_spinner_dropdown_item,
            modules.map { it.name }
        )
        modulePicker.setSelection(select.coerceIn(modules.indices))
        currentModule = modules[select.coerceIn(modules.indices)]
        loadBooks()
    }

    private fun loadBooks() {
        val module = currentModule ?: return
        books = module.books()
        bookPicker.adapter = ArrayAdapter(
            this, android.R.layout.simple_spinner_dropdown_item,
            books.map { it.name }
        )
        bookPicker.setSelection(0)
        renderChapter()
    }

    private fun renderChapter() {
        val module = currentModule ?: return
        val book = books.getOrNull(bookPicker.selectedItemPosition) ?: return
        val verses = module.chapter(book.id, chapter)
        if (verses.isEmpty() && chapter > 1) {
            chapter = 1
            renderChapter()
            return
        }
        chapterLabel.text = chapter.toString()
        val formatted = SpannableStringBuilder()
        verses.forEach { (number, text) ->
            val marker = "$number.  "
            val start = formatted.length
            formatted.append(marker)
            formatted.setSpan(StyleSpan(Typeface.BOLD), start, formatted.length,
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            formatted.setSpan(ForegroundColorSpan(FOREST), start, formatted.length,
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            formatted.append(text)
            formatted.append("\n\n")
        }
        passage.text = if (verses.isEmpty()) "No hay texto para este capítulo." else formatted
    }

    private fun changeChapter(delta: Int) {
        val book = books.getOrNull(bookPicker.selectedItemPosition) ?: return
        val maxChapter = currentModule?.maxChapter(book.id) ?: return
        chapter = (chapter + delta).coerceIn(1, maxChapter)
        renderChapter()
    }

    private fun search() {
        val input = EditText(this).apply {
            hint = "Palabra o frase"
            setSingleLine()
        }
        AlertDialog.Builder(this)
            .setTitle("Buscar")
            .setView(input)
            .setNegativeButton("Cancelar", null)
            .setPositiveButton("Buscar") { _, _ ->
                val query = input.text.toString().trim()
                if (query.isNotEmpty()) showSearchResults(query)
            }
            .show()
    }

    private fun showSearchResults(query: String) {
        val module = currentModule ?: return
        Toast.makeText(this, "Buscando…", Toast.LENGTH_SHORT).show()
        Thread {
            val results = try { module.search(query) } catch (_: Exception) { emptyList() }
            runOnUiThread {
                if (isFinishing) return@runOnUiThread
                if (results.isEmpty()) {
                    Toast.makeText(this, "No se encontraron resultados", Toast.LENGTH_SHORT).show()
                    return@runOnUiThread
                }
                val labels = results.map { "${it.bookName} ${it.chapter}:${it.verse}  ${it.text}" }
                AlertDialog.Builder(this).setTitle("Resultados (${results.size})")
                    .setItems(labels.toTypedArray()) { _, index ->
                        val result = results[index]
                        val bookPosition = books.indexOfFirst { it.id == result.bookId }
                        if (bookPosition >= 0) bookPicker.setSelection(bookPosition)
                        chapter = result.chapter
                        renderChapter()
                    }.setPositiveButton("Cerrar", null).show()
            }
        }
            .start()
    }

    private fun chooseModule() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/octet-stream"
        }
        startActivityForResult(intent, REQUEST_MODULE)
    }

    @Deprecated("Compatibilidad con el selector de documentos del sistema")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQUEST_MODULE || resultCode != RESULT_OK) return
        val uri: Uri = data?.data ?: return
        var staging: File? = null
        var validated: BibleModule? = null
        try {
            val directory = File(filesDir, "modules").apply { mkdirs() }
            val stagedFile = File.createTempFile("import-", ".sqlite", directory)
            staging = stagedFile
            contentResolver.openInputStream(uri).use { input ->
                requireNotNull(input) { "No se pudo leer el archivo" }
                FileOutputStream(stagedFile).use { output -> input.copyTo(output) }
            }
            val imported = BibleModule.open(stagedFile)
            validated = imported
            if (modules.any { it.id == imported.id }) {
                Toast.makeText(this, "${imported.name} ya está instalada", Toast.LENGTH_SHORT).show()
                return
            }
            val safeId = imported.id.replace(Regex("[^A-Za-z0-9._-]"), "_")
            var target = File(directory, "$safeId.sqlite")
            var suffix = 2
            while (target.exists()) target = File(directory, "$safeId-${suffix++}.sqlite")
            check(stagedFile.renameTo(target)) { "No se pudo instalar el módulo" }
            staging = null
            refreshModules()
            val selected = modules.indexOfFirst { it.id == imported.id }.coerceAtLeast(0)
            updateModulePicker(selected)
            Toast.makeText(this, "${imported.name} instalada", Toast.LENGTH_SHORT).show()
        } catch (error: Exception) {
            Toast.makeText(this, error.message ?: "Módulo no válido", Toast.LENGTH_LONG).show()
        } finally {
            validated?.close()
            staging?.delete()
        }
    }

    override fun onDestroy() {
        if (this::modules.isInitialized) modules.forEach { it.close() }
        super.onDestroy()
    }

    private fun showFatal(message: String) {
        setContentView(TextView(this).apply {
            text = "Biblia Elim\n\n$message"
            textSize = 18f
            setPadding(dp(24), dp(24), dp(24), dp(24))
            setTextColor(INK)
            setBackgroundColor(CANVAS)
        })
        configureSystemBars()
    }

    private fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()

    private fun spacedParams(bottom: Int) = LinearLayout.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
    ).apply { bottomMargin = dp(bottom) }

    private fun roundedBackground(fill: Int, stroke: Int, radiusDp: Float) =
        GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            cornerRadius = dp(radiusDp.toInt()).toFloat()
            setColor(fill)
            setStroke(dp(1), stroke)
        }

    private fun roundedRipple(fill: Int, ripple: Int, radiusDp: Float) = RippleDrawable(
        ColorStateList.valueOf(ripple), roundedBackground(fill, fill, radiusDp), null
    )

    private fun configureSystemBars() {
        val decor = window.decorView
        window.statusBarColor = CANVAS
        window.navigationBarColor = CANVAS
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            decor.post {
                val appearance = android.view.WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS or
                    android.view.WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS
                decor.windowInsetsController?.setSystemBarsAppearance(appearance, appearance)
            }
        } else {
            var appearance = View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                appearance = appearance or View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR
            }
            window.decorView.systemUiVisibility = appearance
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.isStatusBarContrastEnforced = false
            window.isNavigationBarContrastEnforced = false
        }
    }

    companion object {
        private const val REQUEST_MODULE = 42
        private const val CANVAS = 0xFFF7F3EA.toInt()
        private const val SURFACE = 0xFFFFFEFA.toInt()
        private const val OUTLINE = 0xFFE6E0D4.toInt()
        private const val INK = 0xFF302D27.toInt()
        private const val MUTED = 0xFF776F63.toInt()
        private const val FOREST = 0xFF385A48.toInt()
        private const val PALE_GREEN = 0xFFE8EFE8.toInt()
    }
}

private data class BibleBook(val id: Long, val name: String)
private data class SearchHit(val bookId: Long, val bookName: String, val chapter: Int, val verse: Int, val text: String)
private data class DictionaryEntry(val title: String, val keys: String, val definition: String, val refs: String, val study: String)

private class BibleModule private constructor(
    private val database: SQLiteDatabase,
    val id: String,
    val name: String
) {
    fun close() { if (database.isOpen) database.close() }
    fun books(): List<BibleBook> = database.rawQuery(
        "SELECT book_id,name FROM books ORDER BY position", null
    ).use { cursor ->
        buildList {
            while (cursor.moveToNext()) add(BibleBook(cursor.getLong(0), cursor.getString(1)))
        }
    }

    fun chapter(bookId: Long, chapter: Int): List<Pair<Int, String>> = database.rawQuery(
        "SELECT verse,text FROM verses WHERE book_id=? AND chapter=? ORDER BY verse",
        arrayOf(bookId.toString(), chapter.toString())
    ).use { cursor ->
        buildList {
            while (cursor.moveToNext()) add(cursor.getInt(0) to cursor.getString(1))
        }
    }

    fun maxChapter(bookId: Long): Int = database.rawQuery(
        "SELECT COALESCE(MAX(chapter),1) FROM verses WHERE book_id=?",
        arrayOf(bookId.toString())
    ).use { cursor -> if (cursor.moveToFirst()) cursor.getInt(0) else 1 }

    fun wordCounts(bookId: Long): Map<String, Int> {
        val stop = setOf("el","la","los","las","un","una","de","del","al","y","e","o","que","en","a","por","para","con","sin","sobre","como","mas","menos","muy","no","ni","si","se","su","sus","mi","mis","tu","tus","le","les","lo","me","te","nos","es","son","ser","fue","eran","era","esta","estan","estaba","ha","han","he","hay","este","esta","esto","ese","esa","yo","tu","el","ella","ellos","quien","cual","cuando","donde","porque","pero","sino","tambien","asi","bien","todo","toda","todos","todas","otro","otra","mismo","cada","pues","entonces","ahora","despues","antes","aqui","alli","oh","hecho","hacer","hace","tiene","tienen","dice","dijo","decir","puede","poder","va","van","ir","tan","tanto","poco","mucho","the","of","and","or","to","in","for","on","with","at","by","from","as","is","was","are","were","be","that","this","it","he","she","they","his","her","not","but","if","have","has","had","do","will","would","unto","thee","thou","thy","behold")
        val counts = HashMap<String, Int>()
        database.rawQuery("SELECT text FROM verses WHERE book_id=?", arrayOf(bookId.toString())).use { cursor ->
            while (cursor.moveToNext()) {
                val normalized = java.text.Normalizer.normalize(cursor.getString(0).lowercase(), java.text.Normalizer.Form.NFD)
                    .replace(Regex("\\p{M}+"), "")
                Regex("[\\p{L}]{3,}").findAll(normalized).forEach { match ->
                    if (match.value !in stop) counts[match.value] = (counts[match.value] ?: 0) + 1
                }
            }
        }
        return counts.entries.sortedWith(compareByDescending<Map.Entry<String, Int>> { it.value }.thenBy { it.key })
            .take(70).associate { it.key to it.value }
    }

    fun search(query: String): List<SearchHit> = database.rawQuery(
        "SELECT v.book_id,b.name,v.chapter,v.verse,v.text FROM verses v " +
            "JOIN books b USING(book_id) WHERE instr(lower(v.text),lower(?))>0 " +
            "ORDER BY b.position,v.chapter,v.verse LIMIT 100",
        arrayOf(query)
    ).use { cursor ->
        buildList {
            while (cursor.moveToNext()) add(
                SearchHit(cursor.getLong(0), cursor.getString(1), cursor.getInt(2),
                    cursor.getInt(3), cursor.getString(4))
            )
        }
    }

    companion object {
        fun open(file: File): BibleModule {
            val db = SQLiteDatabase.openDatabase(file.path, null, SQLiteDatabase.OPEN_READONLY)
            try {
                val version = db.rawQuery("PRAGMA user_version", null).use {
                    check(it.moveToFirst())
                    it.getInt(0)
                }
                require(version == 1) { "Versión SQLite no compatible" }
                val metadata = mutableMapOf<String, String>()
                db.rawQuery("SELECT key,value FROM metadata", null).use { cursor ->
                    while (cursor.moveToNext()) metadata[cursor.getString(0)] = cursor.getString(1)
                }
                require(metadata["schema_version"] == "1" && metadata["feature.verses"] == "true") {
                    "El archivo no es un módulo Biblia Elim v1"
                }
                require(metadata["module_type"] == "bible") { "El módulo no contiene una Biblia" }
                db.rawQuery("SELECT 1 FROM books LIMIT 1", null).use {
                    require(it.moveToFirst()) { "La Biblia no contiene libros" }
                }
                return BibleModule(db, metadata["module_id"] ?: file.nameWithoutExtension,
                    metadata["name"] ?: file.nameWithoutExtension)
            } catch (error: Exception) {
                db.close()
                throw error
            }
        }
    }
}

private class SelectionListener(private val selected: (Int) -> Unit) :
    android.widget.AdapterView.OnItemSelectedListener {
    override fun onNothingSelected(parent: android.widget.AdapterView<*>?) = Unit
    override fun onItemSelected(
        parent: android.widget.AdapterView<*>?, view: android.view.View?, position: Int, id: Long
    ) = selected(position)
}

private class FlowLayout(context: android.content.Context) : ViewGroup(context) {
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val available = (width - paddingLeft - paddingRight).coerceAtLeast(0)
        var x = paddingLeft
        var y = paddingTop
        var rowHeight = 0
        for (i in 0 until childCount) {
            val child = getChildAt(i)
            measureChild(child, widthMeasureSpec, heightMeasureSpec)
            if (x > paddingLeft && x + child.measuredWidth > width - paddingRight) {
                x = paddingLeft
                y += rowHeight + 4
                rowHeight = 0
            }
            x += child.measuredWidth + 2
            rowHeight = maxOf(rowHeight, child.measuredHeight)
        }
        setMeasuredDimension(width, resolveSize(y + rowHeight + paddingBottom, heightMeasureSpec))
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        val width = right - left
        var x = paddingLeft
        var y = paddingTop
        var rowHeight = 0
        for (i in 0 until childCount) {
            val child = getChildAt(i)
            if (x > paddingLeft && x + child.measuredWidth > width - paddingRight) {
                x = paddingLeft
                y += rowHeight + 4
                rowHeight = 0
            }
            child.layout(x, y, x + child.measuredWidth, y + child.measuredHeight)
            x += child.measuredWidth + 2
            rowHeight = maxOf(rowHeight, child.measuredHeight)
        }
    }
}
