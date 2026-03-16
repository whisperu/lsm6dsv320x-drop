/**
 * @file minimal_drop_detect.c
 * @brief LSM6DSV320X 掉落检测最小示例
 *
 * 这个文件演示了掉落检测的核心逻辑结构，包括：
 *   - 初始化
 *   - 轮询/中断处理
 *   - 自由落体检测
 *   - 撞击检测
 *   - 跌落高度估算
 *   - 撞击方向和静止朝向判断
 *
 * 注意：本文件不能直接编译为完整工程，需要：
 *   1. 提供板级 I2C/SPI 读写实现（见 platform_read / platform_write）
 *   2. 提供毫秒延时函数（见 platform_delay_ms）
 *   3. 包含 ST 官方驱动头文件 lsm6dsv320x_reg.h
 *
 * 参考来源：STMicroelectronics/X-CUBE-MEMS1
 *   Projects/NUCLEO-F401RE/Applications/CUSTOM/HighGLowGFusion_LSM6DSV320X/
 */

#include <stdint.h>
#include <math.h>

/* ============================================================
 * 1. 包含 ST 官方驱动头文件
 *    实际使用时请确认路径正确
 * ============================================================ */
#include "lsm6dsv320x_reg.h"

/* ============================================================
 * 2. 板级接口声明（需要根据你的硬件实现）
 * ============================================================ */

/**
 * @brief 通过 I2C 或 SPI 向传感器写入数据
 *        这里需要替换为你的板级实现
 */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *buf, uint16_t len)
{
    /* TODO: 在这里实现 I2C/SPI 写操作 */
    /* 例如：HAL_I2C_Mem_Write(handle, LSM6DSV320X_I2C_ADDR, reg, ...) */
    (void)handle; (void)reg; (void)buf; (void)len;
    return 0;
}

/**
 * @brief 通过 I2C 或 SPI 从传感器读取数据
 *        这里需要替换为你的板级实现
 */
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
    /* TODO: 在这里实现 I2C/SPI 读操作 */
    /* 例如：HAL_I2C_Mem_Read(handle, LSM6DSV320X_I2C_ADDR, reg, ...) */
    (void)handle; (void)reg; (void)buf; (void)len;
    return 0;
}

/**
 * @brief 延时函数，单位：毫秒
 *        这里需要替换为你的板级实现
 */
static void platform_delay_ms(uint32_t ms)
{
    /* TODO: 在这里实现毫秒延时，例如 HAL_Delay(ms) */
    (void)ms;
}

/* ============================================================
 * 3. 常量定义
 * ============================================================ */

/* 时间戳分辨率：1 LSB = 25 μs */
#define LSM6DSV320X_TIMESTAMP_RESOLUTION_S  (25e-6f)

/* 方向字符串缓冲区大小（符号 + 轴名 + 空终止符，例如 "+Z\0"） */
#define DIRECTION_STRING_SIZE  8

/* ============================================================
 * 4. 全局变量
 * ============================================================ */

/* ST 官方驱动上下文，包含读写函数指针 */
static stmdev_ctx_t dev_ctx;

/* 掉落检测状态机 */
typedef enum {
    STATE_IDLE       = 0,  /* 正常状态，等待自由落体 */
    STATE_FREE_FALL  = 1,  /* 自由落体中，等待撞击 */
    STATE_IMPACT     = 2,  /* 撞击发生，等待静止 */
    STATE_POST_REST  = 3,  /* 撞击后静止，计算结果 */
} DropState_t;

static DropState_t drop_state = STATE_IDLE;

/* 时间戳记录 */
static uint32_t ts_freefall_start = 0;  /* 进入自由落体时的时间戳 */
static uint32_t ts_impact         = 0;  /* 撞击发生时的时间戳 */

/* 撞击瞬间三轴高 g 数据（mg） */
static float impact_ax_mg = 0.0f;
static float impact_ay_mg = 0.0f;
static float impact_az_mg = 0.0f;

/* 最终结果 */
static float  result_drop_height_m   = 0.0f;  /* 近似跌落高度（米） */
static char   result_impact_direction[DIRECTION_STRING_SIZE] = {0};  /* 主撞击方向，如 "+Z" */
static char   result_rest_orientation[DIRECTION_STRING_SIZE] = {0};  /* 静止后朝上的轴，如 "+Z" */

/* ============================================================
 * 5. 初始化
 * ============================================================ */

/**
 * @brief 初始化传感器，配置自由落体检测和高 g 检测
 */
