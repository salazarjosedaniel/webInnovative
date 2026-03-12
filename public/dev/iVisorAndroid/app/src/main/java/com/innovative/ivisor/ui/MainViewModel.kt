package com.innovative.ivisor.ui

import androidx.lifecycle.*
import com.innovative.ivisor.data.Result
import com.innovative.ivisor.data.iVisorRepository
import com.innovative.ivisor.model.ConfigConexion
import com.innovative.ivisor.model.Empresa
import com.innovative.ivisor.model.Producto
import kotlinx.coroutines.launch

class MainViewModel(private val config: ConfigConexion) : ViewModel() {

    private val repository = iVisorRepository(config)

    // Estado del producto buscado
    private val _productoState = MutableLiveData<UiState<Producto>>()
    val productoState: LiveData<UiState<Producto>> = _productoState

    // Datos de la empresa
    private val _empresaState = MutableLiveData<UiState<Empresa>>()
    val empresaState: LiveData<UiState<Empresa>> = _empresaState

    // Sugerencias de autocomplete
    private val _descripciones = MutableLiveData<List<String>>(emptyList())
    val descripciones: LiveData<List<String>> = _descripciones

    // Indica si se debe mostrar precio en dólares
    val mostrarDolares: Boolean get() = config.useFactor

    init {
        cargarEmpresa()
        cargarDescripciones()
    }

    /**
     * Equivalente a SetData(codprod) en MainWindow.xaml.cs
     */
    fun buscarProducto(codigo: String) {
        if (codigo.isBlank()) return
        viewModelScope.launch {
            _productoState.value = UiState.Loading
            when (val result = repository.buscarProducto(codigo)) {
                is Result.Success -> _productoState.value = UiState.Success(result.data)
                is Result.Error   -> _productoState.value = UiState.Error(result.message)
                else -> {}
            }
        }
    }

    /**
     * Equivalente a SetDataConf() en MainWindow.xaml.cs
     */
    private fun cargarEmpresa() {
        viewModelScope.launch {
            _empresaState.value = UiState.Loading
            when (val result = repository.getEmpresa()) {
                is Result.Success -> _empresaState.value = UiState.Success(result.data)
                is Result.Error   -> _empresaState.value = UiState.Error(result.message)
                else -> {}
            }
        }
    }

    private fun cargarDescripciones() {
        viewModelScope.launch {
            _descripciones.value = repository.getDescripciones()
        }
    }
}

// ─── Factory para inyectar ConfigConexion ────────────────────────────────────
class MainViewModelFactory(private val config: ConfigConexion) : ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(MainViewModel::class.java)) {
            @Suppress("UNCHECKED_CAST")
            return MainViewModel(config) as T
        }
        throw IllegalArgumentException("Unknown ViewModel class")
    }
}

// ─── Sealed class para estados de UI ─────────────────────────────────────────
sealed class UiState<out T> {
    object Loading : UiState<Nothing>()
    data class Success<T>(val data: T) : UiState<T>()
    data class Error(val message: String) : UiState<Nothing>()
}
