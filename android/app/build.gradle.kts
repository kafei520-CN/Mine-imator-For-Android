plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.mineimator.app"
    compileSdk = 36
    ndkVersion = "29.0.13846066"

    defaultConfig {
        applicationId = "com.mineimator.app"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "2.0.2"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_shared")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            isDebuggable = false
            // Same key as the debug builds so this package upgrades the one already installed.
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets.getByName("main") {
        assets.srcDir("../../GmProject/shaders")
        assets.srcDir("../../CppProject/Asset/Shaders")
        assets.srcDir("../../GmProject/datafiles")
        assets.srcDir("E:/Android/sprites")
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

dependencies {
    // QtCore JNI_OnLoad looks up these classes. The Android qtbase package has no host MinGW DLLs for moc, and this jar is the Java side of that same kit.
    implementation(files("E:/Android/Qt/5.15.2/android/jar/QtAndroid.jar"))
}
