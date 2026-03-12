package com.innovative.ivisor.data

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import com.innovative.ivisor.model.ConfigConexion
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.map
import java.io.IOException

val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "ivisor_config")

class PreferencesManager(private val context: Context) {

    companion object {
        val KEY_BASE_URL = stringPreferencesKey("base_url")
        val KEY_USE_FACTOR = booleanPreferencesKey("use_factor")
        val KEY_TIPO_PRECIO = intPreferencesKey("tipo_precio")
    }

    val configFlow: Flow<ConfigConexion> = context.dataStore.data
        .catch { exception ->
            if (exception is IOException) emit(emptyPreferences())
            else throw exception
        }
        .map { prefs ->
            ConfigConexion(
                baseUrl = prefs[KEY_BASE_URL] ?: "http://192.168.1.100:8080/",
                useFactor = prefs[KEY_USE_FACTOR] ?: true,
                tipoPrecio = prefs[KEY_TIPO_PRECIO] ?: 3
            )
        }

    suspend fun saveConfig(config: ConfigConexion) {
        context.dataStore.edit { prefs ->
            prefs[KEY_BASE_URL] = config.baseUrl
            prefs[KEY_USE_FACTOR] = config.useFactor
            prefs[KEY_TIPO_PRECIO] = config.tipoPrecio
        }
    }
}
