/**
 * @file minimal_drop_detect.c
 * @brief LSM6DSV320X 掉落检测最小示例
 *
 * 这个文件演示了掉落检测的核心逻辑结构，包括：
 *   - 初始化（WHO_AM_I 验证、ODR / 量程配置、free-fall 和 high-g 配置）
 *   - 状态机轮询处理
 *   - 自由落体检测
 *   - 高 g 撞击检测
 *   - 跌落高度估算（h = 1/2 * g * t^2）
 *   - 撞击方向和静止朝向判断
 *
 * 注意：本文件不能直接编译为完整工程，需要：
 *   1. 提供板级 I2C/SPI 读写实现（见 platform_read / platform_write）
 *   2. 提供毫秒延时函数（见 platform_delay_ms）
 *   3. 包含 ST 官方驱动头文件 lsm6dsv320x_reg.h
 *      来源：https://github.com/STMicroelectronics/lsm6dsv320x-pid
 *
 * API 签名和结构体字段名均基于 ST 官方 PID 驱动实际代码验证：
 *   - lsm6dsv320x_data_rate_t 使用 LSM6DSV320X_ODR_AT_120Hz（无 XL_ 前缀）
 *   - pin_int1_route_t.freefall（无下划线）
 *   - all_sources_t.free_fall（有下划线）
 *   - hg_event_t.hg_wakeup / hg_wakeup_x / hg_wakeup_y / hg_wakeup_z
 *   - hg_wake_up_cfg_t.hg_wakeup_ths / hg_shock_dur
 *
 * 参考来源：
 *   STMicroelectronics/lsm6dsv320x-pid (PID 驱动)
 *   STMicroelectronics/X-CUBE-MEMS1
 *     Projects/.../HighGLowGFusion_LSM6DSV320X/ (应用示例)
 */

#include <stdint.h>
#include <string.h>
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
static int32_t platform_write(void *handle, uint8_t reg,
                               const uint8_t *buf, uint16_t len)
{
    /* TODO: 在这里实现 I2C/SPI 写操作
     * 例如 STM32 HAL:
     *   HAL_I2C_Mem_Write(&hi2c1, LSM6DSV320X_I2C_ADD_L, reg,
     *                     I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, len, 1000);
     */
    (void)handle; (void)reg; (void)buf; (void)len;
    return 0;
}

/**
 * @brief 通过 I2C 或 SPI 从传感器读取数据
 *        这里需要替换为你的板级实现
 */
