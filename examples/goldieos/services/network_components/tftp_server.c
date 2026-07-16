#if 0
#include "lwip/udp.h"
#include "lwip/mem.h"
#include <string.h>

#include "stdlib.h"
#include "lwip/nettool/misc.h"
#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include <stdio.h>

#ifdef CONFIG_MIDDLEWARE_SUPPORT_LFS
#include "lfs.h"
#include "fcntl.h"
#include "sfc.h"
#include "partition.h"
#include "littlefs_adapt.h"
#endif
// TFTP调试选项
//#define TFTP_DEBUG(...) // printf(__VA_ARGS__)
// =====================================================

// TFTP协议定义
#define TFTP_OPCODE_RRQ   1
#define TFTP_OPCODE_WRQ   2
#define TFTP_OPCODE_DATA  3
#define TFTP_OPCODE_ACK   4
#define TFTP_OPCODE_ERROR 5

// 最大文件名长度和块大小
#define TFTP_MAX_FILENAME 256
#define TFTP_BLOCK_SIZE   512

// TFTP连接状态结构体
typedef struct {
    struct udp_pcb *pcb;      // UDP控制块
    int file;                 // 文件描述符
    u16_t block_num;          // 当前块号
    ip_addr_t addr;           // 客户端IP
    u16_t port;               // 客户端端口
    char filename[TFTP_MAX_FILENAME]; // 目标文件名
    uint8_t mode;             // 0:写模式 1:读模式
    uint8_t last_block;       // 最后块标记
} tftp_connection;
//asdf
static struct udp_pcb *tftp_pcb;

// ================== 内部函数声明 ==================
static void tftp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, 
                              const ip_addr_t *addr, u16_t port);
static void tftp_data_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                   const ip_addr_t *addr, u16_t port);
static void tftp_ack_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                  const ip_addr_t *addr, u16_t port);
static void send_tftp_ack(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port, u16_t block_num);
static void send_tftp_error(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port, 
                           u16_t code, const char *msg);
static void send_tftp_data(tftp_connection *conn);
static void cleanup_connection(tftp_connection *conn);

// ================== 核心函数 ==================
void tftp_init(void) {
    tftp_pcb = udp_new();
    if (tftp_pcb) {
        udp_bind(tftp_pcb, IP_ADDR_ANY, 69);  // 绑定到TFTP默认端口69
        udp_recv(tftp_pcb, tftp_recv_callback, NULL);
        printf("[TFTP] Server started on port 69\n");
    }
}

// ================== 回调函数 ==================
static void tftp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, 
                              const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)pcb;

    if (!p || p->len < 4) {
        pbuf_free(p);
        return;
    }

    u16_t opcode = ntohs(*(u16_t *)p->payload);
    if (opcode == TFTP_OPCODE_WRQ || opcode == TFTP_OPCODE_RRQ) {
        char *filename = (char *)p->payload + 2;
        char *mode = filename + strlen(filename) + 1;

        // 仅支持octet模式
        if (strcmp(mode, "octet") != 0) {
            printf("[TFTP] Unsupported mode: %s\n", mode);
            send_tftp_error(pcb, addr, port, 0, "Unsupported mode");
            pbuf_free(p);
            return;
        }

        // 创建新的UDP连接
        struct udp_pcb *data_pcb = udp_new();
        if (!data_pcb) {
            send_tftp_error(pcb, addr, port, 0, "Out of resources");
            pbuf_free(p);
            return;
        }
        if (udp_bind(data_pcb, IP_ADDR_ANY, 0) != ERR_OK) {
            udp_remove(data_pcb);
            send_tftp_error(pcb, addr, port, 0, "Bind failed");
            pbuf_free(p);
            return;
        }
        udp_connect(data_pcb, addr, port);

        // 创建连接结构体
        tftp_connection *conn = (tftp_connection *)osal_kmalloc(sizeof(tftp_connection), OSAL_GFP_ATOMIC);
        if (!conn) {
            udp_remove(data_pcb);
            send_tftp_error(pcb, addr, port, 0, "Out of memory");
            pbuf_free(p);
            return;
        }
        memset(conn, 0, sizeof(tftp_connection));
        conn->pcb = data_pcb;
        conn->port = port;
        ip_addr_copy(conn->addr, *addr);
        strncpy(conn->filename, filename, TFTP_MAX_FILENAME - 1);

        if (opcode == TFTP_OPCODE_WRQ) {
            // 写请求处理
            conn->mode = 0;
            conn->block_num = 1;
            fs_adapt_delete(conn->filename);
            conn->file = fs_adapt_open(conn->filename, O_RDWR | O_CREAT);
            if (conn->file < 0) {
                send_tftp_error(data_pcb, addr, port, 1, "Create file failed");
                osal_kfree(conn);
                udp_remove(data_pcb);
                pbuf_free(p);
                return;
            }
            udp_recv(data_pcb, tftp_data_recv_callback, conn);
            send_tftp_ack(data_pcb, addr, port, 0);
            printf("[TFTP] Start receiving file: %s\n", conn->filename);
        } else {
            // 读请求处理
            conn->mode = 1;
            conn->block_num = 1;
            conn->file = fs_adapt_open(conn->filename, O_RDONLY);
            if (conn->file < 0) {
				printf("[TFTP] File not found: %s\n", conn->filename);
                send_tftp_error(data_pcb, addr, port, 1, "File not found");
                osal_kfree(conn);
                udp_remove(data_pcb);
                pbuf_free(p);
                return;
            }
            udp_recv(data_pcb, tftp_ack_recv_callback, conn);
            send_tftp_data(conn); // 发送第一个数据块
            printf("[TFTP] Start sending file: %s\n", conn->filename);
        }
    }
    pbuf_free(p);
}