void drop_detect_init(void)
{
    /* 绑定板级读写函数到驱动上下文 */
    dev_ctx.write_reg = platform_write;
    dev_ctx.read_reg  = platform_read;
    dev_ctx.mdelay    = platform_delay_ms;
    /* TODO: dev_ctx.handle 指向你的 I2C/SPI 句柄，例如 &hi2c1 */
    dev_ctx.handle    = NULL;

    /* 等待传感器启动 */
    platform_delay_ms(10);

    /* --- 配置加速度计 ODR（输出数据率） ---
     * 这里设置低 g 加速度计为 104 Hz
     * 具体枚举值参考 lsm6dsv320x_reg.h */
    lsm6dsv320x_xl_data_rate_set(&dev_ctx, LSM6DSV320X_XL_ODR_AT_104Hz);

    /* --- 配置低 g 量程（±4g） ---
     * 自由落体和静止姿态判断用 */
    lsm6dsv320x_xl_full_scale_set(&dev_ctx, LSM6DSV320X_4g);

    /* --- 配置自由落体阈值（250 mg）---
     * 合加速度低于此值认为进入自由落体 */
    lsm6dsv320x_ff_thresholds_set(&dev_ctx, LSM6DSV320X_250_mg);

    /* --- 配置自由落体持续时间（5 个 ODR 周期）---
     * 防止短暂抖动误触发 */
    lsm6dsv320x_ff_time_windows_set(&dev_ctx, 5);

    /* --- 配置高 g 唤醒阈值 ---
     * 具体寄存器值请参考数据手册换算 */
    lsm6dsv320x_hg_wake_up_cfg_t hg_cfg = {0};
    hg_cfg.wake_ths = 0x20;
    lsm6dsv320x_hg_wake_up_cfg_set(&dev_ctx, hg_cfg);

    /* --- 启用时间戳 --- */
    lsm6dsv320x_timestamp_set(&dev_ctx, 1);

    /* --- 将自由落体事件路由到 INT1 引脚（可选）---
     * 如果你使用轮询方式，这步可以跳过 */
    lsm6dsv320x_pin_int1_route_t int1_route = {0};
    int1_route.free_fall = 1;
    lsm6dsv320x_pin_int1_route_set(&dev_ctx, &int1_route);

    drop_state = STATE_IDLE;
}

/* ============================================================
 * 6. 辅助函数：合加速度计算
 * ============================================================ */

/**
 * @brief 计算三轴加速度的合向量大小（mg）
 */
static float magnitude_mg(float ax, float ay, float az)
{
    return sqrtf(ax * ax + ay * ay + az * az);
}

/**
 * @brief 将符号和轴名写入方向字符串缓冲区
 *        例如 set_direction(buf, '+', 'Z') → buf = "+Z"
 */
static void set_direction(char *dest, char sign, char axis)
{
    dest[0] = sign;
    dest[1] = axis;
    dest[2] = '\0';
}

/* ============================================================
 * 7. 主处理函数（轮询或中断回调中调用）
 * ============================================================ */

/**
 * @brief 掉落检测主处理函数
 *        在主循环或中断回调中定期调用
 */
