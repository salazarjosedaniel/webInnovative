package com.innovative.ivisor.model

/**
 * Modelo de Producto — campos mapeados al JSON real de iVisor.Api
 */
data class Producto(
    val ok: Boolean = false,
    val found: Boolean = false,
    val code: String = "",
    val name: String? = null,
    val priceBase: Double? = 0.0,
    val taxPercent: Double? = 0.0,
    val priceWithTax: Double? = 0.0,
    val currency: String = "",
    val stock: Int? = 0,
    val message: String? = null,
    val imagen: ByteArray? = null
) {
    val precioIva: Double get() = (priceBase ?: 0.0) * ((taxPercent ?: 0.0) / 100.0)
    val totalBs: Double get() = (priceBase ?: 0.0) + precioIva

    fun totalDolares(factor: Double): Double =
        if (factor > 0) totalBs / factor else 0.0

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is Producto) return false
        return code == other.code
    }
    override fun hashCode(): Int = code.hashCode()
}

/**
 * Configuración de la empresa
 */
data class Empresa(
    val descrip: String = "",
    val rif: String = "",
    val nroSerial: String = "",
    val factor: Double = 1.0,
    val imagen: ByteArray? = null
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is Empresa) return false
        return rif == other.rif
    }
    override fun hashCode(): Int = rif.hashCode()
}

/**
 * Configuración de conexión al backend
 */
data class ConfigConexion(
    val baseUrl: String = "http://192.168.1.100:8080/",
    val useFactor: Boolean = true,
    val tipoPrecio: Int = 3
)
