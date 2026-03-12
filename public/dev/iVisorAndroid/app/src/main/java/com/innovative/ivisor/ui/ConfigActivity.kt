package com.innovative.ivisor.ui

import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.innovative.ivisor.data.PreferencesManager
import com.innovative.ivisor.databinding.ActivityConfigBinding
import com.innovative.ivisor.model.ConfigConexion
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch

/**
 * Equivalente a IConfigurationManager.GetConnectionString() + FormConnectToSQLServer en WPF.
 * Permite configurar la URL del backend API y opciones del visor.
 */
class ConfigActivity : AppCompatActivity() {

    private lateinit var binding: ActivityConfigBinding
    private lateinit var prefsManager: PreferencesManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityConfigBinding.inflate(layoutInflater)
        setContentView(binding.root)

        prefsManager = PreferencesManager(this)

        // Cargar config guardada
        lifecycleScope.launch {
            val config = prefsManager.configFlow.first()
            binding.etBaseUrl.setText(config.baseUrl)
            binding.switchUseFactor.isChecked = config.useFactor
            binding.rgTipoPrecio.check(
                when (config.tipoPrecio) {
                    1 -> com.innovative.ivisor.R.id.rbPrecio1
                    2 -> com.innovative.ivisor.R.id.rbPrecio2
                    else -> com.innovative.ivisor.R.id.rbPrecio3
                }
            )
        }

        binding.btnConectar.setOnClickListener {
            val url = binding.etBaseUrl.text.toString().trim()
            if (url.isEmpty()) {
                Toast.makeText(this, "Ingrese la URL del servidor", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            // Asegurar que la URL termine en /
            val baseUrl = if (url.endsWith("/")) url else "$url/"

            val tipoPrecio = when (binding.rgTipoPrecio.checkedRadioButtonId) {
                com.innovative.ivisor.R.id.rbPrecio1 -> 1
                com.innovative.ivisor.R.id.rbPrecio2 -> 2
                else -> 3
            }

            val config = ConfigConexion(
                baseUrl = baseUrl,
                useFactor = binding.switchUseFactor.isChecked,
                tipoPrecio = tipoPrecio
            )

            lifecycleScope.launch {
                prefsManager.saveConfig(config)
                startActivity(Intent(this@ConfigActivity, MainActivity::class.java))
            }
        }
    }
}
