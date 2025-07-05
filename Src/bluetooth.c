// 头文件包含
#include "esp_log.h"
#include "nvs_flash.h"
/* BLE 相关头文件 */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "ble_spp_server.h"
#include "driver/uart.h"
#include "esp_log.h"

// 静态变量和全局变量声明
static int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg); // GAP事件回调
static uint8_t own_addr_type; // 本机BLE地址类型
int gatt_svr_register(void); // GATT服务注册函数声明
QueueHandle_t spp_common_uart_queue = NULL; // UART事件队列
static bool conn_handle_subs[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1]; // 记录每个连接是否订阅通知
static uint16_t ble_spp_svc_gatt_read_val_handle; // SPP特征句柄

void ble_store_config_init(void); // 存储配置初始化

uint8_t buf[256] = {}; // BLE数据缓冲区
// 联合体，用于解析BLE收到的数据
union{
    struct{
        char head;
        int i;
    } data;
    char c[5];
}rec;
int light = 25; // 亮度变量

/**
 * 打印连接描述信息到控制台
 */
static void ble_spp_server_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    MODLOG_DFLT(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                "encrypted=%d authenticated=%d bonded=%d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

/**
 * 启动BLE广播，设置广播参数和内容
 */
static void
ble_spp_server_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     * 设置广播包内容，包括标志、发射功率、设备名、服务UUID等
     */
    memset(&fields, 0, sizeof fields);

    // 设置广播标志：可发现、仅BLE
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    // 包含TX功率字段
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    // 设置设备名称
    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    // 设置16位服务UUID
    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(BLE_SVC_SPP_UUID16)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    // 应用广播字段
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "设置广播数据出错; rc=%d\n", rc);
        return;
    }

    // 启动广播
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_spp_server_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "使能广播出错; rc=%d\n", rc);
        return;
    }
}

/**
 * GAP事件回调，处理连接、断开、参数更新、订阅等事件
 */
static int
ble_spp_server_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_LINK_ESTAB:
        // 新连接建立或连接失败
        MODLOG_DFLT(INFO, "连接%s; status=%d ",
                    event->connect.status == 0 ? "已建立" : "失败",
                    event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            ble_spp_server_print_conn_desc(&desc);
        }
        MODLOG_DFLT(INFO, "\n");
        if (event->connect.status != 0 || CONFIG_BT_NIMBLE_MAX_CONNECTIONS > 1) {
            // 连接失败或允许多连接时，恢复广播
            ble_spp_server_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        // 连接断开
        MODLOG_DFLT(INFO, "断开连接; 原因=%d ", event->disconnect.reason);
        ble_spp_server_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        conn_handle_subs[event->disconnect.conn.conn_handle] = false;

        // 断开后恢复广播
        ble_spp_server_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        // 连接参数更新
        MODLOG_DFLT(INFO, "连接参数已更新; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        ble_spp_server_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        // 广播完成后再次广播
        MODLOG_DFLT(INFO, "广播完成; reason=%d",
                    event->adv_complete.reason);
        ble_spp_server_advertise();
        return 0;

    case BLE_GAP_EVENT_MTU:
        // MTU更新事件
        MODLOG_DFLT(INFO, "MTU更新事件; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        // 客户端订阅通知
        MODLOG_DFLT(INFO, "订阅事件; conn_handle=%d attr_handle=%d "
                    "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        conn_handle_subs[event->subscribe.conn_handle] = true;
        return 0;

    default:
        return 0;
    }
}

/**
 * BLE协议栈重置回调
 */
static void
ble_spp_server_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "重置状态; 原因=%d\n", reason);
}

/**
 * BLE协议栈同步回调，主要用于启动广播
 */
static void
ble_spp_server_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    // 推断本机BLE地址类型
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "确定地址类型出错; rc=%d\n", rc);
        return;
    }

    // 打印本机BLE地址
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "设备地址: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");
    // 启动广播
    ble_spp_server_advertise();
}

/**
 * BLE主机任务，运行NimBLE协议栈
 */
void ble_spp_server_host_task(void *param)
{
    MODLOG_DFLT(INFO, "BLE主机任务已启动");
    // 只有执行nimble_port_stop()时该函数才会返回
    nimble_port_run();

    nimble_port_freertos_deinit();
}

/**
 * GATT特征访问回调，处理读写请求
 */
