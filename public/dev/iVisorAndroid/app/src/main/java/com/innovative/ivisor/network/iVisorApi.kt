package com.innovative.ivisor.network

import retrofit2.Response
import retrofit2.http.GET
import retrofit2.http.Query

interface iVisorApi {

    @GET("api/product")
    suspend fun getProducto(
        @Query("code") codigo: String,
        @Query("type") tipo: Int = 3
    ): Response<ProductoResponse>

    @GET("api/empresa")
    suspend fun getEmpresa(): Response<EmpresaResponse>

    @GET("api/productos/descripciones")
    suspend fun getDescripciones(): Response<List<String>>
}

// Campos exactos que devuelve tu API (iVisor.Api - Program.cs)
data class ProductoResponse(
    val ok: Boolean?,
    val found: Boolean?,
    val code: String?,
    val name: String?,
    val priceBase: Double?,
    val taxPercent: Double?,
    val priceWithTax: Double?,
    val currency: String?,
    val stock: Int?,
    val message: String?,
    val imagenBase64: String?
)

data class EmpresaResponse(
    val descrip: String?,
    val rif: String?,
    val nroSerial: String?,
    val factor: Double?,
    val imagenBase64: String?
)
