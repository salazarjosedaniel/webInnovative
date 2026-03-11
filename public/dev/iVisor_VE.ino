/***************************************************
 *  VISOR DE PRECIOS - ESP32-S3 + Waveshare 7" + LVGL
 *  Arduino IDE (ESP32 core 3.x) + NimBLE-Arduino
 *
 *  Incluye:
 *   - WiFiManager + URL API configurable (legacy tasas + productos)
 *   - Watchdog (ESP32)
 *   - LED estado (GPIO 2)
 *   - Auto-reconexión WiFi + reset settings si pierde WiFi mucho tiempo
 *   - Registro de dispositivo / logs / status pago (bloquea si no pagado)
 *   - OTA local por navegador + OTA remota por servidor
 *   - BLE Server (RX write): app (nRF/LightBlue) envía código o "CFG"
 *   - BLE Central UART (NUS): scanner BLE UART (NOTIFY)
 *   - BLE Central HID Keyboard: scanner BLE HID (teclado)
 *   - Teclado en pantalla (LVGL): tocar caja -> teclado numérico, ENTER envía
 *   - Mejoras anti-parpadeo: teclado persistente (no recrear), ocultar/mostrar
 *   - Mejora BLE adv: si iPhone apaga BT y vuelve, re-anuncia siempre (watch)
 *
 *  Consulta producto por código:
 *       GET apiProductUrl?code=XXXXXXXX
 *       Espera JSON:
 *         { ok, found, code, name, priceWithTax, currency, ... }
 *
 *  NOTAS IMPORTANTES:
 *   - Si usas USB OTG para algo, este sketch asume OTG mode.
 *   - BLE HID Central depende de que el scanner exponga HID Report (0x2A4D)
 ***************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#include <WebServer.h>
#include <HTTPUpdate.h>
#include <esp_system.h>
#include <WiFiClientSecure.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Preferences.h>
#include <Update.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <StreamUtils.h>

#if ARDUINO_USB_MODE
#error "This sketch should be used when USB is in OTG mode"
#endif

/************ LVGL + Panel ************/
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"

// Si no tienes estos fonts, comenta y usa lv_font_montserrat_xx
#include "font_mont_44.c"
#include "font_mont_22.c"
#include "font_mont_14.c"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

/************ BLE (NimBLE) ************/
#include <NimBLEDevice.h>

/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "driver/gpio.h"
#include "usb/usb_host.h"

#include "hid_host.h"
#include "hid_usage_keyboard.h"
#include "hid_usage_mouse.h"


#define DEBUG_MODE 0

#if DEBUG_MODE

#define LOG(...) Serial.printf(__VA_ARGS__)
#define LOGLN(x) Serial.println(x)
#define LOGT(x) Serial.print(x)
#define LOGF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)

#else

#define LOG(...)
#define LOGLN(x)
#define LOGT(x)
#define LOGF(fmt, ...)

#endif

/**
 * @brief Key event
 */
typedef struct {
  enum key_state { KEY_STATE_PRESSED = 0x00,
                   KEY_STATE_RELEASED = 0x01 } state;
  uint8_t modifier;
  uint8_t key_code;
} key_event_t;


/************ HTTP Worker (evitar crear tasks por escaneo) ************/
static QueueHandle_t httpQueue = nullptr;

struct HttpJob {
  char code[65];
  char source[20];  // "HID USB", "BLE App", etc.
  bool setTextArea;
};

static const char* TAG = "example";
QueueHandle_t hid_host_event_queue;
bool user_shutdown = false;
static SemaphoreHandle_t gPanelReady = nullptr;
static String g_barcode = "";
static volatile bool g_barcode_ready = false;
static String g_barcode_last = "";
// (Opcional) filtro: longitudes típicas para scanner
static bool looksLikeBarcode(const String& s) {
  return s.length() >= 3;  // ajusta a tu caso (EAN13=13, etc.)
}

static inline void handle_scanner_char(uint32_t key_char) {
  char c = (char)key_char;
  if (c == '\r') {
    g_barcode_last = g_barcode;
    g_barcode_ready = true;
    g_barcode = "";
    return;
  }

  // Ignorar control chars
  if ((uint8_t)c < 32) return;

  g_barcode += c;

  // Seguridad por si el scanner manda basura
  if (g_barcode.length() > 64) g_barcode = "";
}

/**
 * @brief HID Host event
 *
 * This event is used for delivering the HID Host event from callback to a task.
 */
typedef struct {
  hid_host_device_handle_t hid_device_handle;
  hid_host_driver_event_t event;
  void* arg;
} hid_host_event_queue_t;

/**
 * @brief HID Protocol string names
 */
static const char* hid_proto_name_str[] = { "NONE", "KEYBOARD", "MOUSE" };

/* Main char symbol for ENTER key */
#define KEYBOARD_ENTER_MAIN_CHAR '\r'
/* When set to 1 pressing ENTER will be extending with LineFeed during serial
 * debug output */
#define KEYBOARD_ENTER_LF_EXTEND 1

/**
 * @brief Scancode to ascii table
 */
const uint8_t keycode2ascii[57][2] = {
  { 0, 0 },                                               /* HID_KEY_NO_PRESS        */
  { 0, 0 },                                               /* HID_KEY_ROLLOVER        */
  { 0, 0 },                                               /* HID_KEY_POST_FAIL       */
  { 0, 0 },                                               /* HID_KEY_ERROR_UNDEFINED */
  { 'a', 'A' },                                           /* HID_KEY_A               */
  { 'b', 'B' },                                           /* HID_KEY_B               */
  { 'c', 'C' },                                           /* HID_KEY_C               */
  { 'd', 'D' },                                           /* HID_KEY_D               */
  { 'e', 'E' },                                           /* HID_KEY_E               */
  { 'f', 'F' },                                           /* HID_KEY_F               */
  { 'g', 'G' },                                           /* HID_KEY_G               */
  { 'h', 'H' },                                           /* HID_KEY_H               */
  { 'i', 'I' },                                           /* HID_KEY_I               */
  { 'j', 'J' },                                           /* HID_KEY_J               */
  { 'k', 'K' },                                           /* HID_KEY_K               */
  { 'l', 'L' },                                           /* HID_KEY_L               */
  { 'm', 'M' },                                           /* HID_KEY_M               */
  { 'n', 'N' },                                           /* HID_KEY_N               */
  { 'o', 'O' },                                           /* HID_KEY_O               */
  { 'p', 'P' },                                           /* HID_KEY_P               */
  { 'q', 'Q' },                                           /* HID_KEY_Q               */
  { 'r', 'R' },                                           /* HID_KEY_R               */
  { 's', 'S' },                                           /* HID_KEY_S               */
  { 't', 'T' },                                           /* HID_KEY_T               */
  { 'u', 'U' },                                           /* HID_KEY_U               */
  { 'v', 'V' },                                           /* HID_KEY_V               */
  { 'w', 'W' },                                           /* HID_KEY_W               */
  { 'x', 'X' },                                           /* HID_KEY_X               */
  { 'y', 'Y' },                                           /* HID_KEY_Y               */
  { 'z', 'Z' },                                           /* HID_KEY_Z               */
  { '1', '!' },                                           /* HID_KEY_1               */
  { '2', '@' },                                           /* HID_KEY_2               */
  { '3', '#' },                                           /* HID_KEY_3               */
  { '4', '$' },                                           /* HID_KEY_4               */
  { '5', '%' },                                           /* HID_KEY_5               */
  { '6', '^' },                                           /* HID_KEY_6               */
  { '7', '&' },                                           /* HID_KEY_7               */
  { '8', '*' },                                           /* HID_KEY_8               */
  { '9', '(' },                                           /* HID_KEY_9               */
  { '0', ')' },                                           /* HID_KEY_0               */
  { KEYBOARD_ENTER_MAIN_CHAR, KEYBOARD_ENTER_MAIN_CHAR }, /* HID_KEY_ENTER */
  { 0, 0 },                                               /* HID_KEY_ESC             */
  { '\b', 0 },                                            /* HID_KEY_DEL             */
  { 0, 0 },                                               /* HID_KEY_TAB             */
  { ' ', ' ' },                                           /* HID_KEY_SPACE           */
  { '-', '_' },                                           /* HID_KEY_MINUS           */
  { '=', '+' },                                           /* HID_KEY_EQUAL           */
  { '[', '{' },                                           /* HID_KEY_OPEN_BRACKET    */
  { ']', '}' },                                           /* HID_KEY_CLOSE_BRACKET   */
  { '\\', '|' },                                          /* HID_KEY_BACK_SLASH      */
  { '\\', '|' },
  /* HID_KEY_SHARP           */  // HOTFIX: for NonUS Keyboards repeat
                                 // HID_KEY_BACK_SLASH
  { ';', ':' },                  /* HID_KEY_COLON           */
  { '\'', '"' },                 /* HID_KEY_QUOTE           */
  { '`', '~' },                  /* HID_KEY_TILDE           */
  { ',', '<' },                  /* HID_KEY_LESS            */
  { '.', '>' },                  /* HID_KEY_GREATER         */
  { '/', '?' }                   /* HID_KEY_SLASH           */
};

/**
 * @brief Makes new line depending on report output protocol type
 *
 * @param[in] proto Current protocol to output
 */
static void hid_print_new_device_report_header(hid_protocol_t proto) {
  static hid_protocol_t prev_proto_output = HID_PROTOCOL_MAX;

  if (prev_proto_output != proto) {
    prev_proto_output = proto;
    printf("\r\n");
    if (proto == HID_PROTOCOL_MOUSE) {
      printf("Mouse\r\n");
    } else if (proto == HID_PROTOCOL_KEYBOARD) {
      printf("Keyboard\r\n");
    } else {
      printf("Generic\r\n");
    }
    fflush(stdout);
  }
}

/**
 * @brief HID Keyboard modifier verification for capitalization application
 * (right or left shift)
 *
 * @param[in] modifier
 * @return true  Modifier was pressed (left or right shift)
 * @return false Modifier was not pressed (left or right shift)
 *
 */
static inline bool hid_keyboard_is_modifier_shift(uint8_t modifier) {
  if (((modifier & HID_LEFT_SHIFT) == HID_LEFT_SHIFT) || ((modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT)) {
    return true;
  }
  return false;
}

/**
 * @brief HID Keyboard get char symbol from key code
 *
 * @param[in] modifier  Keyboard modifier data
 * @param[in] key_code  Keyboard key code
 * @param[in] key_char  Pointer to key char data
 *
 * @return true  Key scancode converted successfully
 * @return false Key scancode unknown
 */
static inline bool hid_keyboard_get_char(uint8_t modifier, uint8_t key_code,
                                         unsigned char* key_char) {
  uint8_t mod = (hid_keyboard_is_modifier_shift(modifier)) ? 1 : 0;

  if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_SLASH)) {
    *key_char = keycode2ascii[key_code][mod];
  } else {
    // All other key pressed
    return false;
  }

  return true;
}

/**
 * @brief HID Keyboard print char symbol
 *
 * @param[in] key_char  Keyboard char to stdout
 */
static inline void hid_keyboard_print_char(unsigned int key_char) {
  if (!!key_char) {
    putchar(key_char);
#if (KEYBOARD_ENTER_LF_EXTEND)
    if (KEYBOARD_ENTER_MAIN_CHAR == key_char) {
      putchar('\n');
    }
#endif  // KEYBOARD_ENTER_LF_EXTEND
    fflush(stdout);
  }
}

/**
 * @brief Key Event. Key event with the key code, state and modifier.
 *
 * @param[in] key_event Pointer to Key Event structure
 *
 */
static void key_event_callback(key_event_t* key_event) {
  unsigned char key_char;

  hid_print_new_device_report_header(HID_PROTOCOL_KEYBOARD);

  if (key_event->KEY_STATE_PRESSED == key_event->state) {
    if (hid_keyboard_get_char(key_event->modifier, key_event->key_code,
                              &key_char)) {
      //hid_keyboard_print_char(key_char);
      handle_scanner_char(key_char);
    }
  }
}

/**
 * @brief Key buffer scan code search.
 *
 * @param[in] src       Pointer to source buffer where to search
 * @param[in] key       Key scancode to search
 * @param[in] length    Size of the source buffer
 */
static inline bool key_found(const uint8_t* const src, uint8_t key,
                             unsigned int length) {
  for (unsigned int i = 0; i < length; i++) {
    if (src[i] == key) {
      return true;
    }
  }
  return false;
}