static int  ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        MODLOG_DFLT(INFO, "读回调");
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        MODLOG_DFLT(INFO, "写事件收到数据,conn_handle = %x,attr_handle = %x", conn_handle, attr_handle);
        // 读取数据内容
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len > sizeof(buf)) len = sizeof(buf);
        if(len > 5)
            break;
        os_mbuf_copydata(ctxt->om, 0, len, buf);
        if(len == 1)
        {
            // 单字节命令，可自定义处理
        }    
        else
        {
            // 多字节数据，解析亮度
            rec.c[1] = buf[1];
            rec.c[2] = buf[2];
            rec.c[3] = buf[3];
            rec.c[4] = buf[4];
            ESP_LOGI("BLE_ESP", "调节亮度: %d:%d", rec.data.i, len);
            light = rec.data.i;
        }
        break;

    default:
        MODLOG_DFLT(INFO, "\n默认回调");
        break;
    }
    return 0;
}

/**
 * 自定义GATT服务定义
 */
static const struct ble_gatt_svc_def new_ble_svc_gatt_defs[] = {
    {
        /*** 服务: SPP */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_SPP_UUID16),
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                /* SPP特征 */
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16),
                .access_cb = ble_svc_gatt_handler,
                .val_handle = &ble_spp_svc_gatt_read_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            }, {
                0, /* 没有更多特征 */
            }
        },
    },
    {
        0, /* 没有更多服务 */
    },
};

/**
 * GATT服务注册回调，打印注册信息
 */
static void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "注册服务 %s, handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "注册特征 %s, def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "注册描述符 %s, handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

/**
 * 初始化GATT服务
 */
int gatt_svr_init(void)
{
    int rc = 0;
    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(new_ble_svc_gatt_defs);

    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(new_ble_svc_gatt_defs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * BLE服务端UART任务，监听串口数据并通过BLE通知客户端
 */
void ble_server_uart_task(void *pvParameters)
{
    MODLOG_DFLT(INFO, "BLE服务端UART任务已启动\n");
    uart_event_t event;
    int rc = 0;
    for (;;) {
        // 等待UART事件
        if (xQueueReceive(spp_common_uart_queue, (void * )&event, (TickType_t)portMAX_DELAY)) {
            switch (event.type) {
            // UART收到数据事件
            case UART_DATA:
                if (event.size) {
                    uint8_t *ntf;
                    ntf = (uint8_t *)malloc(sizeof(uint8_t) * event.size);
                    memset(ntf, 0x00, event.size);
                    uart_read_bytes(UART_NUM_0, ntf, event.size, portMAX_DELAY);

                    for (int i = 0; i <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
                        /* 检查客户端是否已订阅通知 */
                        if (conn_handle_subs[i]) {
                            struct os_mbuf *txom;
                            txom = ble_hs_mbuf_from_flat(ntf, event.size);
                            rc = ble_gatts_notify_custom(i, ble_spp_svc_gatt_read_val_handle,
                                                         txom);
                            if (rc == 0) {
                                MODLOG_DFLT(INFO, "通知发送成功");
                            } else {
                                MODLOG_DFLT(INFO, "发送通知出错 rc = %d", rc);
                            }
                        }
                    }

                    free(ntf);
                }
                break;
            default:
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

/**
 * 初始化UART并启动BLE服务端UART任务
 */
static void ble_spp_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_RTS,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // 安装UART驱动，并获取队列
    uart_driver_install(UART_NUM_0, 4096, 8192, 10, &spp_common_uart_queue, 0);
    // 设置UART参数
    uart_param_config(UART_NUM_0, &uart_config);
    // 设置UART引脚
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    xTaskCreate(ble_server_uart_task, "uTask", 4096, (void *)UART_NUM_0, 8, NULL);
}

/**
 * BLE整体初始化入口
 */
void init_ble()
{
    int rc;

    // 初始化NVS（用于存储PHY校准数据）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化NimBLE协议栈
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", ret);
        return;
    }

    // 初始化连接订阅数组
    for (int i = 0; i <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        conn_handle_subs[i] = false;
    }

    // 初始化UART驱动并启动任务
    ble_spp_uart_init();

    // 配置NimBLE主机参数
    ble_hs_cfg.reset_cb = ble_spp_server_on_reset;
    ble_hs_cfg.sync_cb = ble_spp_server_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = CONFIG_EXAMPLE_IO_TYPE;
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_bonding = 1;
#endif
#ifdef CONFIG_EXAMPLE_MITM
    ble_hs_cfg.sm_mitm = 1;
#endif
#ifdef CONFIG_EXAMPLE_USE_SC
    ble_hs_cfg.sm_sc = 1;
#else
    ble_hs_cfg.sm_sc = 0;
#endif
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_our_key_dist = 1;
    ble_hs_cfg.sm_their_key_dist = 1;
#endif

    // 注册自定义GATT服务
    rc = gatt_svr_init();
    assert(rc == 0);

    // 设置默认设备名
    rc = ble_svc_gap_device_name_set("tetris game");
    assert(rc == 0);

    // 初始化存储
    ble_store_config_init();

    // 启动NimBLE主机任务
    nimble_port_freertos_init(ble_spp_server_host_task);
}