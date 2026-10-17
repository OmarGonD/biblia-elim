plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "org.iglesiaelim.biblia"
    compileSdk = 35

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    defaultConfig {
        applicationId = "org.iglesiaelim.biblia"
        minSdk = 23
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    sourceSets["main"].assets.srcDir(layout.buildDirectory.dir("generated/bible-assets"))

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}

dependencies {
    androidTestImplementation("androidx.test:core:1.6.1")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test:runner:1.6.2")
}

val repositoryRoot = rootProject.projectDir.parentFile
val xdgDataHome = System.getenv("XDG_DATA_HOME")
    ?.takeIf { it.isNotBlank() }
    ?.let { File(it) }
    ?: File(System.getProperty("user.home"), ".local/share")
val customLinuxModulesPath = providers.gradleProperty("linuxBibleModules").orNull
    ?: System.getenv("BIBLIA_ELIM_SQLITE_MODULES")?.takeIf { it.isNotBlank() }
val linuxBibleModules = customLinuxModulesPath?.let { File(it) }
    ?: xdgDataHome.resolve("biblia-elim/modules")
val bundledBibleModules = repositoryRoot.resolve("modulos/sqlite")
val generatedBibleAssets = layout.buildDirectory.dir("generated/bible-assets")
val installedLinuxBibles = linuxBibleModules.listFiles { file -> file.isFile && file.extension == "sqlite" }
    .orEmpty()

val prepareBibleAssets by tasks.registering(Copy::class) {
    val bibleSource = if (installedLinuxBibles.isNotEmpty()) linuxBibleModules else bundledBibleModules
    from(bibleSource) {
        include("*.sqlite")
        into("modules")
    }
    from(repositoryRoot.resolve("ui/diccionario-elim.xml"))
    into(generatedBibleAssets)
    outputs.upToDateWhen { false }
    doFirst {
        project.delete(generatedBibleAssets)
        check(bundledBibleModules.isDirectory) { "No se encuentran los módulos SQLite: $bundledBibleModules" }
        if (customLinuxModulesPath != null) {
            check(installedLinuxBibles.isNotEmpty()) {
                "No se encontraron archivos .sqlite en la carpeta indicada: $linuxBibleModules"
            }
        }
        val dictionary = repositoryRoot.resolve("ui/diccionario-elim.xml")
        check(dictionary.isFile) { "No se encuentra el diccionario: $dictionary" }
    }
    doLast {
        val moduleAssets = generatedBibleAssets.get().asFile.resolve("modules")
        val hashes = moduleAssets.listFiles { file -> file.isFile && file.extension == "sqlite" }
            .orEmpty().sortedBy { it.name }.map { file ->
                val digest = java.security.MessageDigest.getInstance("SHA-256")
                file.inputStream().use { input ->
                    val buffer = ByteArray(64 * 1024)
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        digest.update(buffer, 0, count)
                    }
                }
                val hex = digest.digest().joinToString("") { "%02x".format(it) }
                "$hex  ${file.name}"
            }
        check(hashes.isNotEmpty()) { "No se encontraron módulos SQLite para Android" }
        moduleAssets.resolve("manifest.sha256").writeText(hashes.joinToString("\n", postfix = "\n"))
    }
}

tasks.named("preBuild").configure { dependsOn(prepareBibleAssets) }
