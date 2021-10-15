/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "lwip/opt.h"
#include "mqtt.h"

#if LWIP_IPV4 && LWIP_RAW && LWIP_NETCONN && LWIP_DHCP && LWIP_DNS



/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static void connect_to_mqtt(void *ctx);

/*******************************************************************************
 * Variables
 ******************************************************************************/

static mdio_handle_t mdioHandle = {.ops = &EXAMPLE_MDIO_OPS};
static phy_handle_t phyHandle   = {.phyAddr = EXAMPLE_PHY_ADDRESS, .mdioHandle = &mdioHandle, .ops = &EXAMPLE_PHY_OPS};

/*! @brief MQTT client data. */
static mqtt_client_t *mqtt_client;

/*! @brief MQTT client ID string. */
static char client_id[40];

/*! @brief MQTT client information. */
static const struct mqtt_connect_client_info_t mqtt_client_info = {
    .client_id   = (const char *)&client_id[0],
    .client_user = "IsaacGallegos",
    .client_pass = "aio_Lugx31VC4j5jdqIPl4BHEMFhiagy",
    .keep_alive  = 100,
    .will_topic  = NULL,
    .will_msg    = NULL,
    .will_qos    = 0,
    .will_retain = 0,
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    .tls_config = NULL,
#endif
};

/*! @brief MQTT broker IP address. */
static ip_addr_t mqtt_addr;

/*! @brief Indicates connection to MQTT broker. */
static volatile bool connected = false;

/*! @brief Point to the array of subscribed topics*/
//char *(subscribed_topics[]);
char *mqtt_sub_topics[] = {"IsaacGallegos/feeds/red-led", "IsaacGallegos/feeds/periodo"};

/*! @brief Topic's Quality of Service*/
int *topics_qos;

/*! @brief Array of callbacks for incoming publish in subscribed topics*/
void (**callback_income_publish)(char *msg, int len);

/*! @brief Number of subscribed topics*/
int number_sub_topics;

/*! @brief Saves the topic index when receive a message from a subscribed topic*/
int income_topic_index;

/*! @brief Incoming message from subscribed topic*/
char income_msg[40];

/*! @brief Point to the array of subscribed topics*/
char *published_topic;

/*! @brief Topic's Quality of Service*/
char *published_message;


/*******************************************************************************
 * Code
 ******************************************************************************/

/*!
 * @brief Called when subscription request finishes.
 */
static void mqtt_topic_subscribed_cb(void *arg, err_t err)
{
    const char *topic = (const char *)arg;

    if (err == ERR_OK)
    {
        PRINTF("Subscribed to the topic \"%s\".\r\n", topic);
    }
    else
    {
        PRINTF("Failed to subscribe to the topic \"%s\": %d.\r\n", topic, err);
    }
}

/*!
 * @brief Called when there is a message on a subscribed topic.
 */
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
	int i = 0;
	int cmp;
    LWIP_UNUSED_ARG(arg);

    PRINTF("Received %u bytes from the topic \"%s\": \"", tot_len, topic);

    do{
    	cmp = strcmp(topic, mqtt_sub_topics[i]);
    	if(cmp == 0)
    	{
    		income_topic_index = i;
    	}
    	i += 1;
    } while ((i < number_sub_topics) || (cmp == 0));
}

/*!
 * @brief Called when recieved incoming published message fragment.
 */
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    int i;

    LWIP_UNUSED_ARG(arg);

    for (i = 0; i < len; i++)
    {
        if (isprint(data[i]))
        {
            PRINTF("%c", (char)data[i]);
        }
        else
        {
            PRINTF("\\x%02x", data[i]);
        }
    }

    if (flags & MQTT_DATA_FLAG_LAST)
    {
        PRINTF("\"\r\n");
    }


    strncpy(income_msg, data, len);		//Copy the received message to a global variable
    callback_income_publish[income_topic_index](income_msg, len);	//Pass the message and the len to the callback

}

/*!
 * @brief Subscribe to MQTT topics.
 */
