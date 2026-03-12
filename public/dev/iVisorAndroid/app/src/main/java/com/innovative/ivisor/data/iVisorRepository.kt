package com.innovative.ivisor.data

import android.util.Base64
import com.innovative.ivisor.model.ConfigConexion
import com.innovative.ivisor.model.Empresa
import com.innovative.ivisor.model.Producto
import com.innovative.ivisor.network.RetrofitClient

sealed class Result<out T> {
    data class Success<T>(val data: T) : Result<T>()
    data class Error(val message: String) : Result<Nothing>()
    object Loading : Result<Nothing>()
}

class iVisorRepository(private val config: ConfigConexion) {

    private val api get() = RetrofitClient.getApi(config.baseUrl)

    suspend fun buscarProducto(codigo: String): Result<Producto> {
        return try {
            val response = api.getProducto(codigo.trim(), config.tipoPrecio)
            if (response.isSuccessful) {
                val body = response.body()
                if (body != null) {
                    // Si found=false, el producto no existe
                    if (body.found == false) {
                        return Result.Error(body.message ?: "PRODUCTO NO ENCONTRADO")
                    }
                    val imagen = body.imagenBase64?.let {
                        try { Base64.decode(it, Base64.DEFAULT) } catch (e: Exception) { null }
                    }
                    Result.Success(
                        Producto(
                            ok           = body.ok ?: false,
                            found        = body.found ?: false,
                            code         = body.code ?: "",
                            name         = body.name,
                            priceBase    = body.priceBase ?: 0.0,
                            taxPercent   = body.taxPercent ?: 0.0,
                            priceWithTax = body.priceWithTax ?: 0.0,
                            currency     = body.currency ?: "",
                            stock        = body.stock ?: 0,
                            message      = body.message,
                            imagen       = imagen
                        )
                    )
                } else {
                    Result.Error("PRODUCTO NO ENCONTRADO")
                }
            } else if (response.code() == 404) {
                Result.Error("PRODUCTO NO ENCONTRADO")
            } else {
                Result.Error("Error del servidor: ${response.code()}")
            }
        } catch (e: Exception) {
            Result.Error("Error de conexión: ${e.message}")
        }
    }

    suspend fun getEmpresa(): Result<Empresa> {
        return try {
            val response = api.getEmpresa()
            if (response.isSuccessful) {
                val body = response.body()
                if (body != null) {
                    val imagen = body.imagenBase64?.let {
                        try { Base64.decode(it, Base64.DEFAULT) } catch (e: Exception) { null }
                    }
                    Result.Success(
                        Empresa(
                            descrip    = body.descrip ?: "",
                            rif        = body.rif ?: "",
                            nroSerial  = body.nroSerial ?: "",
                            factor     = body.factor ?: 1.0,
                            imagen     = imagen
                        )
                    )
                } else {
                    Result.Error("Sin información de empresa")
                }
            } else {
                Result.Error("Error del servidor: ${response.code()}")
            }
        } catch (e: Exception) {
            Result.Error("Error de conexión: ${e.message}")
        }
    }

    suspend fun getDescripciones(): List<String> {
        return try {
            val response = api.getDescripciones()
            if (response.isSuccessful) response.body() ?: emptyList()
            else emptyList()
        } catch (e: Exception) {
            emptyList()
        }
    }
}