/**
 * @brief USB HID Host Keyboard Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_keyboard_report_callback(const uint8_t* const data,
                                              const int length) {
  hid_keyboard_input_report_boot_t* kb_report =
    (hid_keyboard_input_report_boot_t*)data;

  if (length < sizeof(hid_keyboard_input_report_boot_t)) {
    return;
  }

  static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = { 0 };
  key_event_t key_event;

  for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {

    // key has been released verification
    if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED && !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
      key_event.key_code = prev_keys[i];
      key_event.modifier = 0;
      key_event.state = key_event.KEY_STATE_RELEASED;
      key_event_callback(&key_event);
    }

    // key has been pressed verification
    if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED && !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
      key_event.key_code = kb_report->key[i];
      key_event.modifier = kb_report->modifier.val;
      key_event.state = key_event.KEY_STATE_PRESSED;
      key_event_callback(&key_event);
    }
  }

  memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

/**
 * @brief USB HID Host Mouse Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_mouse_report_callback(const uint8_t* const data,
                                           const int length) {
  hid_mouse_input_report_boot_t* mouse_report =
    (hid_mouse_input_report_boot_t*)data;

  if (length < sizeof(hid_mouse_input_report_boot_t)) {
    return;
  }

  static int x_pos = 0;
  static int y_pos = 0;

  // Calculate absolute position from displacement
  x_pos += mouse_report->x_displacement;
  y_pos += mouse_report->y_displacement;

  hid_print_new_device_report_header(HID_PROTOCOL_MOUSE);

  printf("X: %06d\tY: %06d\t|%c|%c|\r", x_pos, y_pos,
         (mouse_report->buttons.button1 ? 'o' : ' '),
         (mouse_report->buttons.button2 ? 'o' : ' '));
  fflush(stdout);
}

/**
 * @brief USB HID Host Generic Interface report callback handler
 *
 * 'generic' means anything else than mouse or keyboard
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_generic_report_callback(const uint8_t* const data,
                                             const int length) {
  hid_print_new_device_report_header(HID_PROTOCOL_NONE);
  for (int i = 0; i < length; i++) {
    printf("%02X", data[i]);
  }
  putchar('\r');
  putchar('\n');
  fflush(stdout);
}

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void* arg) {
  uint8_t data[64] = { 0 };
  size_t data_length = 0;
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

  switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
      ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
        hid_device_handle, data, 64, &data_length));

      if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
        if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
          hid_host_keyboard_report_callback(data, data_length);
        } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
          hid_host_mouse_report_callback(data, data_length);
        }
      } else {
        hid_host_generic_report_callback(data, data_length);
      }

      break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",
               hid_proto_name_str[dev_params.proto]);
      ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
      LOGF("HID Device, protocol '%s' DISCONNECTED", hid_proto_name_str[dev_params.proto]);
      break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
      ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",
               hid_proto_name_str[dev_params.proto]);
      break;
    default:
      ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event",
               hid_proto_name_str[dev_params.proto]);
      LOGF("HID Device, protocol '%s' Unhandled event", hid_proto_name_str[dev_params.proto]);
      break;
  }
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event, void* arg) {
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));
  const hid_host_device_config_t dev_config = {
    .callback = hid_host_interface_callback, .callback_arg = NULL
  };


  switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
      ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED",
               hid_proto_name_str[dev_params.proto]);
      LOGF("HID Device, protocol '%s' CONNECTED", hid_proto_name_str[dev_params.proto]);
      ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
      if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
        ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle,
                                                       HID_REPORT_PROTOCOL_BOOT));
        if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
          ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
        }
      }
      ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
      break;
    default:
      break;
  }
}

/**
 * @brief Start USB Host install and handle common USB host library events while
 * app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void* arg) {


  // Espera a que el panel termine begin()
  if (gPanelReady) {
    Serial.println("[USB] Waiting panel ready...");
    xSemaphoreTake(gPanelReady, portMAX_DELAY);
    Serial.println("[USB] Panel ready, starting USB...");
  }

  const usb_host_config_t host_config = {
    .skip_phy_setup = false,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };

  esp_err_t err = usb_host_install(&host_config);
  LOGF("[USB] usb_host_install -> %d\n", (int)err);

  // Avisamos a app_main que el USB ya quedó instalado
  xTaskNotifyGive((TaskHandle_t)arg);

  uint32_t last_alive = millis();

  while (true) {
    uint32_t event_flags = 0;

    // ⚠️ NO usar portMAX_DELAY aquí
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

    // 🔄 Alive cada 2 segundos
    if (millis() - last_alive > 60000) {
      last_alive = millis();
      LOGF("[USB] alive | flags=0x%08lx\n",
           (unsigned long)event_flags);
    }

    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      usb_host_device_free_all();
    }

    // Cede CPU al RTOS
    vTaskDelay(1);
  }



  /**
  const usb_host_config_t host_config = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };

  ESP_ERROR_CHECK(usb_host_install(&host_config));
  xTaskNotifyGive((TaskHandle_t)arg);

  while (true) {
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      usb_host_device_free_all();
    }
  }*/

  /*const gpio_config_t input_pin = {
      .pin_bit_mask = BIT64(APP_QUIT_PIN),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&input_pin));

  const usb_host_config_t host_config = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };

  ESP_ERROR_CHECK(usb_host_install(&host_config));
  xTaskNotifyGive((TaskHandle_t)arg);

  while (gpio_get_level(APP_QUIT_PIN) != 0) {
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

    // Release devices once all clients has deregistered
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      usb_host_device_free_all();
      ESP_LOGI(TAG, "USB Event flags: NO_CLIENTS");
    }
    // All devices were removed
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
      ESP_LOGI(TAG, "USB Event flags: ALL_FREE");
    }
  }
  // App Button was pressed, trigger the flag
  user_shutdown = true;
  ESP_LOGI(TAG, "USB shutdown");
  // Clean up USB Host
  vTaskDelay(10); // Short delay to allow clients clean-up
  ESP_ERROR_CHECK(usb_host_uninstall());
  vTaskDelete(NULL);*/
}

/**
 * @brief HID Host main task
 *
 * Creates queue and get new event from the queue
 *
 * @param[in] pvParameters Not used
 */
void hid_host_task(void* pvParameters) {
  hid_host_event_queue_t evt_queue;
  // Wait queue
  while (!user_shutdown) {
    //Serial.println("[HID] alive q=<uxQueueMessagesWaiting()>");
    if (xQueueReceive(hid_host_event_queue, &evt_queue, pdMS_TO_TICKS(50))) {
      hid_host_device_event(evt_queue.hid_device_handle, evt_queue.event,
                            evt_queue.arg);
    }
  }

  xQueueReset(hid_host_event_queue);
  vQueueDelete(hid_host_event_queue);
  vTaskDelete(NULL);
}

/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event             HID Device event
 * @param[in] arg               Not used
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event, void* arg) {
  Serial.println("[USB] device callback fired");
  const hid_host_event_queue_t evt_queue = {
    .hid_device_handle = hid_device_handle, .event = event, .arg = arg
  };
  xQueueSend(hid_host_event_queue, &evt_queue, 0);
}

void app_main(void) {
  BaseType_t task_created;
  ESP_LOGI(TAG, "HID Host example");

  /*
   * Create usb_lib_task to:
   * - initialize USB Host library
   * - Handle USB Host events while APP pin in in HIGH state
   */

  // ✅ Crear cola ANTES de instalar HID
  hid_host_event_queue = xQueueCreate(10, sizeof(hid_host_event_queue_t));
  assert(hid_host_event_queue != NULL);

  task_created =
    xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096,
                            xTaskGetCurrentTaskHandle(), 2, NULL, 0);
  assert(task_created == pdTRUE);

  // Wait for notification from usb_lib_task to proceed
  ulTaskNotifyTake(false, 1000);

  /*
   * HID host driver configuration
   * - create background task for handling low level event inside the HID driver
   * - provide the device callback to get new HID Device connection event
   */
  const hid_host_driver_config_t hid_host_driver_config = {
    .create_background_task = true,
    .task_priority = 5,
    .stack_size = 4096,
    .core_id = 0,
    .callback = hid_host_device_callback,
    .callback_arg = NULL
  };

  ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

  // Task is working until the devices are gone (while 'user_shutdown' if false)
  user_shutdown = false;

  /*
   * Create HID Host task process for handle events
   * IMPORTANT: Task is necessary here while there is no possibility to interact
   * with USB device from the callback.
   */
  task_created =
    xTaskCreatePinnedToCore(hid_host_task, "hid_task", 4 * 1024, NULL, 2, NULL, 0);
  assert(task_created == pdTRUE);
}

/************ Firmware ************/
#define FW_VERSION "1.0.1"

/************ LED de estado ************/
const uint8_t LED_PIN = 2;

/************ Watchdog ************/
const int WDT_TIMEOUT_SECONDS = 180;

/************ Estados de red ************/
enum NetState { NET_NO_WIFI,
                NET_NO_INTERNET,
                NET_OK };
NetState netState = NET_NO_WIFI;

/************ Timers ************/
unsigned long lastLedToggle = 0;
bool ledOn = false;

unsigned long wifiFallidaDesde = 0;
const unsigned long TIEMPO_MAX_SIN_WIFI = 600000;  // 10 min
const unsigned long INTERVALO_API = 6000;          // 6s
const unsigned long FW_CHECK_INTERVAL = 5UL * 60UL * 1000UL;

/************ NTP ************/
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -4 * 3600;  // GMT-4
const int daylightOffset_sec = 0;

/************ Boot safety ************/
unsigned long bootTime = 0;
const unsigned long MAX_UPTIME = 8UL * 60UL * 60UL * 1000UL;  // 4 horas

/************ OTA Local ************/
WebServer otaServer(80);

/************ Configuración API ************/
String apiUrl = "";         // legacy tasas
String apiProductUrl = "";  // endpoint productos

/************ Pago / dispositivo ************/
static bool servicePaid = true;
static unsigned long lastPaidCheckMs = 0;
static const unsigned long PAID_CHECK_MS = 15000;  // cada 15s

static bool deviceRegistered = false;
static bool lastWifiWasConnected = false;

/************ Preferences (NVS) ************/
Preferences prefs;
static const char* NVS_NS = "cfg";
static const char* KEY_API_LEGACY = "api_legacy";
static const char* KEY_API_PRODUCT = "api_product";
static const char* KEY_SCANNER_PREFIX = "scanner_prefix";
static const char* KEY_SCANNER_MODE = "scanner_mode";
static const char* KEY_OTA_PENDING = "ota_pending";  // bool
static const char* KEY_OTA_URL = "ota_url";          // string
static const char* KEY_OTA_VER = "ota_ver";          // string (opcional)
bool pendingResetCfg = false;
RTC_DATA_ATTR uint32_t g_forcePortal = 0x0;
static const uint32_t FORCE_PORTAL_MAGIC = 0xC0FFEE11;
static volatile bool g_portalMode = false;
/************ WiFiManager buffers ************/
char apiLegacyBuf[200] = "https://web-innovative.vercel.app/api/tasas";
char apiProdBuf[200] = "http://192.168.100.242/api/product";
char apiRegistro[200] = "https://web-innovative.vercel.app/api/device/ensure";
char apiVerificarPago[200] = "https://web-innovative.vercel.app/api/device/status?deviceId=";
char apiSendLog[200] = "https://web-innovative.vercel.app/api/device/log";

/************ WiFiManager params ************/
WiFiManagerParameter* p_apiLegacy = nullptr;
WiFiManagerParameter* p_apiProduct = nullptr;

/************ Scanner config ************/
// Prefijo (configurable) para buscar scanner por nombre
static String scannerNamePrefix = "EY";  // default

// Modo de conexión scanner: 0=AUTO 1=UART 2=HID
static int scannerMode = 0;
/************ Config apply deferred (EVITAR crash LVGL) ************/
static volatile bool pendingApplyScannerMode = false;
static volatile int pendingScannerModeValue = 0;

/************ LVGL Timers ************/
static lv_timer_t* timer_api = nullptr;
static lv_timer_t* timer_wifi = nullptr;
static lv_timer_t* timer_fw = nullptr;
static lv_timer_t* timer_led = nullptr;
static lv_timer_t* timer_ui = nullptr;
static lv_timer_t* timer_ble_adv = nullptr;

/**/
static String g_lastSentCode = "";
static uint32_t g_lastSentMs = 0;
static const uint32_t SEND_ONCE_WINDOW_MS = 1000;  // ajusta: 400-1200ms

/************ Panel ************/
Board* panelBoard = nullptr;

/************ UI objects ************/
lv_obj_t* lblHeader = nullptr;
lv_obj_t* taBarcode = nullptr;
lv_obj_t* lblDesc = nullptr;
lv_obj_t* lblPrice = nullptr;
lv_obj_t* lblPriceUsd = nullptr;
lv_obj_t* lblIva = nullptr;
lv_obj_t* lblCurrency = nullptr;
lv_obj_t* lblRateUsd = nullptr;
lv_obj_t* lblStatus = nullptr;
lv_obj_t* lblDevice = nullptr;
/************ UI Configuración Scanner ************/
static lv_obj_t* screenConfig = nullptr;
static lv_obj_t* homeScreen = nullptr;  // <-- NUEVO: pantalla principal
static lv_obj_t* ddScannerMode = nullptr;
static lv_obj_t* btnCfg = nullptr;
static lv_obj_t* msgboxSaved = nullptr;
// --- Config Gate + Reset ---
static const char* CFG_UI_PASSWORD = "1234";  // <-- CAMBIA TU CLAVE AQUI
static lv_obj_t* screenCfgPass = nullptr;
static lv_obj_t* taCfgPass = nullptr;
static lv_obj_t* kbCfgPass = nullptr;
static lv_obj_t* lblCfgPassMsg = nullptr;
/************ UI Config extra ************/
static lv_obj_t* lblIpCfg = nullptr;
static lv_obj_t* lblApiLegacyCfg = nullptr;
static lv_obj_t* lblApiProductCfg = nullptr;
// Teclado persistente (anti-parpadeo)
static lv_obj_t* kb = nullptr;
static lv_obj_t* kb_bg = nullptr;
static bool kbVisible = false;

/************ Datos actuales visor ************/
static String curCode = "";
static String curDesc = "Escanee un producto....";
static String curPrice = "--.--";

/************ Cache prev para evitar flicker ************/
static String prevCode, prevDesc, prevPrice, prevStatus, prevDevice;
static String prevPriceUsd, prevRateUsd, prevCurrency, prevIva;

/************ USD rate (API legacy) ************/
static double usdRate = 0.0;
static unsigned long lastUsdFetch = 0;
static const unsigned long USD_FETCH_MS = 2UL * 60UL * 60UL * 1000UL;
;  // cada 2H

/************ Flags ************/
bool usarLVGL = true;
bool systemReady = false;
bool lvglWarmupDone = false;
bool shouldRebootAfterConfig = false;

static bool resultShowing = false;
static unsigned long resultUntilMs = 0;
static const unsigned long RESULT_SHOW_MS = 5000;

/************ BLE Server (RX para app) ************/
static const char* BLE_SERVICE_UUID = "12345678-1234-1234-1234-1234567890ab";
static const char* BLE_RX_UUID = "12345678-1234-1234-1234-1234567890ac";
static NimBLECharacteristic* chrRX = nullptr;

/************ BLE CENTRAL UART (NUS) ************/
static NimBLEUUID UART_SERVICE_UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static NimBLEUUID UART_NOTIFY_UUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");  // Notify (TX del scanner)

static NimBLEClient* bleClientUart = nullptr;
static NimBLERemoteCharacteristic* uartNotify = nullptr;
static bool scannerUartConnected = false;
static String centralUartRxBuffer = "";

/************ IDLE PROMOS ************/
static lv_obj_t* idleBox = nullptr;
static lv_obj_t* lblIdleTitle = nullptr;
static lv_obj_t* lblIdleBody = nullptr;

static lv_timer_t* timer_idle_promo = nullptr;

static bool idleModeActive = true;
static unsigned long lastUserActivityMs = 0;
static const unsigned long IDLE_RETURN_MS = 15000;   // volver a promos tras 15s sin actividad
static const unsigned long IDLE_ROTATE_MS = 5000;    // cambiar promo cada 5s

static int idlePromoIndex = 0;

static const char* idlePromoTitles[] = {
  "Promocion",
  "Informacion",
  "Oferta",
  "Recordatorio"
};

static const char* idlePromoBodies[] = {
  "Lleve 2 y pague 1 en productos seleccionados.",
  "Consulte nuestros precios actualizados al instante.",
  "Pregunte por descuentos especiales del dia.",
  "Escanee un producto para ver precio y tasa BCV."
};