static void mqtt_subscribe_topics(mqtt_client_t *client)
{
    err_t err;
    int i;

    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb,
                            LWIP_CONST_CAST(void *, &mqtt_client_info));

    for (i = 0; i < ARRAY_SIZE(mqtt_sub_topics); i++)
    {
        err = mqtt_subscribe(client, mqtt_sub_topics[i], topics_qos[i], mqtt_topic_subscribed_cb, LWIP_CONST_CAST(void *, mqtt_sub_topics[i]));

        if (err == ERR_OK)
        {
            PRINTF("Subscribing to the topic \"%s\" with QoS %d...\r\n", mqtt_sub_topics[i], topics_qos[i]);
        }
        else
        {
            PRINTF("Failed to subscribe to the topic \"%s\" with QoS %d: %d.\r\n", mqtt_sub_topics[i], topics_qos[i], err);
        }
    }
}

/*!
 * @brief Called when connection state changes.
 */
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    const struct mqtt_connect_client_info_t *client_info = (const struct mqtt_connect_client_info_t *)arg;

    connected = (status == MQTT_CONNECT_ACCEPTED);

    switch (status)
    {
        case MQTT_CONNECT_ACCEPTED:
            PRINTF("MQTT client \"%s\" connected.\r\n", client_info->client_id);
            mqtt_subscribe_topics(client);
            break;

        case MQTT_CONNECT_DISCONNECTED:
            PRINTF("MQTT client \"%s\" not connected.\r\n", client_info->client_id);
            /* Try to reconnect 1 second later */
            sys_timeout(1000, connect_to_mqtt, NULL);
            break;

        case MQTT_CONNECT_TIMEOUT:
            PRINTF("MQTT client \"%s\" connection timeout.\r\n", client_info->client_id);
            /* Try again 1 second later */
            sys_timeout(1000, connect_to_mqtt, NULL);
            break;

        case MQTT_CONNECT_REFUSED_PROTOCOL_VERSION:
        case MQTT_CONNECT_REFUSED_IDENTIFIER:
        case MQTT_CONNECT_REFUSED_SERVER:
        case MQTT_CONNECT_REFUSED_USERNAME_PASS:
        case MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_:
            PRINTF("MQTT client \"%s\" connection refused: %d.\r\n", client_info->client_id, (int)status);
            /* Try again 10 seconds later */
            sys_timeout(10000, connect_to_mqtt, NULL);
            break;

        default:
            PRINTF("MQTT client \"%s\" connection status: %d.\r\n", client_info->client_id, (int)status);
            /* Try again 10 seconds later */
            sys_timeout(10000, connect_to_mqtt, NULL);
            break;
    }
}

/*!
 * @brief Starts connecting to MQTT broker. To be called on tcpip_thread.
 */
static void connect_to_mqtt(void *ctx)
{
    LWIP_UNUSED_ARG(ctx);

    PRINTF("Connecting to MQTT broker at %s...\r\n", ipaddr_ntoa(&mqtt_addr));

    mqtt_client_connect(mqtt_client, &mqtt_addr, EXAMPLE_MQTT_SERVER_PORT, mqtt_connection_cb,
                        LWIP_CONST_CAST(void *, &mqtt_client_info), &mqtt_client_info);
}

/*!
 * @brief Called when publish request finishes.
 */
static void mqtt_message_published_cb(void *arg, err_t err)
{
    const char *topic = (const char *)arg;

    if (err == ERR_OK)
    {
        PRINTF("Published to the topic \"%s\".\r\n", topic);
    }
    else
    {
        PRINTF("Failed to publish to the topic \"%s\": %d.\r\n", topic, err);
    }
}

/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_message(void *ctx)
{
	char *topic   = published_topic;
    char *message = published_message;

    LWIP_UNUSED_ARG(ctx);

    PRINTF("Going to publish to the topic \"%s\"...\r\n", topic);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)topic);
}

/*!
 * @brief Application thread.
 */
