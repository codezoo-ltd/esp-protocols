/* MQTT over SSL Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_modem_api.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "sdkconfig.h"
#include "driver/gpio.h"

#include <stdio.h>

#define GPIO_OUTPUT_LED				(gpio_num_t)GPIO_NUM_23
#define GPIO_OUTPUT_LED_PIN_SEL		(1ULL<<GPIO_OUTPUT_LED)
#define GPIO_OUTPUT_RESET			(gpio_num_t)GPIO_NUM_5
#define GPIO_OUTPUT_RESET_PIN_SEL	(1ULL<<GPIO_OUTPUT_RESET)
#define GPIO_OUTPUT_RELAY1			(gpio_num_t)GPIO_NUM_32
#define GPIO_OUTPUT_RELAY1_PIN_SEL	(1ULL<<GPIO_OUTPUT_RELAY1)
#define GPIO_OUTPUT_RELAY2			(gpio_num_t)GPIO_NUM_33
#define GPIO_OUTPUT_RELAY2_PIN_SEL	(1ULL<<GPIO_OUTPUT_RELAY2)
#define GPIO_OUTPUT_RELAY3			(gpio_num_t)GPIO_NUM_25
#define GPIO_OUTPUT_RELAY3_PIN_SEL	(1ULL<<GPIO_OUTPUT_RELAY3)
#define GPIO_OUTPUT_RELAY4			(gpio_num_t)GPIO_NUM_26
#define GPIO_OUTPUT_RELAY4_PIN_SEL	(1ULL<<GPIO_OUTPUT_RELAY4)

#if defined(CONFIG_EXAMPLE_FLOW_CONTROL_NONE)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_NONE
#elif defined(CONFIG_EXAMPLE_FLOW_CONTROL_SW)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_SW
#elif defined(CONFIG_EXAMPLE_FLOW_CONTROL_HW)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_HW
#endif

static const char *TAG = "mqtts_example";
static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const int GOT_DATA_BIT = BIT2;
static const int USB_DISCONNECTED_BIT = BIT3; // Used only with USB DTE but we define it uncondit

#define USERNAME "mqtt_username"
#define PASSWORD "mqtt_password"
#define SUB_TOPIC1 "type1sc/100/test"
#define SUB_TOPIC2 "type1sc/100/status"

#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_eclipseprojects_io_pem_start[]  = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_eclipseprojects_io_pem_start[]   asm("_binary_mqtt_eclipseprojects_io_pem_start");
#endif
extern const uint8_t mqtt_eclipseprojects_io_pem_end[]   asm("_binary_mqtt_eclipseprojects_io_pem_end");

static void config_gpio(void)
{
	gpio_config_t io_conf = {};                     //zero-initialize the config structure.

	io_conf.intr_type = GPIO_INTR_DISABLE;          //disable interrupt
	io_conf.mode = GPIO_MODE_OUTPUT;                //set as output mode
	io_conf.pin_bit_mask = (GPIO_OUTPUT_LED_PIN_SEL | GPIO_OUTPUT_RESET_PIN_SEL | GPIO_OUTPUT_RELAY1_PIN_SEL | GPIO_OUTPUT_RELAY2_PIN_SEL | GPIO_OUTPUT_RELAY3_PIN_SEL | GPIO_OUTPUT_RELAY4_PIN_SEL);
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;   //disable pull-down mode
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;       //disable pull-up mode

	gpio_config(&io_conf);                          //configure GPIO with the given settings
}

static void gpio_modem(void)
{
	/* Power on the modem */
	ESP_LOGI(TAG, "Power on the modem");
	gpio_set_level(GPIO_OUTPUT_RESET, 0);
	vTaskDelay(pdMS_TO_TICKS(100));
	gpio_set_level(GPIO_OUTPUT_RESET, 1);
	vTaskDelay(pdMS_TO_TICKS(2000));
}