static const int idlePromoCount = sizeof(idlePromoTitles) / sizeof(idlePromoTitles[0]);

static void updateIdlePromo() {
  if (!usarLVGL || !idleBox || !lblIdleTitle || !lblIdleBody) return;
  if (!idleModeActive) return;

  lvgl_port_lock(-1);
  lv_label_set_text(lblIdleTitle, idlePromoTitles[idlePromoIndex]);
  lv_label_set_text(lblIdleBody, idlePromoBodies[idlePromoIndex]);
  lvgl_port_unlock();

  idlePromoIndex++;
  if (idlePromoIndex >= idlePromoCount) idlePromoIndex = 0;
}

static void showIdleMode() {
  idleModeActive = true;

  if (!usarLVGL) return;
  lvgl_port_lock(-1);

  if (idleBox) lv_obj_clear_flag(idleBox, LV_OBJ_FLAG_HIDDEN);

  lvgl_port_unlock();

  updateIdlePromo();
}

static void hideIdleMode() {
  idleModeActive = false;

  if (!usarLVGL) return;
  lvgl_port_lock(-1);

  if (idleBox) lv_obj_add_flag(idleBox, LV_OBJ_FLAG_HIDDEN);

  lvgl_port_unlock();
}

static void markUserActivity() {
  lastUserActivityMs = millis();
  hideIdleMode();
}

static void timer_idle_promo_cb(lv_timer_t* t) {
  (void)t;
  if (idleModeActive) {
    updateIdlePromo();
  }
}

/************ BLE CENTRAL HID (Keyboard) ************/
// HID Service UUID = 0x1812
static NimBLEUUID HID_SERVICE_UUID((uint16_t)0x1812);
// Report characteristic UUID = 0x2A4D
static NimBLEUUID HID_REPORT_UUID((uint16_t)0x2A4D);

static NimBLEClient* bleClientHid = nullptr;
static NimBLERemoteCharacteristic* hidReportChar = nullptr;
static bool scannerHidConnected = false;

static String centralHidRxBuffer = "";
static uint8_t lastHidReport[16];
static size_t lastHidReportLen = 0;

/************ Helpers ************/
static String getDeviceId() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toUpperCase();
  return mac;
}

static String getFwInfoUrl() {
  return String("https://web-innovative.vercel.app/api/fw/info?deviceId=") + getDeviceId();
}

/************ UI helpers ************/
static void scheduleClearIn5s() {
  resultShowing = true;
  resultUntilMs = millis() + RESULT_SHOW_MS;
}

static void uiSetStatus(const String& s) {
  if (!usarLVGL || !lblStatus) return;
  if (s == prevStatus) return;
  lvgl_port_lock(-1);
  lv_label_set_text(lblStatus, s.c_str());
  lvgl_port_unlock();
  prevStatus = s;
}

static void uiSetDeviceId() {
  String dev = getDeviceId();
  if (dev == prevDevice) return;
  lvgl_port_lock(-1);
  if (lblDevice) lv_label_set_text_fmt(lblDevice, "ID: %s", dev.c_str());
  lvgl_port_unlock();
  prevDevice = dev;
}


static void uiSetBarcode(const String& code) {
  if (!usarLVGL || !taBarcode) return;
  if (code == prevCode) return;
  lvgl_port_lock(-1);
  lv_textarea_set_text(taBarcode, code.c_str());
  lvgl_port_unlock();
  prevCode = code;
}

static void uiSetDesc(const String& desc) {
  if (!usarLVGL || !lblDesc) return;
  if (desc == prevDesc) return;
  lvgl_port_lock(-1);
  lv_label_set_text(lblDesc, desc.c_str());
  lvgl_port_unlock();
  prevDesc = desc;
}

static void uiSetPrice(const String& priceOnly) {
  if (!usarLVGL || !lblPrice) return;
  if (priceOnly == prevPrice) return;
  lvgl_port_lock(-1);
  lv_label_set_text_fmt(lblPrice, "%s Bs.", priceOnly.c_str());
  lvgl_port_unlock();
  prevPrice = priceOnly;
}

static void uiSetPriceUsd(const String& priceUsdOnly) {
  if (!usarLVGL || !lblPriceUsd) return;
  if (priceUsdOnly == prevPriceUsd) return;
  lvgl_port_lock(-1);
  lv_label_set_text_fmt(lblPriceUsd, "%s $", priceUsdOnly.c_str());
  lvgl_port_unlock();
  prevPriceUsd = priceUsdOnly;
}

static void uiSetCurrency(const String& Currency) {
  if (!usarLVGL || !lblCurrency) return;
  if (Currency == prevCurrency) return;
  lvgl_port_lock(-1);
  lv_label_set_text_fmt(lblCurrency, "%s", Currency.c_str());
  lvgl_port_unlock();
  prevCurrency = Currency;
}

static void uiSetRateUsd(const String& rateUsdOnly) {
  if (!usarLVGL || !lblRateUsd) return;
  if (rateUsdOnly == prevRateUsd) return;
  lvgl_port_lock(-1);
  lv_label_set_text_fmt(lblRateUsd, "%s Bs.", rateUsdOnly.c_str());
  lvgl_port_unlock();
  prevRateUsd = rateUsdOnly;
}

static void uiSetIva(const String& ivaText) {
  if (!usarLVGL || !lblIva) return;
  if (ivaText == prevIva) return;
  lvgl_port_lock(-1);
  lv_label_set_text(lblIva, ivaText.c_str());
  lvgl_port_unlock();
  prevIva = ivaText;
}

static void clearProductUI() {
  curCode = "";
  curDesc = "Escanee un producto...";
  curPrice = "--.--";
  uiSetBarcode("");
  uiSetDesc(curDesc);
  uiSetPrice(curPrice);
  uiSetPriceUsd("--.--");
  uiSetIva("IVA: --.-- Bs.");
  uiSetStatus(servicePaid ? "Listo. Escanee..." : "Servicio suspendido");
}


static void uiUpdateConfigInfo() {
  if (!usarLVGL) return;

  String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("Sin WiFi");
  String legacy = apiUrl;
  legacy.trim();
  String prod = apiProductUrl;
  prod.trim();

  lvgl_port_lock(-1);

  if (lblIpCfg) lv_label_set_text_fmt(lblIpCfg, "IP: %s", ip.c_str());

  if (lblApiLegacyCfg) {
    String t = "Tasas URL:\n" + legacy;
    lv_label_set_text(lblApiLegacyCfg, t.c_str());
  }

  if (lblApiProductCfg) {
    String t = "Producto URL:\n" + prod;
    lv_label_set_text(lblApiProductCfg, t.c_str());
  }

  lvgl_port_unlock();
}

/************ LED indicador ************/
static void actualizarLed() {
  unsigned long now = millis();
  switch (netState) {
    case NET_OK:
      digitalWrite(LED_PIN, HIGH);
      break;
    case NET_NO_INTERNET:
      {
        const unsigned long intervalo = 500;
        if (now - lastLedToggle >= intervalo) {
          lastLedToggle = now;
          ledOn = !ledOn;
          digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
        }
        break;
      }
    case NET_NO_WIFI:
      {
        const unsigned long intervalo = 150;
        if (now - lastLedToggle >= intervalo) {
          lastLedToggle = now;
          ledOn = !ledOn;
          digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
        }
        break;
      }
  }
}

/************ Watchdog init ************/
static void initWatchdog() {
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT_SECONDS * 1000,
    .idle_core_mask = (1 << 0),
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
}

/************ HTTP helpers ************/
static bool httpBeginAuto(HTTPClient& http, const String& url) {
  if (url.startsWith("https://")) {
    static WiFiClientSecure clientSecure;
    clientSecure.setInsecure();  // (dev) si quieres validar TLS, cámbialo
    return http.begin(clientSecure, url);
  }
  return http.begin(url);
}

/************ NVS load/save ************/
static void loadConfigFromNVS() {
  prefs.begin(NVS_NS, true);
  String legacy = prefs.getString(KEY_API_LEGACY, apiLegacyBuf);
  String prod = prefs.getString(KEY_API_PRODUCT, apiProdBuf);
  scannerNamePrefix = prefs.getString(KEY_SCANNER_PREFIX, "EY");
  scannerMode = prefs.getInt(KEY_SCANNER_MODE, 0);
  prefs.end();

  legacy.trim();
  prod.trim();
  scannerNamePrefix.trim();

  strncpy(apiLegacyBuf, legacy.c_str(), sizeof(apiLegacyBuf) - 1);
  apiLegacyBuf[sizeof(apiLegacyBuf) - 1] = '\0';

  strncpy(apiProdBuf, prod.c_str(), sizeof(apiProdBuf) - 1);
  apiProdBuf[sizeof(apiProdBuf) - 1] = '\0';

  apiUrl = String(apiLegacyBuf);
  apiProductUrl = String(apiProdBuf);

  Serial.println("[NVS] apiUrl=" + apiUrl);
  Serial.println("[NVS] apiProductUrl=" + apiProductUrl);
  Serial.println("[NVS] scannerNamePrefix=" + scannerNamePrefix);
  Serial.println("[NVS] scannerMode=" + String(scannerMode));
}

static void saveConfigToNVS() {
  prefs.begin(NVS_NS, false);
  prefs.putString(KEY_API_LEGACY, apiUrl);
  prefs.putString(KEY_API_PRODUCT, apiProductUrl);
  prefs.putString(KEY_SCANNER_PREFIX, scannerNamePrefix);
  prefs.putInt(KEY_SCANNER_MODE, scannerMode);
  prefs.end();
}

static void saveScannerModeOnly() {
  prefs.begin(NVS_NS, false);
  prefs.putInt(KEY_SCANNER_MODE, scannerMode);
  prefs.end();
}

static void applyScannerModeRuntime() {
  LOGF("[CFG] Aplicando modo scanner = %d\n", scannerMode);

  // Desconectar UART si estaba activo
  if (bleClientUart && bleClientUart->isConnected()) {
    bleClientUart->disconnect();
  }

  // Desconectar HID si estaba activo
  if (bleClientHid && bleClientHid->isConnected()) {
    bleClientHid->disconnect();
  }

  scannerUartConnected = false;
  scannerHidConnected = false;

  uiSetStatus("Modo scanner actualizado");
}


static void saveConfigCallback() {
  Serial.println("💾 WiFiManager: configuración guardada.");
  shouldRebootAfterConfig = true;

  if (p_apiLegacy) apiUrl = String(p_apiLegacy->getValue());
  if (p_apiProduct) apiProductUrl = String(p_apiProduct->getValue());

  apiUrl.trim();
  apiProductUrl.trim();
  scannerNamePrefix.trim();

  saveConfigToNVS();
}

/************ Reset WiFi + NVS (por BLE CFG) ************/
static void resetWifiAndEnterConfig() {
  uiSetStatus("Modo configuracion...");
  Serial.println("CFG → reset WiFi + NVS");

  enterPortalMode();

  WiFiManager wm;
  wm.resetSettings();

  prefs.begin(NVS_NS, false);
  prefs.clear();
  prefs.end();

  delay(500);

  wm.setConfigPortalTimeout(0);
  wm.setBreakAfterConfig(true);
  wm.startConfigPortal("iVisor");

  exitPortalMode();

  delay(500);
  ESP.restart();
}

/************ Logs + Registro + Pago ************/
static void sendLog(const String& event) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;

  StaticJsonDocument<256> doc;
  doc["deviceId"] = getDeviceId();
  doc["event"] = event;
  doc["firmware"] = FW_VERSION;

  String body;
  serializeJson(doc, body);

  http.setTimeout(4000);
  if (!httpBeginAuto(http, apiSendLog)) return;
  http.addHeader("Content-Type", "application/json");
  http.POST(body);
  http.end();
}

static void registrarDispositivo() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  StaticJsonDocument<256> doc;
  doc["deviceId"] = getDeviceId();
  doc["firmware"] = FW_VERSION;
  doc["ip"] = WiFi.localIP().toString();

  String body;
  serializeJson(doc, body);

  http.setTimeout(5000);
  if (!httpBeginAuto(http, apiRegistro)) return;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();

  LOGT("Registro dispositivo → ");
  Serial.println(code);

  if (code == 200 || code == 201) deviceRegistered = true;
}

static bool verificarPago() {
  if (WiFi.status() != WL_CONNECTED) return true;  // no bloquees por falta de wifi

  HTTPClient http;

  http.setTimeout(5000);
  if (!httpBeginAuto(http, apiVerificarPago + getDeviceId())) return true;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return true;  // si falla status, no bloquees “por error”
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload)) return true;

  bool pagado = doc["paid"] | true;
  return pagado;
}

/************ Verificar WiFi + auto resetSettings ************/
static void verificarWiFi() {
  if (WiFi.SSID() == "") {
    netState = NET_NO_WIFI;
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiFallidaDesde == 0) wifiFallidaDesde = millis();
    netState = NET_NO_WIFI;

    WiFi.reconnect();
    delay(30);

    if (WiFi.status() == WL_CONNECTED) {
      wifiFallidaDesde = 0;
      return;
    }

    if (millis() - wifiFallidaDesde > TIEMPO_MAX_SIN_WIFI) {
      LOGLN("⛔ WiFi perdido mucho tiempo. Reset settings y restart...");
      WiFiManager wm;
      wm.resetSettings();

      prefs.begin(NVS_NS, false);
      prefs.clear();
      prefs.end();

      delay(1500);
      ESP.restart();
    }
  } else {
    wifiFallidaDesde = 0;
  }
}

/************ OTA Local por navegador ************/

static void handleDoUpdate() {
  HTTPUpload& upload = otaServer.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.println("🔄 Inicio de subida OTA local...");
    Update.begin();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.println("✅ OTA local correcta, reiniciando...");
    else Serial.println("❌ OTA local falló");
  }

  otaServer.sendHeader("Connection", "close");
  otaServer.send(200, "text/plain", "OK, reiniciando...");
  delay(800);
  ESP.restart();
}

static void handleRoot() {
  otaServer.send(200, "text/html",
                 "<h1>Visor Precios</h1>"
                 "<p>Firmware actual: " FW_VERSION "</p>"
                 "<p><a href=\"/update\">Actualizar firmware</a></p>");
}

static void handleUpdate() {
  otaServer.send(200, "text/html",
                 "<h2>OTA Local</h2>"
                 "<form method='POST' action='/doUpdate' enctype='multipart/form-data'>"
                 "<input type='file' name='firmware'>"
                 "<input type='submit' value='Actualizar'>"
                 "</form>");
}