static void app_thread(void *arg)
{
    struct netif *netif = (struct netif *)arg;
    struct dhcp *dhcp;
    err_t err;
    int i;

    /* Wait for address from DHCP */

    PRINTF("Getting IP address from DHCP...\r\n");

    do
    {
        if (netif_is_up(netif))
        {
            dhcp = netif_dhcp_data(netif);
        }
        else
        {
            dhcp = NULL;
        }

        sys_msleep(20U);

    } while ((dhcp == NULL) || (dhcp->state != DHCP_STATE_BOUND));

    PRINTF("\r\nIPv4 Address     : %s\r\n", ipaddr_ntoa(&netif->ip_addr));
    PRINTF("IPv4 Subnet mask : %s\r\n", ipaddr_ntoa(&netif->netmask));
    PRINTF("IPv4 Gateway     : %s\r\n\r\n", ipaddr_ntoa(&netif->gw));

    /*
     * Check if we have an IP address or host name string configured.
     * Could just call netconn_gethostbyname() on both IP address or host name,
     * but we want to print some info if goint to resolve it.
     */
    if (ipaddr_aton(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr) && IP_IS_V4(&mqtt_addr))
    {
        /* Already an IP address */
        err = ERR_OK;
    }
    else
    {
        /* Resolve MQTT broker's host name to an IP address */
        PRINTF("Resolving \"%s\"...\r\n", EXAMPLE_MQTT_SERVER_HOST);
        err = netconn_gethostbyname(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr);
    }

    if (err == ERR_OK)
    {
        /* Start connecting to MQTT broker from tcpip_thread */
        err = tcpip_callback(connect_to_mqtt, NULL);
        if (err != ERR_OK)
        {
            PRINTF("Failed to invoke broker connection on the tcpip_thread: %d.\r\n", err);
        }
    }
    else
    {
        PRINTF("Failed to obtain IP address: %d.\r\n", err);
    }

    /* Publish some messages */
    /*for (i = 0; i < 2;)
    {
        if (connected)
        {
            err = tcpip_callback(publish_message, NULL);
            if (err != ERR_OK)
            {
                PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
            }
            i++;
        }

        sys_msleep(1000U);
    }*/
    vTaskDelete(NULL);
}

static void generate_client_id(void)
{
    uint32_t mqtt_id[MQTT_ID_SIZE];
    int res;

    get_mqtt_id(&mqtt_id[0]);

    res = snprintf(client_id, sizeof(client_id), "nxp_%08lx%08lx%08lx%08lx", mqtt_id[3], mqtt_id[2], mqtt_id[1],
                   mqtt_id[0]);
    if ((res < 0) || (res >= sizeof(client_id)))
    {
        PRINTF("snprintf failed: %d\r\n", res);
        while (1)
        {
        }
    }
}

/*!
 * @brief Initializes lwIP stack.
 *
 * @param arg unused
 */
void lwip_mqtt_stack_init(void *arg)
{
    static struct netif netif;
    ip4_addr_t netif_ipaddr, netif_netmask, netif_gw;
    ethernetif_config_t enet_config = {
        .phyHandle  = &phyHandle,
        .macAddress = configMAC_ADDR,
    };

    LWIP_UNUSED_ARG(arg);
    generate_client_id();

    mdioHandle.resource.csrClock_Hz = EXAMPLE_CLOCK_FREQ;

    IP4_ADDR(&netif_ipaddr, 0U, 0U, 0U, 0U);
    IP4_ADDR(&netif_netmask, 0U, 0U, 0U, 0U);
    IP4_ADDR(&netif_gw, 0U, 0U, 0U, 0U);

    tcpip_init(NULL, NULL);

    LOCK_TCPIP_CORE();
    mqtt_client = mqtt_client_new();
    UNLOCK_TCPIP_CORE();
    if (mqtt_client == NULL)
    {
        PRINTF("mqtt_client_new() failed.\r\n");
        while (1)
        {
        }
    }

    netifapi_netif_add(&netif, &netif_ipaddr, &netif_netmask, &netif_gw, &enet_config, EXAMPLE_NETIF_INIT_FN,
                       tcpip_input);
    netifapi_netif_set_default(&netif);
    netifapi_netif_set_up(&netif);

    netifapi_dhcp_start(&netif);

    PRINTF("\r\n************************************************\r\n");
    PRINTF(" MQTT client example\r\n");
    PRINTF("************************************************\r\n");

    if (sys_thread_new("app_task", app_thread, &netif, APP_THREAD_STACKSIZE, 14) == NULL)
    {
        LWIP_ASSERT("stack_init(): Task creation failed.", 0);
    }

    vTaskDelete(NULL);
}

/*!
 * @brief Defines the topics to be subscribed
 *
 * @param topics
 */
void set_subscribed_topics(char *(topics[]), int qos[], void (*func[])(char* msg, int len),int num_topics)
{
	//subscribed_topics = topics;
	topics_qos = qos;
	callback_income_publish = func;
}

void mqtt_publish_mesagge(char *topic, char *msg)
{
	err_t err;
    if (connected)
    {
    	published_topic = topic;
    	published_message = msg;
        err = tcpip_callback(publish_message, NULL);
        if (err != ERR_OK)
        {
            PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
        }
    }
}

bool mqtt_is_connected(void)
{
	return connected;
}

#endif