//
// Note: this function is for testing purposes only publishing part of the active partition
//       (to be checked against the original binary)
//
static void send_binary(esp_mqtt_client_handle_t client)
{
	esp_partition_mmap_handle_t out_handle;
	const void *binary_address;
	const esp_partition_t *partition = esp_ota_get_running_partition();
	esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &binary_address, &out_handle);
	// sending only the configured portion of the partition (if it's less than the partition size)
	int binary_size = MIN(CONFIG_BROKER_BIN_SIZE_TO_SEND, partition->size);
	int msg_id = esp_mqtt_client_publish(client, "/topic/binary", binary_address, binary_size, 0, 0);
	ESP_LOGI(TAG, "binary sent with msg_id=%d", msg_id);
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
	esp_mqtt_event_handle_t event = event_data;
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
	switch ((esp_mqtt_event_id_t)event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			msg_id = esp_mqtt_client_subscribe(client, SUB_TOPIC1, 0);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

			msg_id = esp_mqtt_client_subscribe(client, SUB_TOPIC2, 0);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

//			msg_id = esp_mqtt_client_unsubscribe(client, SUB_TOPIC2);
//			ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			break;

		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			msg_id = esp_mqtt_client_publish(client, SUB_TOPIC1, "data", 0, 0, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			gpio_set_level(GPIO_OUTPUT_LED, 1);
			vTaskDelay(pdMS_TO_TICKS(100));
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
			printf("DATA=%.*s\r\n", event->data_len, event->data);

			if (strncmp(event->data, "relay1:1", event->data_len) == 0) {
				ESP_LOGI(TAG, "RELAY1 On.");					
				gpio_set_level(GPIO_OUTPUT_RELAY1, 1);
				vTaskDelay(pdMS_TO_TICKS(100));
				esp_mqtt_client_publish(client, SUB_TOPIC2, "RELAY1 On.", 0, 0, 0);
			}
			else if (strncmp(event->data, "relay1:0", event->data_len) == 0) {
				ESP_LOGI(TAG, "RELAY1 Off.");
				gpio_set_level(GPIO_OUTPUT_RELAY1, 0);
				vTaskDelay(pdMS_TO_TICKS(100));
				esp_mqtt_client_publish(client, SUB_TOPIC2, "RELAY1 Off.", 0, 0, 0);
			}
			else if (strncmp(event->data, "relay2:1", event->data_len) == 0) {
				ESP_LOGI(TAG, "RELAY2 On.");
				gpio_set_level(GPIO_OUTPUT_RELAY2, 1);
				vTaskDelay(pdMS_TO_TICKS(100));
				esp_mqtt_client_publish(client, SUB_TOPIC2, "RELAY2 On.", 0, 0, 0);				
			}
			else if (strncmp(event->data, "relay2:0", event->data_len) == 0) {
				ESP_LOGI(TAG, "RELAY2 Off.");
				gpio_set_level(GPIO_OUTPUT_RELAY2, 0);
				vTaskDelay(pdMS_TO_TICKS(100));
				esp_mqtt_client_publish(client, SUB_TOPIC2, "RELAY2 Off.", 0, 0, 0);				
			}
			else if (strncmp(event->data, "relay3:1", event->data_len) == 0) {
				ESP_LOGI(TAG, "RELAY3 On.");
				gpio_set_level(GPIO_OUTPUT_RELAY3, 1);
				vTaskDelay(pdMS_TO_TICKS(100));
				esp_mqtt_client_publish(client, SUB_TOPIC2, "RELAY3 On.", 0, 0, 0);				
			}
			else if (strncmp(event->data, "relay3:0", event->data_len) == 0) {
				ESP_LOGI(TAG, "RELAY3 Off.");
				gpio_set_level(GPIO_OUTPUT_RELAY3, 0);
				vTaskDelay(pdMS_TO_TICKS(100));
				esp_mqtt_client_publish(client, SUB_TOPIC2, "RELAY3 Off.", 0, 0, 0);				
			}
			else if (strncmp(event->data, "relay4:1", event->data_len) == 0) {
				ESP_LOGI(TAG, "RELAY4 On.");
				gpio_set_level(GPIO_OUTPUT_RELAY4, 1);
				vTaskDelay(pdMS_TO_TICKS(100));
				esp_mqtt_client_publish(client, SUB_TOPIC2, "RELAY4 On.", 0, 0, 0);				
			}
			else if (strncmp(event->data, "relay4:0", event->data_len) == 0) {
				ESP_LOGI(TAG, "RELAY4 Off.");
				gpio_set_level(GPIO_OUTPUT_RELAY4, 0);
				vTaskDelay(pdMS_TO_TICKS(100));
				esp_mqtt_client_publish(client, SUB_TOPIC2, "RELAY4 Off.", 0, 0, 0);				
			}
			else if (strncmp(event->data, "send binary please", event->data_len) == 0) {
				ESP_LOGI(TAG, "Sending the binary");
				send_binary(client);
			}
			xEventGroupSetBits(event_group, GOT_DATA_BIT);
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			gpio_set_level(GPIO_OUTPUT_LED, 0);
			vTaskDelay(pdMS_TO_TICKS(100));			
			if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
				ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
				ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
				ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
						strerror(event->error_handle->esp_transport_sock_errno));
			} else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
				ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
			} else {
				ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
			}
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG, "PPP state changed event %" PRIu32, event_id);
	if (event_id == NETIF_PPP_ERRORUSER) {
		/* User interrupted event from esp-netif */
		esp_netif_t **p_netif = event_data;
		ESP_LOGI(TAG, "User interrupted event from netif:%p", *p_netif);
	}
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data)
{
	ESP_LOGD(TAG, "IP event! %" PRIu32, event_id);
	if (event_id == IP_EVENT_PPP_GOT_IP) {
		esp_netif_dns_info_t dns_info;

		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		esp_netif_t *netif = event->esp_netif;

		ESP_LOGI(TAG, "Modem Connect to PPP Server");
		ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
		ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
		ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
		ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
		esp_netif_get_dns_info(netif, 0, &dns_info);
		ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
		esp_netif_get_dns_info(netif, 1, &dns_info);
		ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
		ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
		xEventGroupSetBits(event_group, CONNECT_BIT);

		ESP_LOGI(TAG, "GOT ip event!!!");
	} else if (event_id == IP_EVENT_PPP_LOST_IP) {
		ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
	} else if (event_id == IP_EVENT_GOT_IP6) {
		ESP_LOGI(TAG, "GOT IPv6 event!");

		ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
		ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
	}
}