// ✅ SOLO para escribir el bin (NO responder aquí)
static void handleDoUpdateUpload() {
  HTTPUpload& upload = otaServer.upload();

  if (upload.status == UPLOAD_FILE_START) {
    LOGF("🔄 OTA: start '%s'\n", upload.filename.c_str());
    esp_task_wdt_reset();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    esp_task_wdt_reset();
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    esp_task_wdt_reset();
    if (Update.end(true)) {
      LOGF("✅ OTA: success, %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    esp_task_wdt_reset();
    Update.end();
    Serial.println("❌ OTA: aborted");
  }
}

// ✅ Responder UNA sola vez al finalizar el POST
static void handleDoUpdateFinish() {
  bool ok = !Update.hasError();
  otaServer.sendHeader("Connection", "close");
  otaServer.send(200, "text/plain", ok ? "OK, reiniciando..." : "FAIL, revisa Serial");

  delay(800);
  if (ok) ESP.restart();
}

static void setupOtaHttpServer() {
  otaServer.on("/", HTTP_GET, handleRoot);
  otaServer.on("/update", HTTP_GET, handleUpdate);

  // (Opcional) si entras por navegador en GET /doUpdate
  otaServer.on("/doUpdate", HTTP_GET, []() {
    otaServer.sendHeader("Location", "/update");
    otaServer.send(302, "text/plain", "");
  });

  // ✅ aquí el finish handler + upload handler separado
  otaServer.on("/doUpdate", HTTP_POST, handleDoUpdateFinish, handleDoUpdateUpload);

  otaServer.begin();
  Serial.println("🌐 Servidor OTA HTTP iniciado en puerto 80.");
}


/************ OTA Remota ************/
static int semverCmp(const String& a, const String& b) {
  int a1 = 0, a2 = 0, a3 = 0, b1 = 0, b2 = 0, b3 = 0;
  sscanf(a.c_str(), "%d.%d.%d", &a1, &a2, &a3);
  sscanf(b.c_str(), "%d.%d.%d", &b1, &b2, &b3);
  if (a1 != b1) return (a1 < b1) ? -1 : 1;
  if (a2 != b2) return (a2 < b2) ? -1 : 1;
  if (a3 != b3) return (a3 < b3) ? -1 : 1;
  return 0;
}

static bool isNewerVersion(const String& remote, const String& local) {
  if (remote.length() == 0 || local.length() == 0) return false;
  return semverCmp(remote, local) > 0;
}

static void debugHeadGet(const String& url) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20000);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // clave
  http.setTimeout(20000);

  Serial.println("[DBG] GET " + url);
  if (!http.begin(client, url)) {
    Serial.println("[DBG] http.begin failed");
    return;
  }

  int code = http.GET();
  LOGF("[DBG] code=%d size=%d\n", code, http.getSize());

  // Importante: ver si hay Location (redirect)
  String loc = http.getLocation();
  if (loc.length()) Serial.println("[DBG] Location: " + loc);

  http.end();
}

static void pauseTimersForOta(bool pause) {
  if (!usarLVGL) return;
  lvgl_port_lock(-1);

  if (timer_api) lv_timer_pause(timer_api);
  if (timer_wifi) lv_timer_pause(timer_wifi);
  if (timer_ble_adv) lv_timer_pause(timer_ble_adv);
  if (timer_ui) lv_timer_pause(timer_ui);

  // si NO quieres pausar LED / watchdog visual, no pauses timer_led
  // if (timer_led) lv_timer_pause(timer_led);

  if (!pause) {
    if (timer_api) lv_timer_resume(timer_api);
    if (timer_wifi) lv_timer_resume(timer_wifi);
    if (timer_ble_adv) lv_timer_resume(timer_ble_adv);
    if (timer_ui) lv_timer_resume(timer_ui);
    // if (timer_led) lv_timer_resume(timer_led);
  }

  lvgl_port_unlock();
}


static bool testHttpsRaw(const char* host, const char* path) {
  LOGF("\n[TEST] host=%s path=%s\n", host, path);

  // 1) DNS
  IPAddress ip;
  if (!WiFi.hostByName(host, ip)) {
    Serial.println("[TEST] ❌ DNS hostByName failed");
    return false;
  }
  LOGT("[TEST] DNS OK -> ");
  Serial.println(ip);

  // 2) TCP + TLS
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20000);

  Serial.println("[TEST] Connecting 443...");
  if (!client.connect(host, 443)) {
    Serial.println("[TEST] ❌ connect(443) failed");

    char errbuf[120];
    client.lastError(errbuf, sizeof(errbuf));
    LOGT("[TEST] TLS lastError: ");
    Serial.println(errbuf);

    return false;
  }
  Serial.println("[TEST] ✅ connected");

  // 3) GET manual
  client.printf(
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "User-Agent: ESP32\r\n"
    "Connection: close\r\n\r\n",
    path, host);

  // lee status line
  String status = client.readStringUntil('\n');
  LOGT("[TEST] status line: ");
  Serial.println(status);

  return status.startsWith("HTTP/1.1 200");
}

static void scheduleOtaAndReboot(const String& fwUrl, const String& remoteVersion) {
  prefs.begin(NVS_NS, false);
  prefs.putBool(KEY_OTA_PENDING, true);
  prefs.putString(KEY_OTA_URL, fwUrl);
  prefs.putString(KEY_OTA_VER, remoteVersion);  // opcional
  prefs.end();

  Serial.println("[OTA] Scheduled. Rebooting into OTA mode...");
  delay(200);
  ESP.restart();
}

static bool doHttpUpdate2(const String& fwUrl) {
  WiFiClient client;
  HTTPUpdate httpUpdate;
  Serial.println("🔄 Iniciando OTA desde: " + fwUrl);
  t_httpUpdate_return ret = httpUpdate.update(client, fwUrl);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      esp_task_wdt_reset();
      LOGF("❌ OTA falló. Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      sendLog(String("OTA_FAIL ") + httpUpdate.getLastErrorString().c_str());
      return false;
    case HTTP_UPDATE_NO_UPDATES:
      esp_task_wdt_reset();
      Serial.println("ℹ️ No hay actualización disponible.");
      return false;
    case HTTP_UPDATE_OK:
      esp_task_wdt_reset();
      Serial.println("✅ OTA OK, reiniciando...");
      return true;
  }
  return false;
}

static bool runOtaBootModeIfPending() {
  // 1) Lee flags
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  const bool pending = prefs.getBool(KEY_OTA_PENDING, false);
  String fwUrl = prefs.getString(KEY_OTA_URL, "");
  prefs.end();

  if (!pending || fwUrl.isEmpty()) return false;

  Serial.println("[OTA-BOOT] Pending OTA detected");
  Serial.println("[OTA-BOOT] URL: " + fwUrl);

  // 2) WiFi mínimo
  WiFi.mode(WIFI_STA);
  WiFi.begin();  // usa credenciales guardadas

  const unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OTA-BOOT] WiFi not connected. Keeping pending and rebooting...");
    delay(1500);
    ESP.restart();
  }

  // 3) Fuerza HTTPS (recomendado)
  //    Si te llega http://, conviértelo a https:// para evitar 308
  if (fwUrl.startsWith("http://")) {
    fwUrl.replace("http://", "https://");
    Serial.println("[OTA-BOOT] URL rewritten to HTTPS: " + fwUrl);
  }

  // 4) HTTP GET del BIN
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  const char* hdrKeys[] = { "Location", "Content-Length", "Content-Type" };
  http.collectHeaders(hdrKeys, 3);

  WiFiClientSecure client;
  client.setInsecure();  // OK para Vercel (si quieres pinning luego lo hacemos)
  client.setTimeout(20000);

  if (!http.begin(client, fwUrl)) {
    Serial.println("❌ [OTA-BOOT] http.begin() failed");
    Serial.println("❌ [OTA-BOOT] OTA failed. Rebooting (pending kept).");
    delay(1500);
    ESP.restart();
    return true;
  }

  Serial.println("🔄 [OTA-BOOT] GET: " + fwUrl);
  int code = http.GET();
  LOGF("[OTA-BOOT] HTTP code=%d\n", code);

  if (code != HTTP_CODE_OK) {
    LOGF("[OTA-BOOT] Location=%s\n", http.header("Location").c_str());
    http.end();
    Serial.println("❌ [OTA-BOOT] OTA failed. Rebooting (pending kept).");
    delay(1500);
    ESP.restart();
    return true;
  }

  // 5) Validaciones fuertes
  int totalLen = http.getSize();
  LOGF("[OTA-BOOT] Content-Length=%d\n", totalLen);

  if (totalLen <= 0) {
    Serial.println("❌ [OTA-BOOT] Invalid Content-Length. Aborting OTA.");
    http.end();
    Serial.println("❌ [OTA-BOOT] OTA failed. Rebooting (pending kept).");
    delay(1500);
    ESP.restart();
    return true;
  }

  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    Serial.println("❌ [OTA-BOOT] Stream null.");
    http.end();
    Serial.println("❌ [OTA-BOOT] OTA failed. Rebooting (pending kept).");
    delay(1500);
    ESP.restart();
    return true;
  }

  // 6) Escribe firmware
  if (!Update.begin((size_t)totalLen)) {
    Serial.println("❌ [OTA-BOOT] Update.begin failed");
    Update.printError(Serial);
    http.end();
    Serial.println("❌ [OTA-BOOT] OTA failed. Rebooting (pending kept).");
    delay(1500);
    ESP.restart();
    return true;
  }

  size_t written = Update.writeStream(*stream);
  LOGF("[OTA-BOOT] Written=%u\n", (unsigned)written);

  if (written != (size_t)totalLen) {
    Serial.println("❌ [OTA-BOOT] Incomplete write");
    Update.abort();
    http.end();
    Serial.println("❌ [OTA-BOOT] OTA failed. Rebooting (pending kept).");
    delay(1500);
    ESP.restart();
    return true;
  }

  if (!Update.end(true)) {
    Serial.println("❌ [OTA-BOOT] Update.end failed");
    Update.printError(Serial);
    http.end();
    Serial.println("❌ [OTA-BOOT] OTA failed. Rebooting (pending kept).");
    delay(1500);
    ESP.restart();
    return true;
  }

  http.end();

  // 7) Limpia flags y reinicia a firmware nuevo
  Serial.println("✅ [OTA-BOOT] OTA OK. Clearing flags and restarting...");
  prefs.begin(NVS_NS, false);
  prefs.putBool(KEY_OTA_PENDING, false);
  prefs.remove(KEY_OTA_URL);
  prefs.remove(KEY_OTA_VER);
  prefs.end();

  delay(500);
  ESP.restart();
  return true;
}

static void checkForFirmwareUpdate(bool manualTrigger) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(5000);
  if (!httpBeginAuto(http, getFwInfoUrl())) return;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload)) return;

  String remoteVersion = doc["version"] | "";
  bool force = doc["force"] | false;
  String fwUrl = doc["url"] | "";

  if (remoteVersion == "" || fwUrl == "") return;

  bool shouldUpdate = false;
  if (isNewerVersion(remoteVersion, FW_VERSION)) shouldUpdate = true;
  else if (manualTrigger && force) shouldUpdate = true;

  if (shouldUpdate) {
    Serial.println("🛑 Pausando timers para OTA...");
    pauseTimersForOta(true);
    uiSetStatus("Actualizando firmware... NO APAGAR");

    delay(200);  // pequeño margen visual

    scheduleOtaAndReboot(fwUrl, remoteVersion);
  }
}

/************ USD rate (API legacy) ************/
static double parseUsdRateFromJson(const String& payload) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload)) return 0.0;
  return doc["usd"].as<double>();
}

static bool fetchUsdRate() {

  LOGLN(WiFi.status());


  if (WiFi.status() != WL_CONNECTED) return false;

  String url = apiUrl;
  url.trim();

  LOGLN(url);

  if (url.length() == 0) return false;

  HTTPClient http;
  http.setTimeout(4000);
  if (!httpBeginAuto(http, url)) {
    LOGLN("httpBeginAuto");
    return false;
  }

  int code = http.GET();
  Serial.println(code);
  if (code != 200) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();
  Serial.println(payload);
  double r = parseUsdRateFromJson(payload);
  if (r <= 0.000001) return false;

  usdRate = r;
  LOGF("[USD] Rate updated: %.6f\n", usdRate);
  return true;
}

/************ HTTP producto ************/
struct HttpTaskParam {
  String code;
};


static void skipToJsonStart(Stream& s) {
  // 1) Saltar BOM UTF-8 si aparece (EF BB BF)
  int p = s.peek();
  if (p == 0xEF) {
    uint8_t bom[3];
    size_t n = s.readBytes((char*)bom, 3);
    if (!(n == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF)) {
      // Si no era BOM real, ya se consumieron bytes; normalmente esto no pasa
      // si tu server no usa 0xEF al inicio.
    }
  }

  // 2) Saltar cualquier cosa hasta '{' o '['
  while (true) {
    int c = s.peek();
    if (c < 0) return;                 // EOF
    if (c == '{' || c == '[') return;  // inicio JSON
    s.read();                          // descarta
  }
}

static bool consultarProductoPorCodigo(
  const String& code,
  double& outPriceWithTax,
  double& outPriceBase,
  double& outTaxPercent,
  double& outTaxAmount,
  char* outName,
  size_t outNameLen,
  int& outHttpCode) {
  outHttpCode = -999;
  outPriceWithTax = 0.0;
  outPriceBase = 0.0;
  outTaxPercent = 0.0;
  outTaxAmount = 0.0;

  if (WiFi.status() != WL_CONNECTED) {
    uiSetStatus("WiFi desconectado");
    return false;
  }

  char url[200];
  snprintf(url, sizeof(url), "%s%s%s",
           apiProductUrl.c_str(),
           (apiProductUrl.indexOf('?') >= 0) ? "&code=" : "?code=",
           code.c_str());

  LOGLN(url);

  HTTPClient http;
  http.setTimeout(10000);

  if (!httpBeginAuto(http, url)) {
    uiSetStatus("HTTP begin() fallo");
    return false;
  }

  int httpCode = http.GET();
  outHttpCode = httpCode;

  if (httpCode != 200) {
    LOGLN(httpCode);
    LOGLN(http.errorToString(httpCode));
    http.end();
    return false;
  }

  WiFiClient* raw = http.getStreamPtr();
  if (!raw) {
    LOGLN("Error: stream NULL\n");
    http.end();
    return false;
  }

  unsigned long t0 = millis();
  while (!raw->available() && (millis() - t0) < 250) {
    delay(1);
  }

  ReadBufferingStream stream(*raw, 256);
  skipToJsonStart(stream);

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, stream);

  http.end();

  if (err) {
    LOGF("JSON Error: %s\n", err.c_str());
    return false;
  }

  bool ok = doc["ok"] | false;
  bool found = doc["found"] | false;
  if (!ok || !found) return false;

  const char* name = doc["name"] | "";
  strlcpy(outName, name, outNameLen);

  outPriceBase = doc["priceBase"] | 0.0;
  outTaxPercent = doc["taxPercent"] | 0.0;
  outPriceWithTax = doc["priceWithTax"] | 0.0;

  // Si la API trae taxAmount o iva, lo usamos.
  // Si no, lo calculamos con priceWithTax - priceBase.
  if (!doc["taxAmount"].isNull()) {
    outTaxAmount = doc["taxAmount"].as<double>();
  } else if (!doc["iva"].isNull()) {
    outTaxAmount = doc["iva"].as<double>();
  } else {
    outTaxAmount = outPriceWithTax - outPriceBase;
    if (outTaxAmount < 0) outTaxAmount = 0;
  }

  return true;
}