void drop_detect_process(void)
{
    int16_t raw[3];
    float ax_mg, ay_mg, az_mg;
    float mag;

    switch (drop_state) {

    /* ----------------------------------------------------------
     * 状态 0：正常状态，检测自由落体
     * ---------------------------------------------------------- */
    case STATE_IDLE:
        /* 读取低 g 三轴原始数据 */
        lsm6dsv320x_acceleration_raw_get(&dev_ctx, raw);
        ax_mg = lsm6dsv320x_from_fs4_to_mg(raw[0]);
        ay_mg = lsm6dsv320x_from_fs4_to_mg(raw[1]);
        az_mg = lsm6dsv320x_from_fs4_to_mg(raw[2]);

        /* 计算合加速度 */
        mag = magnitude_mg(ax_mg, ay_mg, az_mg);

        /* 判断是否进入自由落体（合加速度低于 300 mg） */
        if (mag < 300.0f) {
            /* 记录进入失重时的时间戳 */
            lsm6dsv320x_timestamp_raw_get(&dev_ctx, &ts_freefall_start);
            drop_state = STATE_FREE_FALL;
        }
        break;

    /* ----------------------------------------------------------
     * 状态 1：自由落体中，等待撞击
     * ---------------------------------------------------------- */
    case STATE_FREE_FALL:
        /* 读取低 g 三轴，确认是否还在失重状态 */
        lsm6dsv320x_acceleration_raw_get(&dev_ctx, raw);
        ax_mg = lsm6dsv320x_from_fs4_to_mg(raw[0]);
        ay_mg = lsm6dsv320x_from_fs4_to_mg(raw[1]);
        az_mg = lsm6dsv320x_from_fs4_to_mg(raw[2]);
        mag = magnitude_mg(ax_mg, ay_mg, az_mg);

        /* 如果合加速度恢复到 1g 附近，可能没有发生撞击（误触发），复位 */
        if (mag > 800.0f && mag < 1200.0f) {
            drop_state = STATE_IDLE;
            break;
        }

        /* 检测高 g 撞击事件 */
        lsm6dsv320x_hg_wu_event_t hg_event;
        lsm6dsv320x_hg_event_get(&dev_ctx, &hg_event);

        if (hg_event.wake_up) {
            /* 记录撞击时刻时间戳 */
            lsm6dsv320x_timestamp_raw_get(&dev_ctx, &ts_impact);

            /* 读取撞击瞬间高 g 三轴数据 */
            int16_t hg_raw[3];
            lsm6dsv320x_hg_acceleration_raw_get(&dev_ctx, hg_raw);
            impact_ax_mg = lsm6dsv320x_from_fs320_to_mg(hg_raw[0]);
            impact_ay_mg = lsm6dsv320x_from_fs320_to_mg(hg_raw[1]);
            impact_az_mg = lsm6dsv320x_from_fs320_to_mg(hg_raw[2]);

            drop_state = STATE_IMPACT;
        }
        break;

    /* ----------------------------------------------------------
     * 状态 2：撞击发生，等待静止（约 100 ms）
     * ---------------------------------------------------------- */
    case STATE_IMPACT:
        /* 等待撞击后传感器趋于静止 */
        platform_delay_ms(100);
        drop_state = STATE_POST_REST;
        break;

    /* ----------------------------------------------------------
     * 状态 3：撞击后静止，计算跌落高度和方向
     * ---------------------------------------------------------- */
    case STATE_POST_REST:
        /* --- 计算近似跌落高度 ---
         *
         * 原理：自由落体 h = (1/2) * g * t^2
         * 时间戳分辨率：1 LSB = 25 μs
         *
         * 注意：这只是近似估算，误差来源较多（见 docs/drop-detection-plan.md）
         */
        {
            uint32_t ts_diff = ts_impact - ts_freefall_start;
            float fall_duration_s = (float)ts_diff * LSM6DSV320X_TIMESTAMP_RESOLUTION_S;  /* 单位：秒 */
            result_drop_height_m = 0.5f * 9.8f * fall_duration_s * fall_duration_s;
        }

        /* --- 判断主撞击方向 ---
         *
         * 找出撞击瞬间三轴中绝对值最大的轴
         */
        {
            float abs_x = fabsf(impact_ax_mg);
            float abs_y = fabsf(impact_ay_mg);
            float abs_z = fabsf(impact_az_mg);

            if (abs_x >= abs_y && abs_x >= abs_z) {
                /* X 轴为主撞击方向 */
                set_direction(result_impact_direction, impact_ax_mg > 0 ? '+' : '-', 'X');
            } else if (abs_y >= abs_x && abs_y >= abs_z) {
                /* Y 轴为主撞击方向 */
                set_direction(result_impact_direction, impact_ay_mg > 0 ? '+' : '-', 'Y');
            } else {
                /* Z 轴为主撞击方向 */
                set_direction(result_impact_direction, impact_az_mg > 0 ? '+' : '-', 'Z');
            }
        }

        /* --- 判断静止后的朝向 ---
         *
         * 读取低 g 三轴，看哪个轴受重力分量最大
         * 静止时哪轴绝对值接近 1000 mg，说明那个轴竖直
         */
        {
            lsm6dsv320x_acceleration_raw_get(&dev_ctx, raw);
            ax_mg = lsm6dsv320x_from_fs4_to_mg(raw[0]);
            ay_mg = lsm6dsv320x_from_fs4_to_mg(raw[1]);
            az_mg = lsm6dsv320x_from_fs4_to_mg(raw[2]);

            float abs_x = fabsf(ax_mg);
            float abs_y = fabsf(ay_mg);
            float abs_z = fabsf(az_mg);

            if (abs_x >= abs_y && abs_x >= abs_z) {
                set_direction(result_rest_orientation, ax_mg > 0 ? '+' : '-', 'X');
            } else if (abs_y >= abs_x && abs_y >= abs_z) {
                set_direction(result_rest_orientation, ay_mg > 0 ? '+' : '-', 'Y');
            } else {
                set_direction(result_rest_orientation, az_mg > 0 ? '+' : '-', 'Z');
            }
        }

        /* 结果已就绪，回到待机状态 */
        drop_state = STATE_IDLE;
        break;

    default:
        drop_state = STATE_IDLE;
        break;
    }
}

/* ============================================================
 * 8. 结果读取
 * ============================================================ */

/**
 * @brief 读取最近一次掉落检测结果
 *
 * @param height_m         [OUT] 近似跌落高度（米）
 * @param impact_dir       [OUT] 主撞击方向字符串，如 "+Z"
 * @param rest_orient      [OUT] 静止后朝向字符串，如 "-Z"
 */
void drop_detect_get_result(float *height_m,
                             const char **impact_dir,
                             const char **rest_orient)
{
    if (height_m)    *height_m    = result_drop_height_m;
    if (impact_dir)  *impact_dir  = result_impact_direction;
    if (rest_orient) *rest_orient = result_rest_orientation;
}

/* ============================================================
 * 9. 主函数示例（轮询模式）
 * ============================================================ */

/*
int main(void)
{
    // TODO: 初始化你的硬件平台（GPIO、I2C/SPI、中断等）

    drop_detect_init();

    while (1) {
        drop_detect_process();

        // TODO: 在这里添加你的主循环逻辑
        // 例如：读取结果并通过串口输出
        // float h;
        // const char *impact, *orient;
        // drop_detect_get_result(&h, &impact, &orient);
        // printf("height=%.2fm impact=%s rest=%s\n", h, impact, orient);

        // TODO: 插入适当延时或等待中断
        platform_delay_ms(10);
    }
}
*/