static int32_t platform_read(void *handle, uint8_t reg,
                              uint8_t *buf, uint16_t len)
{
    /* TODO: 在这里实现 I2C/SPI 读操作
     * 例如 STM32 HAL:
     *   HAL_I2C_Mem_Read(&hi2c1, LSM6DSV320X_I2C_ADD_L, reg,
     *                    I2C_MEMADD_SIZE_8BIT, buf, len, 1000);
     */
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

/** 时间戳分辨率：1 LSB = 25 μs */
#define LSM6DSV320X_TIMESTAMP_LSB_US     (25.0f)
#define LSM6DSV320X_TIMESTAMP_LSB_SEC    (25e-6f)

/** 重力加速度 (m/s^2) */
#define GRAVITY_M_S2                     (9.8f)

/** 自由落体判定阈值（合加速度 < 此值认为失重，单位 mg） */
#define FREEFALL_MAG_THRESHOLD_MG        (300.0f)

/** 自由落体超时（如果在此时间内没有检测到撞击，复位状态机，单位 ms） */
#define FREEFALL_TIMEOUT_MS              (2000)

/** 撞击后等待静止的时间（ms） */
#define POST_IMPACT_SETTLE_MS            (100)

/** 方向字符串缓冲区大小 */
#define DIRECTION_STR_SIZE               (4)

/* ============================================================
 * 4. 全局变量
 * ============================================================ */

/** ST 官方驱动上下文，包含读写函数指针 */
static stmdev_ctx_t dev_ctx;

/** 掉落检测状态机 */
typedef enum {
    STATE_IDLE       = 0,  /**< 正常状态，等待自由落体 */
    STATE_FREE_FALL  = 1,  /**< 自由落体中，等待撞击 */
    STATE_IMPACT     = 2,  /**< 撞击发生，等待静止 */
    STATE_POST_REST  = 3,  /**< 撞击后静止，计算结果 */
} drop_state_t;

static drop_state_t drop_state = STATE_IDLE;

/** 时间戳记录 */
static uint32_t ts_freefall_start = 0;
static uint32_t ts_impact         = 0;

/** 撞击瞬间三轴高 g 数据（mg） */
static float impact_ax_mg = 0.0f;
static float impact_ay_mg = 0.0f;
static float impact_az_mg = 0.0f;

/** 最终结果 */
static float  result_height_m = 0.0f;
static char   result_impact_dir[DIRECTION_STR_SIZE];
static char   result_rest_orient[DIRECTION_STR_SIZE];

/* ============================================================
 * 5. 辅助函数
 * ============================================================ */

/** 计算三轴合向量大小 */
static float vector_magnitude(float x, float y, float z)
{
    return sqrtf(x * x + y * y + z * z);
}

/** 写入方向字符串，如 "+Z" */
static void set_direction(char *buf, char sign, char axis)
{
    buf[0] = sign;
    buf[1] = axis;
    buf[2] = '\0';
}

/** 找出三轴绝对值最大的轴并写入方向字符串 */
static void find_dominant_axis(float ax, float ay, float az, char *out)
{
    float abs_x = fabsf(ax);
    float abs_y = fabsf(ay);
    float abs_z = fabsf(az);

    if (abs_x >= abs_y && abs_x >= abs_z) {
        set_direction(out, ax > 0 ? '+' : '-', 'X');
    } else if (abs_y >= abs_x && abs_y >= abs_z) {
        set_direction(out, ay > 0 ? '+' : '-', 'Y');
    } else {
        set_direction(out, az > 0 ? '+' : '-', 'Z');
    }
}

/* ============================================================
 * 6. 初始化
 * ============================================================ */

/**
 * @brief 初始化传感器
 *
 * 完整流程：
 *   1. 绑定板级读写回调
 *   2. 验证 WHO_AM_I
 *   3. 软件复位
 *   4. 启用 BDU
 *   5. 配置低 g 加速度计（ODR、量程）
 *   6. 配置高 g 加速度计（ODR、量程）
 *   7. 配置自由落体检测（阈值、时间窗口）
 *   8. 配置高 g 唤醒检测（阈值、中断使能）
 *   9. 启用时间戳
 *  10. 配置 INT1 中断路由
 *
 * @return 0 成功，-1 芯片未识别
 */
int32_t drop_detect_init(void)
{
    uint8_t who_am_i;

    /* --- 1. 绑定板级读写函数 --- */
    dev_ctx.write_reg = platform_write;
    dev_ctx.read_reg  = platform_read;
    dev_ctx.mdelay    = platform_delay_ms;
    dev_ctx.handle    = NULL;  /* TODO: 替换为你的 I2C/SPI 句柄 */

    /* 等待传感器上电 */
    platform_delay_ms(10);

    /* --- 2. 验证 WHO_AM_I (0x0F → 0x73) --- */
    lsm6dsv320x_device_id_get(&dev_ctx, &who_am_i);
    if (who_am_i != LSM6DSV320X_ID) {
        return -1;  /* 芯片未正确识别 */
    }

    /* --- 3. 软件复位 --- */
    lsm6dsv320x_sw_reset(&dev_ctx);
    platform_delay_ms(10);

    /* --- 4. 启用 Block Data Update ---
     * 防止在读取过程中数据被更新 */
    lsm6dsv320x_block_data_update_set(&dev_ctx, 1);

    /* --- 5. 配置低 g 加速度计 ---
     * ODR = 120 Hz（注意：枚举值为 LSM6DSV320X_ODR_AT_120Hz，无 XL_ 前缀）
     * 量程 = ±4g */
    lsm6dsv320x_xl_data_rate_set(&dev_ctx, LSM6DSV320X_ODR_AT_120Hz);
    lsm6dsv320x_xl_full_scale_set(&dev_ctx, LSM6DSV320X_4g);

    /* --- 6. 配置高 g 加速度计 ---
     * ODR = 480 Hz
     * 量程 = ±320g */
    lsm6dsv320x_hg_xl_data_rate_set(&dev_ctx, LSM6DSV320X_HG_XL_ODR_AT_480Hz);
    lsm6dsv320x_hg_xl_full_scale_set(&dev_ctx, LSM6DSV320X_320g);

    /* --- 7. 配置自由落体检测 ---
     * 阈值 = 250 mg（合加速度低于此值触发）
     * 时间窗口 = 5 个 ODR 周期 */
    lsm6dsv320x_ff_thresholds_set(&dev_ctx, LSM6DSV320X_250_mg);
    lsm6dsv320x_ff_time_windows_set(&dev_ctx, 5);

    /* --- 8. 配置高 g 唤醒检测 --- */
    {
        lsm6dsv320x_hg_wake_up_cfg_t hg_cfg;
        memset(&hg_cfg, 0, sizeof(hg_cfg));
        hg_cfg.hg_wakeup_ths = 0x20;  /* 阈值，8 位，具体值参考数据手册 */
        hg_cfg.hg_shock_dur  = 0x01;  /* 冲击持续时间，4 位 */
        lsm6dsv320x_hg_wake_up_cfg_set(&dev_ctx, hg_cfg);

        lsm6dsv320x_hg_wu_interrupt_cfg_t hg_int_cfg;
        memset(&hg_int_cfg, 0, sizeof(hg_int_cfg));
        hg_int_cfg.hg_interrupts_enable = 1;
        lsm6dsv320x_hg_wu_interrupt_cfg_set(&dev_ctx, hg_int_cfg);
    }

    /* --- 9. 启用时间戳 --- */
    lsm6dsv320x_timestamp_set(&dev_ctx, 1);

    /* --- 10. 将 free-fall 事件路由到 INT1 引脚 ---
     * 注意：pin_int1_route_t 的字段名是 freefall（无下划线） */
    {
        lsm6dsv320x_pin_int1_route_t int1_route;
        memset(&int1_route, 0, sizeof(int1_route));
        int1_route.freefall = 1;
        lsm6dsv320x_pin_int1_route_set(&dev_ctx, &int1_route);
    }

    /* 初始化状态机 */
    drop_state = STATE_IDLE;
    memset(result_impact_dir,  0, sizeof(result_impact_dir));
    memset(result_rest_orient, 0, sizeof(result_rest_orient));
    result_height_m = 0.0f;

    return 0;
}

/* ============================================================
 * 7. 主处理函数（在主循环或定时回调中调用）
 * ============================================================ */

/**
 * @brief 掉落检测状态机处理
 *
 * 状态转换：
 *   IDLE → FREE_FALL → IMPACT → POST_REST → IDLE
 *
 * 在主循环或中断回调中定期调用此函数。
 */
void drop_detect_process(void)
{
    int16_t raw[3];
    float   ax_mg, ay_mg, az_mg;
    float   mag;

    switch (drop_state) {

    /* ----------------------------------------------------------
     * 状态 0：正常状态，检测自由落体
     * ---------------------------------------------------------- */
    case STATE_IDLE:
    {
        /* 方式一：使用 all_sources_get 检测（推荐） */
        lsm6dsv320x_all_sources_t sources;
        lsm6dsv320x_all_sources_get(&dev_ctx, &sources);

        if (sources.free_fall) {
            /* 芯片硬件检测到自由落体，记录时间戳 */
            lsm6dsv320x_timestamp_raw_get(&dev_ctx, &ts_freefall_start);
            drop_state = STATE_FREE_FALL;
            break;
        }

        /* 方式二：软件轮询合加速度作为备选 */
        lsm6dsv320x_acceleration_raw_get(&dev_ctx, raw);
        ax_mg = lsm6dsv320x_from_fs4_to_mg(raw[0]);
        ay_mg = lsm6dsv320x_from_fs4_to_mg(raw[1]);
        az_mg = lsm6dsv320x_from_fs4_to_mg(raw[2]);
        mag = vector_magnitude(ax_mg, ay_mg, az_mg);

        if (mag < FREEFALL_MAG_THRESHOLD_MG) {
            lsm6dsv320x_timestamp_raw_get(&dev_ctx, &ts_freefall_start);
            drop_state = STATE_FREE_FALL;
        }
        break;
    }

    /* ----------------------------------------------------------
     * 状态 1：自由落体中，等待撞击
     * ---------------------------------------------------------- */
    case STATE_FREE_FALL:
    {
        /* 检测高 g 撞击事件 */
        lsm6dsv320x_hg_event_t hg_event;
        lsm6dsv320x_hg_event_get(&dev_ctx, &hg_event);

        if (hg_event.hg_wakeup) {
            /* 记录撞击时间戳 */
            lsm6dsv320x_timestamp_raw_get(&dev_ctx, &ts_impact);

            /* 读取撞击瞬间高 g 三轴数据 */
            int16_t hg_raw[3];
            lsm6dsv320x_hg_acceleration_raw_get(&dev_ctx, hg_raw);
            impact_ax_mg = lsm6dsv320x_from_fs320_to_mg(hg_raw[0]);
            impact_ay_mg = lsm6dsv320x_from_fs320_to_mg(hg_raw[1]);
            impact_az_mg = lsm6dsv320x_from_fs320_to_mg(hg_raw[2]);

            drop_state = STATE_IMPACT;
            break;
        }

        /* 超时检测：如果长时间未检测到撞击，复位 */
        {
            uint32_t ts_now;
            lsm6dsv320x_timestamp_raw_get(&dev_ctx, &ts_now);
            float elapsed_ms = (float)(ts_now - ts_freefall_start)
                               * LSM6DSV320X_TIMESTAMP_LSB_US / 1000.0f;
            if (elapsed_ms > (float)FREEFALL_TIMEOUT_MS) {
                drop_state = STATE_IDLE;
            }
        }
        break;
    }

    /* ----------------------------------------------------------
     * 状态 2：撞击发生，等待静止
     * ---------------------------------------------------------- */
    case STATE_IMPACT:
        platform_delay_ms(POST_IMPACT_SETTLE_MS);
        drop_state = STATE_POST_REST;
        break;

    /* ----------------------------------------------------------
     * 状态 3：撞击后静止，计算跌落高度和方向
     * ---------------------------------------------------------- */
    case STATE_POST_REST:
    {
        /* --- 计算近似跌落高度 ---
         * h = (1/2) * g * t^2
         * 时间戳分辨率：1 LSB = 25 μs
         */
        uint32_t ts_diff = ts_impact - ts_freefall_start;
        float fall_sec = (float)ts_diff * LSM6DSV320X_TIMESTAMP_LSB_SEC;
        result_height_m = 0.5f * GRAVITY_M_S2 * fall_sec * fall_sec;

        /* --- 判断主撞击方向 ---
         * 找出撞击瞬间三轴中绝对值最大的轴 */
        find_dominant_axis(impact_ax_mg, impact_ay_mg, impact_az_mg,
                           result_impact_dir);

        /* --- 判断静止后的朝向 ---
         * 读取低 g 三轴，看哪个轴受重力分量最大 */
        lsm6dsv320x_acceleration_raw_get(&dev_ctx, raw);
        ax_mg = lsm6dsv320x_from_fs4_to_mg(raw[0]);
        ay_mg = lsm6dsv320x_from_fs4_to_mg(raw[1]);
        az_mg = lsm6dsv320x_from_fs4_to_mg(raw[2]);
        find_dominant_axis(ax_mg, ay_mg, az_mg, result_rest_orient);

        /* 结果已就绪，回到待机状态 */
        drop_state = STATE_IDLE;
        break;
    }

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
 * @param height_m    [OUT] 近似跌落高度（米），可为 NULL
 * @param impact_dir  [OUT] 主撞击方向字符串指针（如 "+Z"），可为 NULL
 * @param rest_orient [OUT] 静止后朝向字符串指针（如 "-Z"），可为 NULL
 */
void drop_detect_get_result(float *height_m,
                             const char **impact_dir,
                             const char **rest_orient)
{
    if (height_m    != NULL) *height_m    = result_height_m;
    if (impact_dir  != NULL) *impact_dir  = result_impact_dir;
    if (rest_orient != NULL) *rest_orient = result_rest_orient;
}

/**
 * @brief 获取当前状态机状态（用于调试）
 * @return 当前状态枚举值
 */
drop_state_t drop_detect_get_state(void)
{
    return drop_state;
}

/* ============================================================
 * 9. 主函数示例（轮询模式）
 * ============================================================ */

/*
 * 以下是一个典型的 main 函数框架，展示如何调用上述接口。
 * 取消注释后可以作为你工程的起点。
 *
 * int main(void)
 * {
 *     // TODO: 初始化你的硬件平台（时钟、GPIO、I2C/SPI、中断等）
 *     // 例如 STM32: HAL_Init(); SystemClock_Config(); MX_I2C1_Init();
 *
 *     if (drop_detect_init() != 0) {
 *         // 芯片初始化失败，检查接线和 I2C 地址
 *         while (1);
 *     }
 *
 *     while (1) {
 *         drop_detect_process();
 *
 *         // 检查是否有新的检测结果
 *         float h;
 *         const char *impact, *orient;
 *         drop_detect_get_result(&h, &impact, &orient);
 *
 *         if (h > 0.0f) {
 *             // 有新结果，输出到串口
 *             printf("Drop detected!\n");
 *             printf("  Height:    %.2f m\n", h);
 *             printf("  Impact:    %s\n", impact);
 *             printf("  Rest face: %s\n", orient);
 *         }
 *
 *         platform_delay_ms(10);
 *     }
 * }
 */