static void httpWorkerTask(void* pv) {
  (void)pv;
  HttpJob job;

  for (;;) {
    if (xQueueReceive(httpQueue, &job, portMAX_DELAY) != pdTRUE) continue;

    String code = String(job.code);
    code.trim();
    if (!code.length()) continue;

    if (!servicePaid) {
      uiSetStatus("Servicio suspendido");
      uiSetDesc("SUSPENDIDO");
      uiSetPrice("--.--");
      uiSetPriceUsd("--.--");
      uiSetRateUsd("--.--");
      uiSetIva("IVA: --.-- Bs.");
      scheduleClearIn5s();
      continue;
    }

    // UI input
    curCode = code;
    if (job.setTextArea) uiSetBarcode(curCode);
    uiSetStatus(String(job.source));
    uiSetStatus("Consultando...");

    int httpCode;
    double priceWithTax = 0.0;
    double priceBase = 0.0;
    double taxPercent = 0.0;
    double taxAmount = 0.0;
    char name[80];

    bool okReq = consultarProductoPorCodigo(
      code,
      priceWithTax,
      priceBase,
      taxPercent,
      taxAmount,
      name,
      sizeof(name),
      httpCode);

    if (!okReq) {
      uiSetStatus("No encontrado / Error");
      uiSetPrice("0.00");
      uiSetPriceUsd("0.00");
      uiSetIva("IVA: 0.00 Bs.");
      scheduleClearIn5s();
      continue;
    }

    uiSetDesc(name);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", priceWithTax);
    uiSetPrice(String(buf));

    // IVA real traído/calculado desde API
    char bufIva[32];
    snprintf(bufIva, sizeof(bufIva), "%.2f", taxAmount);

    char ivaText[64];
    if (taxPercent > 0.0) {
      snprintf(ivaText, sizeof(ivaText), "IVA (%.0f%%): %s Bs.", taxPercent, bufIva);
    } else {
      snprintf(ivaText, sizeof(ivaText), "IVA: %s Bs.", bufIva);
    }
    uiSetIva(String(ivaText));

    if (usdRate > 0.000001) {
      double usd = priceWithTax / usdRate;
      char bufUsd[32];
      snprintf(bufUsd, sizeof(bufUsd), "%.2f", usd);
      uiSetPriceUsd(String(bufUsd));

      char bufRate[32];
      snprintf(bufRate, sizeof(bufRate), "%.2f", usdRate);
      uiSetRateUsd(String(bufRate));
    } else {
      uiSetPriceUsd("--.--");
      uiSetRateUsd("--.--");
    }

    uiSetStatus("OK");
    scheduleClearIn5s();
    vTaskDelay(1);
  }
}

static bool shouldSendCodeOnce(const String& code) {
  String c = code;
  c.trim();
  if (c.length() == 0) return false;

  uint32_t now = millis();

  // Mismo código dentro de la ventana => ignorar
  if (c == g_lastSentCode && (now - g_lastSentMs) < SEND_ONCE_WINDOW_MS) {
    return false;
  }

  g_lastSentCode = c;
  g_lastSentMs = now;
  return true;
}

static void launchHttpForCodeEx(const String& code, const String& sourceLabel, bool setTextArea) {
  String c = code;
  c.trim();
  if (c.length() == 0) return;

  if (!servicePaid) {
    uiSetStatus("Servicio suspendido");
    return;
  }

  if (!shouldSendCodeOnce(c)) {
    LOGLN("Code DUP!");
    return;
  }

  if (!httpQueue) {
    uiSetStatus("HTTP worker no listo");
    return;
  }



  HttpJob job{};
  strncpy(job.code, c.c_str(), sizeof(job.code) - 1);
  strncpy(job.source, sourceLabel.c_str(), sizeof(job.source) - 1);
  job.setTextArea = setTextArea;

  if (xQueueSend(httpQueue, &job, 0) != pdTRUE) {
    uiSetStatus("Cola ocupada (intente de nuevo)");
  }
}

static void launchHttpForCode(const String& code, const String& sourceLabel) {
  launchHttpForCodeEx(code, sourceLabel, true);
}

/************ BLE Server (RX para app) ************/
static String sanitizeCode(const String& in) {
  String s = in;
  s.trim();
  s.replace("\r", "");
  s.replace("\n", "");
  return s;
}

static void handleBleWrite(NimBLECharacteristic* pCharacteristic) {
  std::string v = pCharacteristic->getValue();
  if (v.empty()) return;

  String incoming = sanitizeCode(String(v.c_str()));
  Serial.println("[BLE APP RX] " + incoming);

  if (incoming.equalsIgnoreCase("CFG")) {
    resetWifiAndEnterConfig();
    return;
  }

  launchHttpForCode(incoming, "BLE App");
}

class RXCallbacks : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    handleBleWrite(pCharacteristic);
  }
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    (void)connInfo;
    handleBleWrite(pCharacteristic);
  }
};

/************ BLE UART notify callback ************/
static void scannerUartNotifyCB(NimBLERemoteCharacteristic* pRC, uint8_t* data, size_t len, bool isNotify) {
  (void)pRC;
  (void)isNotify;

  for (size_t i = 0; i < len; i++) {
    char c = (char)data[i];
    if (c == '\r' || c == '\n') {
      if (centralUartRxBuffer.length() > 0) {
        String code = centralUartRxBuffer;
        centralUartRxBuffer = "";
        code.trim();
        if (code.length()) {
          LOGLN("[SCANNER UART] CODE=" + code);
          launchHttpForCode(code, "Scanner BLE UART");
        }
      }
    } else {
      centralUartRxBuffer += c;
      if (centralUartRxBuffer.length() > 128) centralUartRxBuffer.remove(0, centralUartRxBuffer.length() - 128);
    }
  }
}

static bool connectToScannerUart(const NimBLEAdvertisedDevice& adv) {
  LOGLN("[SCANNER UART] Connecting...");

  if (bleClientUart == nullptr) {
    bleClientUart = NimBLEDevice::createClient();
    bleClientUart->setConnectionParams(12, 12, 0, 51);
    bleClientUart->setConnectTimeout(5);
  }

  if (!bleClientUart->connect((NimBLEAdvertisedDevice*)&adv)) {
    LOGLN("[SCANNER UART] Connect failed");
    return false;
  }

  NimBLERemoteService* svc = bleClientUart->getService(UART_SERVICE_UUID);
  if (!svc) {
    LOGLN("[SCANNER UART] NUS service not found");
    bleClientUart->disconnect();
    return false;
  }

  uartNotify = svc->getCharacteristic(UART_NOTIFY_UUID);
  if (!uartNotify || !uartNotify->canNotify()) {
    LOGLN("[SCANNER UART] Notify char invalid");
    bleClientUart->disconnect();
    return false;
  }

  bool subOk = uartNotify->subscribe(true, scannerUartNotifyCB, true);
  if (!subOk) {
    LOGLN("[SCANNER UART] Subscribe failed");
    bleClientUart->disconnect();
    return false;
  }

  scannerUartConnected = true;
  uiSetStatus("Scanner UART conectado");
  LOGLN("[SCANNER UART] Connected OK");
  return true;
}

/************ BLE HID keyboard: keymap básico (US) ************/
static char hidKeycodeToAscii(uint8_t keycode, bool shift) {
  if (keycode >= 0x04 && keycode <= 0x1D) {
    char c = 'a' + (keycode - 0x04);
    return shift ? (char)toupper(c) : c;
  }
  if (keycode >= 0x1E && keycode <= 0x27) {
    const char noShift[] = "1234567890";
    const char withShift[] = "!@#$%^&*()";
    return shift ? withShift[keycode - 0x1E] : noShift[keycode - 0x1E];
  }

  switch (keycode) {
    case 0x28: return '\n';  // Enter
    case 0x2C: return ' ';   // Space
    case 0x2D: return shift ? '_' : '-';
    case 0x2E: return shift ? '+' : '=';
    case 0x2F: return shift ? '{' : '[';
    case 0x30: return shift ? '}' : ']';
    case 0x31: return shift ? '|' : '\\';
    case 0x33: return shift ? ':' : ';';
    case 0x34: return shift ? '"' : '\'';
    case 0x35: return shift ? '~' : '`';
    case 0x36: return shift ? '<' : ',';
    case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
    default: return 0;
  }
}

static void processHidKeyboardReport(const uint8_t* data, size_t len) {
  if (len < 8) return;

  // Evitar duplicados (key hold)
  if (lastHidReportLen == len && memcmp(lastHidReport, data, len) == 0) return;
  lastHidReportLen = min(len, sizeof(lastHidReport));
  memcpy(lastHidReport, data, lastHidReportLen);

  uint8_t modifier = data[0];
  bool shift = (modifier & 0x02) || (modifier & 0x20);  // LSHIFT o RSHIFT

  // Primer keycode no-cero
  uint8_t keycode = 0;
  for (int i = 2; i <= 7; i++) {
    if (data[i] != 0) {
      keycode = data[i];
      break;
    }
  }
  if (keycode == 0) return;

  // Backspace
  if (keycode == 0x2A) {
    if (centralHidRxBuffer.length() > 0) centralHidRxBuffer.remove(centralHidRxBuffer.length() - 1);
    return;
  }

  char c = hidKeycodeToAscii(keycode, shift);
  if (c == 0) return;

  if (c == '\n') {
    String code = centralHidRxBuffer;
    centralHidRxBuffer = "";
    code.trim();
    if (code.length()) {
      LOGLN("[SCANNER HID] CODE=" + code);
      launchHttpForCode(code, "Scanner BLE HID");
    }
    return;
  }

  centralHidRxBuffer += c;
  if (centralHidRxBuffer.length() > 128) centralHidRxBuffer.remove(0, centralHidRxBuffer.length() - 128);
}

static void hidNotifyCB(NimBLERemoteCharacteristic* pRC, uint8_t* data, size_t len, bool isNotify) {
  (void)pRC;
  (void)isNotify;
  processHidKeyboardReport(data, len);
}

static bool connectToScannerHid(const NimBLEAdvertisedDevice& adv) {
  LOGLN("[SCANNER HID] Connecting...");

  if (bleClientHid == nullptr) {
    bleClientHid = NimBLEDevice::createClient();
    bleClientHid->setConnectionParams(12, 12, 0, 51);
    bleClientHid->setConnectTimeout(5);
  }

  if (!bleClientHid->connect((NimBLEAdvertisedDevice*)&adv)) {
    LOGLN("[SCANNER HID] Connect failed");
    return false;
  }

  NimBLERemoteService* svc = bleClientHid->getService(NimBLEUUID((uint16_t)0x1812));
  if (!svc) svc = bleClientHid->getService(HID_SERVICE_UUID);

  if (!svc) {
    LOGLN("[SCANNER HID] HID service not found");
    bleClientHid->disconnect();
    return false;
  }

  hidReportChar = svc->getCharacteristic(HID_REPORT_UUID);
  if (!hidReportChar) {
    LOGLN("[SCANNER HID] Report char not found");
    bleClientHid->disconnect();
    return false;
  }
  if (!hidReportChar->canNotify() && !hidReportChar->canIndicate()) {
    LOGLN("[SCANNER HID] Report char no notify/indicate");
    bleClientHid->disconnect();
    return false;
  }

  bool subOk = hidReportChar->subscribe(true, hidNotifyCB, true);
  if (!subOk) {
    LOGLN("[SCANNER HID] Subscribe failed");
    bleClientHid->disconnect();
    return false;
  }

  scannerHidConnected = true;
  centralHidRxBuffer = "";
  lastHidReportLen = 0;
  uiSetStatus("Scanner HID conectado");
  LOGLN("[SCANNER HID] Connected OK");
  return true;
}

/************ Scanner Central Task (AUTO/UART/HID) ************/
static bool advHasService(const NimBLEAdvertisedDevice* adv, const NimBLEUUID& uuid) {
  if (!adv) return false;
  return adv->isAdvertisingService(uuid);
}

static bool matchesPrefix(const NimBLEAdvertisedDevice* adv) {
  if (!adv) return false;
  String name = adv->getName().c_str();
  if (name.length() == 0) return false;
  return name.startsWith(scannerNamePrefix);
}

static void disconnectUartIfAny() {
  scannerUartConnected = false;
  uartNotify = nullptr;
  if (bleClientUart && bleClientUart->isConnected()) bleClientUart->disconnect();
}

static void disconnectHidIfAny() {
  scannerHidConnected = false;
  hidReportChar = nullptr;
  if (bleClientHid && bleClientHid->isConnected()) bleClientHid->disconnect();
}

static void printDevice(const NimBLEAdvertisedDevice* adv) {
  String name = adv->getName().c_str();
  String addr = adv->getAddress().toString().c_str();
  int rssi = adv->getRSSI();

  LOGF("DEV addr=%s rssi=%d name='%s' ",
       addr.c_str(), rssi, name.c_str());

  if (adv->haveServiceUUID()) {
    LOGT(" uuids=");
    for (int i = 0; i < adv->getServiceUUIDCount(); i++) {
      NimBLEUUID u = adv->getServiceUUID(i);
      LOGT(u.toString().c_str());
      if (i < adv->getServiceUUIDCount() - 1) LOGT(",");
    }
  }
  Serial.println();
}