static void mqtt_app_start(void)
{
	const esp_mqtt_client_config_t mqtt_cfg = {
		.broker = {
			.address.uri = CONFIG_BROKER_URI,
			.verification.certificate = (const char *)mqtt_eclipseprojects_io_pem_start
		}, 
		.credentials = {
			.username = (const char *)USERNAME,
			.authentication.password = (const char *)PASSWORD
		}
	};

	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
	/* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(client);
}

void app_main(void)
{
	// Initialize GPIO
	config_gpio();
	gpio_modem();

	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
	esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
	esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
	esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
	esp_log_level_set("transport", ESP_LOG_VERBOSE);
	esp_log_level_set("outbox", ESP_LOG_VERBOSE);

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

	/* Configure the PPP netif */
	esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);
	esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
	esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config); assert(esp_netif);

	event_group = xEventGroupCreate();

	/* Configure the DTE */
	esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();

	ESP_LOGI(TAG, "Initializing esp_modem for a generic module...");
	esp_modem_dce_t *dce = esp_modem_new(&dte_config, &dce_config, esp_netif);
	assert(dce);
	if (dte_config.uart_config.flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
		esp_err_t err = esp_modem_set_flow_control(dce, 2, 2);  //2/2 means HW Flow Control.
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Failed to set the set_flow_control mode");
			return;
		}
		ESP_LOGI(TAG, "HW set_flow_control OK");
	}

	xEventGroupClearBits(event_group, CONNECT_BIT | GOT_DATA_BIT | USB_DISCONNECTED_BIT);	
    esp_err_t err = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", err);
        return;
    }
    /* Wait for IP address */
    ESP_LOGI(TAG, "Waiting for IP address");
    xEventGroupWaitBits(event_group, CONNECT_BIT | USB_DISCONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

	mqtt_app_start();
}
