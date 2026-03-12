package com.innovative.ivisor.ui

import android.graphics.BitmapFactory
import android.os.Bundle
import android.view.KeyEvent
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import android.widget.ArrayAdapter
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.isVisible
import androidx.lifecycle.lifecycleScope
import com.innovative.ivisor.data.PreferencesManager
import com.innovative.ivisor.databinding.ActivityMainBinding
import com.innovative.ivisor.model.ConfigConexion
import com.innovative.ivisor.model.Empresa
import com.innovative.ivisor.model.Producto
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import java.text.NumberFormat
import java.util.Locale

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var viewModel: MainViewModel
    private lateinit var config: ConfigConexion

    private val nfi = NumberFormat.getNumberInstance(Locale("es", "VE")).apply {
        minimumFractionDigits = 2
        maximumFractionDigits = 2
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        lifecycleScope.launch {
            config = PreferencesManager(this@MainActivity).configFlow.first()
            val factory = MainViewModelFactory(config)
            val vm: MainViewModel by viewModels { factory }
            viewModel = vm
            setupUI()
            observeViewModel()
        }
    }

    private fun setupUI() {
        binding.btnBuscar.setOnClickListener { buscarProducto() }

        binding.etCodigo.setOnEditorActionListener { _, actionId, event ->
            if (actionId == EditorInfo.IME_ACTION_SEARCH ||
                event?.keyCode == KeyEvent.KEYCODE_ENTER) {
                buscarProducto()
                true
            } else false
        }

        binding.btnConfig.setOnClickListener { finish() }
        binding.etCodigo.requestFocus()
    }

    private fun buscarProducto() {
        val codigo = binding.etCodigo.text.toString().trim()
        if (codigo.isNotEmpty()) {
            viewModel.buscarProducto(codigo)
            binding.etCodigo.setText("")
            binding.etCodigo.requestFocus()
        }
    }

    private fun observeViewModel() {
        viewModel.empresaState.observe(this) { state ->
            when (state) {
                is UiState.Success -> mostrarEmpresa(state.data)
                is UiState.Error   -> {
                    binding.tvRazonSocial.text = "Sin información"
                    binding.tvRif.text = ""
                }
                is UiState.Loading -> { }
            }
        }

        viewModel.productoState.observe(this) { state ->
            when (state) {
                is UiState.Loading -> mostrarCargando(true)
                is UiState.Success -> { mostrarCargando(false); mostrarProducto(state.data) }
                is UiState.Error   -> { mostrarCargando(false); mostrarError(state.message) }
            }
        }

        viewModel.descripciones.observe(this) { lista ->
            val adapter = ArrayAdapter(this, android.R.layout.simple_dropdown_item_1line, lista)
            binding.etCodigo.setAdapter(adapter)
        }
    }

    private fun mostrarEmpresa(empresa: Empresa) {
        binding.tvRazonSocial.text = empresa.descrip
        binding.tvRif.text = empresa.rif

        if (empresa.imagen != null) {
            val bmp = BitmapFactory.decodeByteArray(empresa.imagen, 0, empresa.imagen.size)
            binding.ivLogoEmpresa.setImageBitmap(bmp)
        }

        val mostrarDolares = config.useFactor && empresa.factor > 1.0
        binding.groupDolares.isVisible = mostrarDolares
    }

    private fun mostrarProducto(producto: Producto) {
        // Nombre del producto
        binding.tvDescripcion.text = producto.name ?: ""
        binding.tvCodigo.text = producto.code

        // Precios — usa priceWithTax si viene, sino calcula
        val precioBase = producto.priceBase ?: 0.0
        val precioIva  = producto.precioIva
        val total      = producto.priceWithTax ?: producto.totalBs

        binding.tvPrecio.text   = nfi.format(precioBase)
        binding.tvIva.text      = nfi.format(precioIva)
        binding.tvTotalBs.text  = nfi.format(total)

        // Precio en dólares
        if (config.useFactor) {
            viewModel.empresaState.value?.let { state ->
                if (state is UiState.Success) {
                    val factor = state.data.factor
                    if (factor > 1.0) {
                        binding.tvTotalDolares.text = nfi.format(producto.totalDolares(factor))
                    }
                }
            }
        }

        // Imagen del producto
        if (producto.imagen != null) {
            val bmp = BitmapFactory.decodeByteArray(producto.imagen, 0, producto.imagen.size)
            binding.ivProducto.setImageBitmap(bmp)
        } else {
            binding.ivProducto.setImageDrawable(null)
        }

        binding.groupProducto.isVisible = true
        binding.tvNoEncontrado.isVisible = false
    }

    private fun mostrarError(mensaje: String) {
        binding.tvDescripcion.text = mensaje
        binding.tvCodigo.text = ""
        binding.tvPrecio.text = "0,00"
        binding.tvIva.text    = "0,00"
        binding.tvTotalBs.text = "0,00"
        binding.tvTotalDolares.text = "0,00"
        binding.ivProducto.setImageDrawable(null)
        binding.tvNoEncontrado.isVisible = true
        binding.groupProducto.isVisible = false
    }

    private fun mostrarCargando(loading: Boolean) {
        binding.progressBar.isVisible = loading
    }
}