static void scannerCentralTask(void* pv) {
  (void)pv;

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(15);

  for (;;) {
    if (scannerUartConnected && bleClientUart && bleClientUart->isConnected()) {
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    if (scannerHidConnected && bleClientHid && bleClientHid->isConnected()) {
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    if (scannerUartConnected) {
      disconnectUartIfAny();
      uiSetStatus("Scanner UART desconectado");
    }
    if (scannerHidConnected) {
      disconnectHidIfAny();
      uiSetStatus("Scanner HID desconectado");
    }

    uiSetStatus("Status: OK...");

    bool okScan = scan->start(3, false);
    if (!okScan) {
      vTaskDelay(pdMS_TO_TICKS(1500));
      continue;
    }

    NimBLEScanResults results = scan->getResults();
    int count = results.getCount();
    LOGLN("[SCANNER] Scanning... mode=" + String(scannerMode) + " prefix=" + scannerNamePrefix + " Count=" + count);
    bool connectedNow = false;

    // Pasada 1: UART
    if (scannerMode == 0 || scannerMode == 1) {
      for (int i = 0; i < count; i++) {
        const NimBLEAdvertisedDevice* adv = results.getDevice(i);
        if (!adv) {
          continue;
        } else printDevice(adv);


        if (matchesPrefix(adv) || advHasService(adv, UART_SERVICE_UUID)) {
          LOGLN("[SCANNER] Candidate UART: " + String(adv->getName().c_str()));
          connectedNow = connectToScannerUart(*adv);
          if (connectedNow) break;
        }
      }
    }

    // Pasada 2: HID
    if (!connectedNow && (scannerMode == 0 || scannerMode == 2)) {
      for (int i = 0; i < count; i++) {
        const NimBLEAdvertisedDevice* adv = results.getDevice(i);
        if (!adv) {
          continue;
        } else printDevice(adv);

        if (advHasService(adv, HID_SERVICE_UUID) || matchesPrefix(adv)) {
          LOGLN("[SCANNER] Candidate HID: " + String(adv->getName().c_str()));
          connectedNow = connectToScannerHid(*adv);
          if (connectedNow) break;
        }
      }
    }

    scan->clearResults();
    if (!connectedNow) vTaskDelay(pdMS_TO_TICKS(1500));
  }
}

static void startScannerCentral() {
  xTaskCreatePinnedToCore(scannerCentralTask, "scannerCentral", 4096, NULL, 1, NULL, 0);
}

/************ BLE Advertising robustness ************/
static void ensureAdvertising() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (!adv) return;
  if (!adv->isAdvertising()) {
    LOGLN("[BLE] Advertising was OFF -> restarting");
    adv->start();
  }
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    (void)pServer;
    LOGLN("[BLE] App connected");
    uiSetStatus("BLE conectado (App)");
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    (void)pServer;
    (void)connInfo;
    LOGF("[BLE] App disconnected. reason=%d\n", reason);
    // Re-anunciar siempre (iPhone BT toggle a veces deja adv apagado)
    ensureAdvertising();
    uiSetStatus("BLE listo. Buscando scanner...");
  }
};

/************ BLE setup (Server + Central) ************/
static void setupBLE() {
  const char* bleName = "iVisor";

  Serial.println("[BLE] init...");
  NimBLEDevice::init(bleName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  // --- BLE Server ---
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(BLE_SERVICE_UUID);
  chrRX = svc->createCharacteristic(BLE_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  chrRX->setCallbacks(new RXCallbacks());
  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->stop();
  adv->addServiceUUID(BLE_SERVICE_UUID);

  NimBLEAdvertisementData advData;
  advData.setName(bleName);
  advData.setCompleteServices(NimBLEUUID(BLE_SERVICE_UUID));
  adv->setAdvertisementData(advData);

  NimBLEAdvertisementData scanResp;
  scanResp.setName(bleName);
  adv->setScanResponseData(scanResp);

  // Intervalos conservadores para iOS
  adv->setMinInterval(0x20);
  adv->setMaxInterval(0x40);

  Serial.println("[BLE] starting advertising...");
  adv->start();  // indefinido

  uiSetStatus("BLE listo. Buscando scanner...");
  startScannerCentral();
}

/************ Teclado en pantalla (anti-parpadeo) ************/
static void hideKeyboard() {
  if (!kb || !kb_bg) return;
  lv_obj_add_flag(kb_bg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  kbVisible = false;
}

static void showKeyboard() {
  if (!kb || !kb_bg || !taBarcode) return;
  lv_obj_clear_flag(kb_bg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  kbVisible = true;

  lv_keyboard_set_textarea(kb, taBarcode);
  lv_obj_move_foreground(kb_bg);
  lv_obj_move_foreground(kb);
}

static void kb_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char* txt = lv_textarea_get_text(taBarcode);
    String manual = String(txt);
    manual.trim();

    hideKeyboard();

    if (manual.length() > 0) {
      // IMPORTANT: para evitar parpadeo, NO volvemos a setear el textarea aquí
      launchHttpForCodeEx(manual, "Manual", false);
    }
  } else if (code == LV_EVENT_CANCEL) {
    hideKeyboard();
  }
}

static void ta_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED) return;
  showKeyboard();
}

/************ Timer callbacks ************/
static void timer_led_cb(lv_timer_t* t) {
  (void)t;
  actualizarLed();
}

static void timer_wifi_cb(lv_timer_t* t) {
  (void)t;
  verificarWiFi();

  bool nowConnected = (WiFi.status() == WL_CONNECTED);
  if (nowConnected && !lastWifiWasConnected) {
    // Acaba de reconectar WiFi
    Serial.println("[WIFI] Reconnected");
    netState = NET_OK;
    deviceRegistered = false;  // forzar re-registro
  }
  lastWifiWasConnected = nowConnected;
}

static void timer_fw_cb(lv_timer_t* t) {
  (void)t;
  checkForFirmwareUpdate(true);
}
static void timer_ui_cb(lv_timer_t* t) {
  (void)t;
  uiSetDeviceId();
}

static void timer_ble_adv_cb(lv_timer_t* t) {
  (void)t;
  ensureAdvertising();
}

static void timer_api_cb(lv_timer_t* t) {
  (void)t;

  unsigned long now = millis();

  // refresca tasa USD
  if (now - lastUsdFetch >= USD_FETCH_MS) {
    lastUsdFetch = now;
    LOGLN("Fetching...");
    fetchUsdRate();
  }

  // check pago
  if (WiFi.status() == WL_CONNECTED && (now - lastPaidCheckMs >= PAID_CHECK_MS)) {
    lastPaidCheckMs = now;
    bool paid = verificarPago();
    if (paid != servicePaid) {
      servicePaid = paid;
      uiSetStatus(servicePaid ? "Listo. Escanee..." : "Servicio suspendido");
      if (!servicePaid) {
        // limpia UI si queda mostrando algo
        uiSetDesc("SUSPENDIDO");
        uiSetPrice("--.--");
        uiSetPriceUsd("--.--");
        uiSetRateUsd("--.--");
        uiSetIva("IVA: --.-- Bs.");
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) netState = NET_OK;
}

/************ Auto reboot safety ************/
static void checkAutoReboot() {
  if (!lvglWarmupDone) return;
  if (millis() - bootTime > MAX_UPTIME) {
    Serial.println("⏳ Reinicio preventivo por uptime...");
    delay(800);
    ESP.restart();
  }
}

static void cfgSaveBtn_cb(lv_event_t* e) {
  (void)e;

  // 1) leer selección del dropdown (esto sí es LVGL y es liviano)
  int newMode = (int)lv_dropdown_get_selected(ddScannerMode);

  // 2) guardar valores (NVS) — esto es liviano, OK hacerlo aquí
  scannerMode = newMode;
  saveScannerModeOnly();

  LOGF("[CFG] Guardado scannerMode=%d (apply deferred)\n", scannerMode);

  // 3) marcar apply para el loop() (NO BLE aquí)
  pendingScannerModeValue = newMode;
  pendingApplyScannerMode = true;

  // 4) popup confirmación
  showSavedPopup();
}

static void doFactoryResetNow() {
  Serial.println("\n[RST] Factory reset START");
  g_forcePortal = FORCE_PORTAL_MAGIC;
  // Evita que BLE/USB/WiFi estén haciendo cosas a la vez
  otaServer.stop();
  delay(50);

  // Apaga WiFi de forma agresiva
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_OFF);
  delay(200);

  // Intenta restaurar estado WiFi (borra credenciales del stack)
  esp_err_t e1 = esp_wifi_restore();
  LOGF("[RST] esp_wifi_restore -> %d\n", (int)e1);
  delay(200);

  // Borra TODO el NVS (incluye WiFi + tu Preferences)
  esp_err_t e2 = nvs_flash_erase();
  LOGF("[RST] nvs_flash_erase -> %d\n", (int)e2);
  delay(200);

  esp_err_t e3 = nvs_flash_init();
  LOGF("[RST] nvs_flash_init -> %d\n", (int)e3);
  delay(200);

  Serial.println("[RST] Factory reset DONE -> restart");
  delay(500);

  ESP.restart();
}

static void reset_mbox_event_cb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

  lv_obj_t* btnm = lv_event_get_target(e);                // btnmatrix
  lv_obj_t* mbox = (lv_obj_t*)lv_event_get_user_data(e);  // msgbox real

  uint16_t idx = lv_btnmatrix_get_selected_btn(btnm);
  const char* txt = lv_btnmatrix_get_btn_text(btnm, idx);

  // DEBUG rápido (quita luego)
  LOGF("[MB] idx=%u txt=%s\n", idx, txt ? txt : "NULL");

  if (txt && (strcmp(txt, "SI") == 0 || strcmp(txt, "Sí") == 0 || strcmp(txt, "YES") == 0)) {
    pendingResetCfg = true;  // SOLO bandera
  } else {
    closeConfigAndGoHome();
  }

  lv_obj_del_async(mbox);  // siempre cerrar el diálogo
}

static void showResetConfirmDialog() {
  static const char* btns[] = { "NO", "SI", "" };

  lv_obj_t* mbox = lv_msgbox_create(NULL, "Reset", "Borrar WiFi y reiniciar?", btns, true);
  lv_obj_center(mbox);

  lv_obj_t* btnm = lv_msgbox_get_btns(mbox);
  lv_obj_add_event_cb(btnm, reset_mbox_event_cb, LV_EVENT_VALUE_CHANGED, mbox);
}

static void cfgPassKb_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_READY) {
    const char* typed = lv_textarea_get_text(taCfgPass);

    if (strcmp(typed, CFG_UI_PASSWORD) == 0) {
      lv_textarea_set_text(taCfgPass, "");
      if (lblCfgPassMsg) lv_label_set_text(lblCfgPassMsg, "");
      // entra al config real
      lv_scr_load(screenConfig);
    } else {
      lv_textarea_set_text(taCfgPass, "");
      if (lblCfgPassMsg) lv_label_set_text(lblCfgPassMsg, "Clave incorrecta");
    }
  } else if (code == LV_EVENT_CANCEL) {
    lv_textarea_set_text(taCfgPass, "");
    if (lblCfgPassMsg) lv_label_set_text(lblCfgPassMsg, "");
    lv_scr_load(homeScreen);
  }
}