// 处理数据包（写请求）
static void tftp_data_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                   const ip_addr_t *addr, u16_t port) {
    (void)pcb;
    (void)addr;
    (void)port;

    tftp_connection *conn = (tftp_connection *)arg;
    if (!conn || !p || p->len < 4) {
        pbuf_free(p);
        return;
    }

    u16_t opcode = ntohs(*(u16_t *)p->payload);
    if (opcode == TFTP_OPCODE_DATA) {
        u16_t block_num = ntohs(*(u16_t *)((u8_t *)p->payload + 2));
        if (block_num == conn->block_num) {
            // 写入数据
            u8_t *data = (u8_t *)p->payload + 4;
            u16_t data_len = p->len - 4;
            int ret = fs_adapt_write(conn->file, (char *)data, data_len);
            if (ret != data_len) {
                printf("[TFTP] Write error data_len %d ret%d \n",data_len,ret);
                send_tftp_error(conn->pcb, NULL, 0, 3, "Write error");
                cleanup_connection(conn);
                pbuf_free(p);
                return;
            }

            // 发送ACK并更新块号
            send_tftp_ack(conn->pcb, NULL, 0, block_num);
            conn->block_num++;

            // 检查传输完成
            if (data_len < TFTP_BLOCK_SIZE) {
                cleanup_connection(conn);
            }
        } else {
            // 重发上一次的ACK
            send_tftp_ack(conn->pcb, NULL, 0, conn->block_num - 1);
        }
    }
    pbuf_free(p);
}

// 处理ACK包（读请求）
static void tftp_ack_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                  const ip_addr_t *addr, u16_t port) {
    (void)pcb;
    (void)addr;
    (void)port;

    tftp_connection *conn = (tftp_connection *)arg;
    if (!conn || !p || p->len < 4) {
        pbuf_free(p);
        return;
    }

    u16_t opcode = ntohs(*(u16_t *)p->payload);
    if (opcode == TFTP_OPCODE_ACK) {
        u16_t ack_block = ntohs(*(u16_t *)((u8_t *)p->payload + 2));
        if (ack_block == conn->block_num - 1) {
            if (conn->last_block) {
                cleanup_connection(conn);
            } else {
                send_tftp_data(conn);
            }
        }
    }
    pbuf_free(p);
}

// ================== 辅助函数 ==================
static void send_tftp_data(tftp_connection *conn) {
    char buffer[TFTP_BLOCK_SIZE];
    int bytes_read = fs_adapt_read(conn->file, buffer, TFTP_BLOCK_SIZE);
    
    // 错误处理
    if (bytes_read < 0) {
        send_tftp_error(conn->pcb, &conn->addr, conn->port, 0, "Read error");
        cleanup_connection(conn);
        return;
    }

    // 文件结束处理
    if (bytes_read == 0) {
        cleanup_connection(conn);
        return;
    }

    // 创建数据包
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 4 + bytes_read, PBUF_RAM);
    if (!p) {
        printf("[TFTP] Failed to allocate data packet\n");
        return;
    }

    // 填充数据包
    u16_t *payload = (u16_t *)p->payload;
    payload[0] = htons(TFTP_OPCODE_DATA);
    payload[1] = htons(conn->block_num);
    memcpy((u8_t *)payload + 4, buffer, bytes_read);

    // 发送数据包
    err_t err = udp_sendto(conn->pcb, p, &conn->addr, conn->port);
    pbuf_free(p);

    if (err != ERR_OK) {
        printf("[TFTP] Failed to send block %d\n", conn->block_num);
        cleanup_connection(conn);
        return;
    }

    // 更新块号和最后块标记
    if (bytes_read < TFTP_BLOCK_SIZE) {
        conn->last_block = 1;
    }
    conn->block_num++;
}

static void send_tftp_ack(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port, u16_t block_num) {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 4, PBUF_RAM);
    if (!p) {
        printf("ACK allocation failed\n");
        return;
    }

    u16_t *payload = (u16_t *)p->payload;
    payload[0] = htons(TFTP_OPCODE_ACK);
    payload[1] = htons(block_num);

    err_t err = addr ? udp_sendto(pcb, p, addr, port) : udp_send(pcb, p);
    if (err != ERR_OK) {
        printf("ACK send error: %d\n", err);
    }
    pbuf_free(p);
}

static void send_tftp_error(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port, 
                           u16_t code, const char *msg) {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 4 + strlen(msg) + 1, PBUF_RAM);
    if (!p) return;

    u16_t *payload = (u16_t *)p->payload;
    payload[0] = htons(TFTP_OPCODE_ERROR);
    payload[1] = htons(code);
    strcpy((char *)payload + 4, msg);

    if (addr && port) {
        udp_sendto(pcb, p, addr, port);
    } else {
        udp_send(pcb, p);
    }
    pbuf_free(p);
}

static void cleanup_connection(tftp_connection *conn) {
    if (conn) {
        if (conn->file >= 0) fs_adapt_close(conn->file);
        if (conn->pcb) udp_remove(conn->pcb);
        osal_kfree(conn);
    }
}

#endif

