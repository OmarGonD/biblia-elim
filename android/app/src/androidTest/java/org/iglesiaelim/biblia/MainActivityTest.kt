package org.iglesiaelim.biblia

import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class MainActivityTest {
    @Test
    fun opensBundledBibleAtGenesis() {
        ActivityScenario.launch(MainActivity::class.java).use { scenario ->
            scenario.onActivity { activity ->
                val visibleText = mutableListOf<String>()
                collectText(activity.window.decorView, visibleText)
                assertTrue(visibleText.any { it == "Biblia Elim" })
                assertTrue(visibleText.any { it.contains("En el principio crió Dios") })
            }
        }
    }

    private fun collectText(view: View, output: MutableList<String>) {
        if (view is TextView) output += view.text.toString()
        if (view is ViewGroup) {
            for (index in 0 until view.childCount) collectText(view.getChildAt(index), output)
        }
    }
}