static void showScannerConfigPasswordGate() {
  if (screenCfgPass) {
    lv_scr_load(screenCfgPass);
    return;
  }

  screenCfgPass = lv_obj_create(NULL);
  lv_obj_clear_flag(screenCfgPass, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screenCfgPass, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(screenCfgPass, LV_OPA_COVER, 0);

  lv_obj_t* title = lv_label_create(screenCfgPass);
  lv_label_set_text(title, "Configuracion");
  lv_obj_set_style_text_font(title, &font_mont_22, 0);
  lv_obj_set_style_text_color(title, lv_color_black(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

  lv_obj_t* sub = lv_label_create(screenCfgPass);
  lv_label_set_text(sub, "Ingrese la clave");
  lv_obj_set_style_text_color(sub, lv_color_hex(0x334155), 0);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 52);

  lblCfgPassMsg = lv_label_create(screenCfgPass);
  lv_label_set_text(lblCfgPassMsg, "");
  lv_obj_set_style_text_color(lblCfgPassMsg, lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_align(lblCfgPassMsg, LV_ALIGN_TOP_MID, 0, 78);

  taCfgPass = lv_textarea_create(screenCfgPass);
  lv_textarea_set_one_line(taCfgPass, true);
  lv_textarea_set_password_mode(taCfgPass, true);
  lv_textarea_set_password_show_time(taCfgPass, 200);
  lv_textarea_set_max_length(taCfgPass, 10);
  lv_textarea_set_placeholder_text(taCfgPass, "****");
  lv_obj_set_width(taCfgPass, 280);
  lv_obj_align(taCfgPass, LV_ALIGN_TOP_MID, 0, 105);

  kbCfgPass = lv_keyboard_create(screenCfgPass);
  lv_keyboard_set_textarea(kbCfgPass, taCfgPass);
  lv_obj_set_height(kbCfgPass, 240);
  lv_obj_align(kbCfgPass, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(kbCfgPass, cfgPassKb_event_cb, LV_EVENT_ALL, NULL);

  lv_scr_load(screenCfgPass);
}


static void showScannerConfigScreen() {
  // Siempre pedir clave antes
  showScannerConfigPasswordGate();

  // Creamos la screenConfig una sola vez (pero NO la cargamos aquí)
  if (screenConfig) return;

  screenConfig = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screenConfig, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(screenConfig, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screenConfig, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* lblTitle = lv_label_create(screenConfig);
  lv_label_set_text(lblTitle, "Panel de Configuracion");
  lv_obj_set_style_text_font(lblTitle, &font_mont_22, 0);
  lv_obj_set_style_text_color(lblTitle, lv_color_black(), 0);
  lv_obj_align(lblTitle, LV_ALIGN_TOP_MID, 0, 18);

  lv_obj_t* lblMode = lv_label_create(screenConfig);
  lv_label_set_text(lblMode, "Scanner:");
  lv_obj_set_style_text_font(lblMode, &font_mont_22, 0);
  lv_obj_set_style_text_color(lblMode, lv_color_hex(0x0F172A), 0);
  lv_obj_align(lblMode, LV_ALIGN_TOP_LEFT, 40, 70);

  lv_obj_t* lblUsbOnly = lv_label_create(screenConfig);
  lv_label_set_text(lblUsbOnly, "USB HID (unico modo)");
  lv_obj_set_style_text_font(lblUsbOnly, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(lblUsbOnly, lv_color_hex(0x334155), 0);
  lv_obj_align(lblUsbOnly, LV_ALIGN_TOP_LEFT, 40, 105);

  // --- Botón Volver ---
  lv_obj_t* btnBack = lv_btn_create(screenConfig);
  lv_obj_set_size(btnBack, 160, 50);
  lv_obj_align(btnBack, LV_ALIGN_BOTTOM_LEFT, 40, -15);
  lv_obj_add_event_cb(
    btnBack, [](lv_event_t*) {
      lv_scr_load(homeScreen);
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t* lblBack = lv_label_create(btnBack);
  lv_label_set_text(lblBack, "Volver");
  lv_obj_center(lblBack);

  // --- Botón Reset Config ---
  lv_obj_t* btnReset = lv_btn_create(screenConfig);
  lv_obj_set_size(btnReset, 220, 50);
  lv_obj_align(btnReset, LV_ALIGN_BOTTOM_RIGHT, -40, -15);
  lv_obj_add_event_cb(
    btnReset, [](lv_event_t*) {
      showResetConfirmDialog();
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t* lblReset = lv_label_create(btnReset);
  lv_label_set_text(lblReset, "Reset Config");
  lv_obj_center(lblReset);

  // --- Info: IP + URLs ---
  lv_obj_t* infoBox = lv_obj_create(screenConfig);
  lv_obj_set_size(infoBox, LV_PCT(90), 240);
  lv_obj_align(infoBox, LV_ALIGN_TOP_MID, 0, 155);
  lv_obj_set_style_bg_color(infoBox, lv_color_hex(0xF1F5F9), 0);
  lv_obj_set_style_bg_opa(infoBox, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(infoBox, 1, 0);
  lv_obj_set_style_border_color(infoBox, lv_color_hex(0xCBD5E1), 0);
  lv_obj_set_style_radius(infoBox, 10, 0);
  lv_obj_set_style_pad_all(infoBox, 12, 0);
  lv_obj_clear_flag(infoBox, LV_OBJ_FLAG_SCROLLABLE);

  lblIpCfg = lv_label_create(infoBox);
  lv_obj_set_width(lblIpCfg, LV_PCT(100));
  lv_label_set_long_mode(lblIpCfg, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(lblIpCfg, lv_color_hex(0x0F172A), 0);
  lv_obj_set_style_text_font(lblIpCfg, &lv_font_montserrat_18, 0);
  lv_obj_align(lblIpCfg, LV_ALIGN_TOP_LEFT, 0, 0);

  lblApiLegacyCfg = lv_label_create(infoBox);
  lv_obj_set_width(lblApiLegacyCfg, LV_PCT(100));
  lv_label_set_long_mode(lblApiLegacyCfg, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(lblApiLegacyCfg, lv_color_hex(0x334155), 0);
  lv_obj_set_style_text_font(lblApiLegacyCfg, &lv_font_montserrat_18, 0);
  lv_obj_align(lblApiLegacyCfg, LV_ALIGN_TOP_LEFT, 0, 40);

  lblApiProductCfg = lv_label_create(infoBox);
  lv_obj_set_width(lblApiProductCfg, LV_PCT(100));
  lv_label_set_long_mode(lblApiProductCfg, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(lblApiProductCfg, lv_color_hex(0x334155), 0);
  lv_obj_set_style_text_font(lblApiProductCfg, &lv_font_montserrat_18, 0);
  lv_obj_align(lblApiProductCfg, LV_ALIGN_TOP_LEFT, 0, 125);

  uiUpdateConfigInfo();
}

/************ Popup confirmación (reusable) ************/

// Mostrar popup "Guardado" y al cerrar volver al home
static void showSavedPopupAndCloseConfig() {
  // Si ya existe, destrúyelo para evitar duplicados
  if (msgboxSaved) {
    lv_obj_del(msgboxSaved);
    msgboxSaved = nullptr;
  }

  static const char* btns[] = { "OK", "" };

  msgboxSaved = lv_msgbox_create(NULL, "Listo", "Configuración guardada.", btns, true);
  lv_obj_set_style_text_font(msgboxSaved, &font_mont_22, 0);
  lv_obj_center(msgboxSaved);

  // Cuando se cierre el msgbox (OK o click fuera), borrar y volver al home
  lv_obj_add_event_cb(
    msgboxSaved,
    [](lv_event_t* e) {
      lv_event_code_t code = lv_event_get_code(e);
      if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_DELETE) {
        // VALUE_CHANGED ocurre al presionar botón
        // DELETE por seguridad si lo destruyen desde otro lado
        // Cerrar msgbox de forma segura (sin reentradas raras)
        lv_obj_t* m = (lv_obj_t*)lv_event_get_target(e);
        if (m) lv_obj_del(m);
        msgboxSaved = nullptr;
        closeConfigAndGoHome();
      }
    },
    LV_EVENT_ALL,
    NULL);
}

/************ Popup confirmación (reusable y seguro) ************/

static void enterPortalMode() {
  g_portalMode = true;
  Serial.println("[PORTAL] Enter portal mode");

  // Pausa timers no esenciales
  if (timer_api) lv_timer_pause(timer_api);
  if (timer_fw) lv_timer_pause(timer_fw);
  if (timer_ble_adv) lv_timer_pause(timer_ble_adv);
  if (timer_ui) lv_timer_pause(timer_ui);

  // Si quieres máxima estabilidad, también pausa timer_wifi:
  // if (timer_wifi) lv_timer_pause(timer_wifi);

  // Detén BLE advertising
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (adv && adv->isAdvertising()) adv->stop();

  // Desconecta scanners BLE
  disconnectUartIfAny();
  disconnectHidIfAny();

  uiSetStatus("Modo configuracion WiFi...");
}

static void exitPortalMode() {
  Serial.println("[PORTAL] Exit portal mode");

  // Reanuda timers
  if (timer_api) lv_timer_resume(timer_api);
  if (timer_fw) lv_timer_resume(timer_fw);
  if (timer_ble_adv) lv_timer_resume(timer_ble_adv);
  if (timer_ui) lv_timer_resume(timer_ui);

  // Si pausaste timer_wifi, reanúdalo:
  // if (timer_wifi) lv_timer_resume(timer_wifi);

  // Reanuda BLE advertising
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (adv && !adv->isAdvertising()) adv->start();

  g_portalMode = false;
  uiSetStatus(servicePaid ? "Listo. Escanee..." : "Servicio suspendido");
}

// Cierra pantalla config -> vuelve a tu pantalla principal
static void closeConfigAndGoHome() {
  // OJO: aquí NO hacemos lv_scr_load(lv_scr_act()) (no tiene sentido)
  // Solo cerramos la pantalla actual si era una screen separada.
  // Si tú creaste una screen config con lv_obj_create(NULL) y la cargaste,
  // debes volver a tu "homeScreen". Si no tienes puntero, usa lv_scr_act()
  // como home por default.
  // Recomendado: guarda un puntero global a tu homeScreen.
  // Por ahora: solo cargamos la screen activa inicial (home) si la tienes.
  // Si no tienes homeScreen, comenta esto y solo del configScreen.
  lv_scr_load(homeScreen);
}

// Llama esto desde LVGL callback: solo muestra popup.
// NO hace BLE aquí.
static void showSavedPopup() {
  if (msgboxSaved) {
    lv_obj_del(msgboxSaved);
    msgboxSaved = nullptr;
  }

  static const char* btns[] = { "OK", "" };
  msgboxSaved = lv_msgbox_create(NULL, "Listo", "Configuración guardada.", btns, true);
  lv_obj_set_style_text_font(msgboxSaved, &font_mont_22, 0);
  lv_obj_center(msgboxSaved);

  // Solo al presionar OK cerramos popup (no hacemos BLE aquí)
  lv_obj_add_event_cb(
    msgboxSaved,
    [](lv_event_t* e) {
      if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        // Cerrar msgbox de forma segura (sin reentradas raras)
        lv_obj_t* m = (lv_obj_t*)lv_event_get_target(e);
        if (m) lv_obj_del(m);
        msgboxSaved = nullptr;

        // Si quieres cerrar la screen de config aquí, hazlo liviano:
        closeConfigAndGoHome();
      }
    },
    LV_EVENT_ALL,
    NULL);
}


/************ SETUP ************/
void setup() {
  Serial.begin(115200);
  if (runOtaBootModeIfPending()) return;
  // --------------------
  // Anti-fragmentación (String): reservas para evitar reallocs en runtime
  // --------------------
  apiUrl.reserve(220);
  apiProductUrl.reserve(220);
  scannerNamePrefix.reserve(40);

  curCode.reserve(80);
  curDesc.reserve(180);
  curPrice.reserve(32);

  prevCode.reserve(80);
  prevDesc.reserve(180);
  prevPrice.reserve(32);
  prevStatus.reserve(80);
  prevDevice.reserve(40);
  prevPriceUsd.reserve(32);
  prevRateUsd.reserve(32);

  centralUartRxBuffer.reserve(200);
  centralHidRxBuffer.reserve(200);

  g_barcode.reserve(80);
  g_barcode_last.reserve(80);
  delay(400);

  LOGT("reset reason: ");
  Serial.println((int)esp_reset_reason());

  bootTime = millis();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  gPanelReady = xSemaphoreCreateBinary();

  Serial.println("=====================================");
  Serial.println("🚀 INICIANDO VISOR PRECIOS v" FW_VERSION);
  Serial.println("=====================================");

  /************ LVGL / PANEL ************/
  if (usarLVGL) {
    Serial.println("Inicializando panel LVGL...");
    panelBoard = new Board();
    panelBoard->init();
#if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = panelBoard->getLCD();
    // When avoid tearing function is enabled, the frame buffer number should be set in the board driver
    //lcd->configFrameBufferNumber(1);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    /**
            * As the anti-tearing feature typically consumes more PSRAM bandwidth, for the ESP32-S3, we need to utilize the
            * "bounce buffer" functionality to enhance the RGB data bandwidth.
            * This feature will consume `bounce_buffer_size * bytes_per_pixel * 2` of SRAM memory.
            */
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
      //static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
      //auto rgb_bus = static_cast<esp_panel::drivers::BusRGB*>(lcd_bus);
      //rgb_bus->configRGB_BounceBufferSize(lcd->getFrameWidth() * 20);
      // Si notas líneas saltando, añade esta línea:
      //rgb_bus->configRgbTimingFreqHz(14 * 1000 * 1000);  // Baja a 12MHz para máxima estabilidad
    }
#endif
#endif
    bool okPanel = panelBoard->begin();

    if (!okPanel) {
      Serial.println("❌ Error al iniciar panel. LVGL deshabilitado.");
      usarLVGL = false;
    } else {
      auto io = panelBoard->getIO_Expander();

      if (io) {
        io->getBase()->digitalWrite(5, 0);
        Serial.println("USB_SEL forced to USB mode");
      } else {
        Serial.println("io->getBase()->digitalWrite(5,0) failed");
      }

      lvgl_port_init(panelBoard->getLCD(), panelBoard->getTouch());

      lvgl_port_lock(-1);

      lv_obj_t* scr = lv_scr_act();

      // ===============================
      // WHITE CLEAN THEME (anti-flicker)
      // ===============================
      lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
      lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
      homeScreen = scr;

      // ---------- HEADER ----------
      lv_obj_t* header = lv_obj_create(scr);
      lv_obj_set_size(header, LV_PCT(100), 50);
      lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
      lv_obj_set_style_bg_color(header, lv_color_hex(0x2563EB), 0);
      lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(header, 0, 0);

      lblHeader = lv_label_create(header);
      lv_label_set_text(lblHeader, "Visor de Precios By Innovative Solutions");
      lv_obj_set_style_text_font(lblHeader, &font_mont_22, 0);
      lv_obj_set_style_text_color(lblHeader, lv_color_white(), 0);
      lv_obj_align(lblHeader, LV_ALIGN_CENTER, 0, 0);

      // Botón config
      btnCfg = lv_btn_create(header);
      lv_obj_set_size(btnCfg, 40, 40);
      lv_obj_align(btnCfg, LV_ALIGN_RIGHT_MID, -10, 0);
      lv_obj_set_style_bg_color(btnCfg, lv_color_hex(0x1D4ED8), 0);
      lv_obj_set_style_bg_opa(btnCfg, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(btnCfg, 0, 0);

      lv_obj_add_event_cb(
        btnCfg,
        [](lv_event_t*) {
          showScannerConfigScreen();
        },
        LV_EVENT_CLICKED,
        NULL);

      lv_obj_t* lblCfg = lv_label_create(btnCfg);
      lv_label_set_text(lblCfg, LV_SYMBOL_SETTINGS);
      lv_obj_set_style_text_color(lblCfg, lv_color_white(), 0);
      lv_obj_center(lblCfg);

      // ---------- IDLE BOX ----------
      idleBox = lv_obj_create(scr);
      lv_obj_set_size(idleBox, LV_PCT(92), 80);
      lv_obj_align(idleBox, LV_ALIGN_BOTTOM_MID, 0, -135);

      lv_obj_set_style_bg_color(idleBox, lv_color_hex(0xEFF6FF), 0);
      lv_obj_set_style_bg_opa(idleBox, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(idleBox, lv_color_hex(0xBFDBFE), 0);
      lv_obj_set_style_border_width(idleBox, 2, 0);
      lv_obj_set_style_radius(idleBox, 12, 0);
      lv_obj_set_style_pad_all(idleBox, 12, 0);
      lv_obj_clear_flag(idleBox, LV_OBJ_FLAG_SCROLLABLE);

      lblIdleTitle = lv_label_create(idleBox);
      lv_label_set_text(lblIdleTitle, "Promocion");
      lv_obj_set_style_text_font(lblIdleTitle, &font_mont_22, 0);
      lv_obj_set_style_text_color(lblIdleTitle, lv_color_hex(0x1D4ED8), 0);
      lv_obj_align(lblIdleTitle, LV_ALIGN_TOP_LEFT, 0, 0);

      lblIdleBody = lv_label_create(idleBox);
      lv_label_set_text(lblIdleBody, "Escanee un producto para ver precio y tasa BCV.");
      lv_label_set_long_mode(lblIdleBody, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(lblIdleBody, LV_PCT(100));
      lv_obj_set_style_text_font(lblIdleBody, &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(lblIdleBody, lv_color_hex(0x334155), 0);
      lv_obj_align(lblIdleBody, LV_ALIGN_TOP_LEFT, 0, 35);

      // ---------- CARD PRINCIPAL ----------
      lv_obj_t* card = lv_obj_create(scr);
      lv_obj_set_size(card, LV_PCT(92), 190);
      lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 70);

      lv_obj_set_style_bg_color(card, lv_color_white(), 0);
      lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(card, lv_color_hex(0xE5E7EB), 0);
      lv_obj_set_style_border_width(card, 2, 0);
      lv_obj_set_style_radius(card, 12, 0);
      lv_obj_set_style_pad_all(card, 16, 0);
      lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

      // Código title
      lv_obj_t* lblCodeTitle = lv_label_create(card);
      lv_label_set_text(lblCodeTitle, "Codigo:");
      lv_obj_set_style_text_font(lblCodeTitle, &font_mont_22, 0);
      lv_obj_set_style_text_color(lblCodeTitle, lv_color_hex(0x111827), 0);
      lv_obj_align(lblCodeTitle, LV_ALIGN_TOP_LEFT, 0, 0);

      // TextArea código
      taBarcode = lv_textarea_create(card);
      lv_obj_set_size(taBarcode, LV_PCT(100), 45);
      lv_obj_align(taBarcode, LV_ALIGN_TOP_MID, 0, 35);
      lv_textarea_set_one_line(taBarcode, true);
      lv_textarea_set_text(taBarcode, "");
      lv_textarea_set_cursor_click_pos(taBarcode, false);

      lv_obj_add_flag(taBarcode, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(taBarcode, ta_event_cb, LV_EVENT_CLICKED, NULL);

      lv_textarea_set_accepted_chars(taBarcode, "0123456789");

      lv_obj_set_style_text_font(taBarcode, &lv_font_montserrat_28, 0);
      lv_obj_set_style_bg_color(taBarcode, lv_color_hex(0xF3F4F6), 0);
      lv_obj_set_style_bg_opa(taBarcode, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(taBarcode, lv_color_hex(0xD1D5DB), 0);
      lv_obj_set_style_border_width(taBarcode, 2, 0);
      lv_obj_set_style_radius(taBarcode, 10, 0);
      lv_obj_set_style_text_color(taBarcode, lv_color_hex(0x111827), 0);

      // Descripción title
      lv_obj_t* lblDescTitle = lv_label_create(card);
      lv_label_set_text(lblDescTitle, "Descripcion:");
      lv_obj_set_style_text_font(lblDescTitle, &font_mont_22, 0);
      lv_obj_set_style_text_color(lblDescTitle, lv_color_hex(0x111827), 0);
      lv_obj_align(lblDescTitle, LV_ALIGN_TOP_LEFT, 0, 95);

      // Descripción texto
      lblDesc = lv_label_create(card);
      lv_label_set_text(lblDesc, curDesc.c_str());
      lv_label_set_long_mode(lblDesc, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(lblDesc, LV_PCT(100));
      lv_obj_set_style_text_font(lblDesc, &lv_font_montserrat_24, 0);
      lv_obj_set_style_text_color(lblDesc, lv_color_hex(0x374151), 0);
      lv_obj_align(lblDesc, LV_ALIGN_TOP_LEFT, 0, 125);

      // ---------- PRECIOS ----------
      lv_obj_t* lblPriceTitle = lv_label_create(scr);
      lv_label_set_text(lblPriceTitle, "Precio:");
      lv_obj_set_style_text_font(lblPriceTitle, &font_mont_22, 0);
      lv_obj_set_style_text_color(lblPriceTitle, lv_color_hex(0x111827), 0);
      lv_obj_align(lblPriceTitle, LV_ALIGN_TOP_LEFT, 30, 348);

      // Precio Bs
      lblPrice = lv_label_create(scr);
      lv_label_set_text(lblPrice, "--.-- Bs.");
      lv_obj_set_style_text_font(lblPrice, &font_mont_44, 0);
      lv_obj_set_style_text_color(lblPrice, lv_color_hex(0x111827), 0);
      lv_obj_align(lblPrice, LV_ALIGN_TOP_LEFT, 30, 378);

      // IVA debajo del precio
      lblIva = lv_label_create(scr);
      lv_label_set_text(lblIva, "IVA: --.-- Bs.");
      lv_obj_set_style_text_font(lblIva, &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(lblIva, lv_color_hex(0x374151), 0);
      lv_obj_align(lblIva, LV_ALIGN_TOP_LEFT, 35, 435);

      // Precio USD
      lv_obj_t* lblUsdTitle = lv_label_create(scr);
      lv_label_set_text(lblUsdTitle, "Precio USD:");
      lv_obj_set_style_text_font(lblUsdTitle, &font_mont_22, 0);
      lv_obj_set_style_text_color(lblUsdTitle, lv_color_hex(0x111827), 0);
      lv_obj_align(lblUsdTitle, LV_ALIGN_TOP_LEFT, 330, 348);

      lblPriceUsd = lv_label_create(scr);
      lv_label_set_text(lblPriceUsd, "--.-- $");
      lv_obj_set_style_text_font(lblPriceUsd, &font_mont_44, 0);
      lv_obj_set_style_text_color(lblPriceUsd, lv_color_hex(0x374151), 0);
      lv_obj_align(lblPriceUsd, LV_ALIGN_TOP_LEFT, 330, 378);

      // Tasa USD
      lv_obj_t* lblRateTitle = lv_label_create(scr);
      lv_label_set_text(lblRateTitle, "Tasa USD BCV:");
      lv_obj_set_style_text_font(lblRateTitle, &font_mont_22, 0);
      lv_obj_set_style_text_color(lblRateTitle, lv_color_hex(0x111827), 0);
      lv_obj_align(lblRateTitle, LV_ALIGN_TOP_LEFT, 560, 348);

      lblRateUsd = lv_label_create(scr);
      lv_label_set_text(lblRateUsd, "--.-- Bs.");
      lv_obj_set_style_text_font(lblRateUsd, &font_mont_44, 0);
      lv_obj_set_style_text_color(lblRateUsd, lv_color_hex(0x374151), 0);
      lv_obj_align(lblRateUsd, LV_ALIGN_TOP_LEFT, 560, 378);

      // ---------- STATUS + DEVICE ----------
      lblStatus = lv_label_create(scr);
      lv_label_set_text(lblStatus, "Status: iniciando...");
      lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_18, 0);
      lv_obj_set_style_text_color(lblStatus, lv_color_hex(0x374151), 0);
      lv_obj_align(lblStatus, LV_ALIGN_BOTTOM_LEFT, 320, -20);

      lblDevice = lv_label_create(scr);
      lv_label_set_text_fmt(lblDevice, "ID: %s", getDeviceId().c_str());
      lv_obj_set_style_text_font(lblDevice, &lv_font_montserrat_18, 0);
      lv_obj_set_style_text_color(lblDevice, lv_color_hex(0x374151), 0);
      lv_obj_align(lblDevice, LV_ALIGN_BOTTOM_RIGHT, -20, -20);

      // ======= Teclado persistente (sin transparencia) =======
      kb_bg = lv_obj_create(lv_scr_act());
      lv_obj_set_size(kb_bg, LV_PCT(100), LV_PCT(100));
      lv_obj_align(kb_bg, LV_ALIGN_CENTER, 0, 0);
      lv_obj_set_style_bg_color(kb_bg, lv_color_hex(0xF9FAFB), 0);
      lv_obj_set_style_bg_opa(kb_bg, LV_OPA_COVER, 0);
      lv_obj_add_flag(kb_bg, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(kb_bg, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_anim_time(kb_bg, 0, 0);

      kb = lv_keyboard_create(lv_scr_act());
      lv_obj_set_size(kb, LV_PCT(100), 220);
      lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
      lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
      lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
      lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_anim_time(kb, 0, 0);
      lv_obj_set_style_pad_all(kb, 2, 0);
      lv_obj_set_style_bg_color(kb, lv_color_white(), 0);
      lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);

      lvgl_port_unlock();

      // Señal para permitir arrancar USB
      xSemaphoreGive(gPanelReady);
      // Warm-up
      lv_timer_create([](lv_timer_t* t) {
        (void)t;
        lvglWarmupDone = true;
        // no borrar timer (uno-shot), da igual si corre 1 vez
      },
                      2500, NULL);
    }
  }

  delay(10000);
  app_main();
  delay(5000);

  LOGF("[BOOT] WiFi.SSID()='%s'\n", WiFi.SSID().c_str());
  LOGF("[BOOT] forcePortal=%s\n",
       (g_forcePortal == FORCE_PORTAL_MAGIC) ? "YES" : "NO");

  if (g_forcePortal == FORCE_PORTAL_MAGIC) {
    g_forcePortal = 0;

    Serial.println("[BOOT] Forcing config portal...");
    WiFiManager wm_force;

    enterPortalMode();

    wm_force.setConfigPortalTimeout(0);
    wm_force.setBreakAfterConfig(true);
    wm_force.startConfigPortal("iVisor");

    exitPortalMode();

    Serial.println("[BOOT] Portal closed -> restart");
    delay(500);
    ESP.restart();
  }

  /************ WiFiManager ************/
  loadConfigFromNVS();

  WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);

  WiFiManager wm;

  p_apiLegacy = new WiFiManagerParameter("apiurl", "URL API (legacy)", apiLegacyBuf, 200);
  wm.addParameter(p_apiLegacy);

  p_apiProduct = new WiFiManagerParameter("apiproduct", "URL API Producto (GET ?code=)", apiProdBuf, 200);
  wm.addParameter(p_apiProduct);

  // Prefijo scanner
  WiFiManagerParameter* p_scannerPrefix = new WiFiManagerParameter("scannerprefix", "Prefijo scanner (BLE)", scannerNamePrefix.c_str(), 32);
  wm.addParameter(p_scannerPrefix);

  // Modo scanner (AUTO/UART/HID)
  char modeBuf[8];
  snprintf(modeBuf, sizeof(modeBuf), "%d", scannerMode);
  WiFiManagerParameter* p_scannerMode = new WiFiManagerParameter("scannermode", "Scanner mode 0=AUTO 1=UART 2=HID", modeBuf, 4);
  wm.addParameter(p_scannerMode);

  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConfigPortalTimeout(0);
  wm.setConnectTimeout(30);

  LOGLN("📡 Iniciando portal WiFi (iVisor) si no hay config...");
  if (!wm.autoConnect("iVisor")) {
    LOGLN("❌ Fallo en portal. Reiniciando...");
    uiSetStatus("Portal WiFi fallo");
    delay(120000);
    ESP.restart();
  }

  apiUrl = String(p_apiLegacy->getValue());
  apiUrl.trim();
  apiProductUrl = String(p_apiProduct->getValue());
  apiProductUrl.trim();

  scannerNamePrefix = String(p_scannerPrefix->getValue());
  scannerNamePrefix.trim();
  scannerMode = atoi(p_scannerMode->getValue());
  if (scannerMode < 0 || scannerMode > 2) scannerMode = 0;

  saveConfigToNVS();

  LOGT("📡 IP: ");
  Serial.println(WiFi.localIP());
  LOGT("🔗 API legacy: ");
  Serial.println(apiUrl);
  LOGT("🔗 API producto: ");
  Serial.println(apiProductUrl);
  LOGT("🔎 Scanner prefix: ");
  Serial.println(scannerNamePrefix);
  LOGT("🔎 Scanner mode: ");
  Serial.println(scannerMode);

  // ---- HTTP worker init ----
  if (!httpQueue) {
    httpQueue = xQueueCreate(8, sizeof(HttpJob));  // cola de 8 consultas
    if (!httpQueue) {
      Serial.println("❌ No se pudo crear httpQueue");
    } else {
      xTaskCreatePinnedToCore(httpWorkerTask, "httpWorker", 8192, NULL, 1, NULL, 0);
      Serial.println("✅ httpWorkerTask iniciado");
    }
  }

  if (shouldRebootAfterConfig) {
    Serial.println("✔ Nueva configuración guardada. Reiniciando en 3s...");
    delay(3000);
    ESP.restart();
  }

  // Iniciales
  fetchUsdRate();
  delay(1000);
  deviceRegistered = false;
  registrarDispositivo();
  delay(1000);
  // Pago inicial
  servicePaid = verificarPago();
  delay(1000);
  setupOtaHttpServer();
  delay(1000);
  uiSetStatus(servicePaid ? "WiFi OK - Servicio iniciado..." : "Servicio suspendido");
  //setupBLE();

  /************ Timers LVGL ************/
  timer_led = lv_timer_create(timer_led_cb, 50, NULL);
  timer_wifi = lv_timer_create(timer_wifi_cb, 2000, NULL);
  timer_api = lv_timer_create(timer_api_cb, INTERVALO_API, NULL);
  timer_fw = lv_timer_create(timer_fw_cb, FW_CHECK_INTERVAL, NULL);
  timer_ui = lv_timer_create(timer_ui_cb, 3000, NULL);
  timer_idle_promo = lv_timer_create(timer_idle_promo_cb, IDLE_ROTATE_MS, NULL);
  // ✅ Watchdog de advertising BLE (iOS BT toggle)
  //timer_ble_adv = lv_timer_create(timer_ble_adv_cb, 2000, NULL);

  systemReady = true;
  initWatchdog();

  uiSetStatus(servicePaid ? "Listo. Scanner." : "Servicio suspendido");
}

/************ LOOP ************/
void loop() {
  if (systemReady) esp_task_wdt_reset();

  if (pendingResetCfg) {
    pendingResetCfg = false;
    resetWifiAndEnterConfig();
  }

    if (g_portalMode) {
    delay(20);
    return;
  }

  if (!idleModeActive && (millis() - lastUserActivityMs > IDLE_RETURN_MS)) {
    clearProductUI();
    showIdleMode();
  }
  // Si estaba mostrando resultado, limpiar a los 5s
  if (resultShowing && (long)(millis() - resultUntilMs) >= 0) {
    resultShowing = false;
    clearProductUI();
  }

  otaServer.handleClient();
  checkAutoReboot();

  if (pendingApplyScannerMode) {
    pendingApplyScannerMode = false;

    int newMode = pendingScannerModeValue;
    LOGF("[CFG] Aplicando modo scanner = %d (en loop)\n", newMode);

    // Aquí sí es seguro tocar BLE:
    scannerMode = newMode;
    applyScannerModeRuntime();  // desconectar/conectar/scan etc
  }

  if (g_barcode_ready) {
    markUserActivity();
    g_barcode_ready = false;
    String code = g_barcode_last;
    esp_task_wdt_reset();
    LOGT("Barcode: ");
    LOGLN(code);
    launchHttpForCode(code, "HID Scanner USB");
  }
  // (opcional) si teclado visible, no hagas nada extra aquí
  delay(20);
}
