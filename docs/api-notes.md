# LSM6DSV320X 掉落检测相关 API 说明

本文档从 ST 官方驱动中提取与掉落检测最相关的 API，适合初学者快速上手。

> 驱动来源：`STMicroelectronics/lsm6dsv320x-pid`
> 文件：`lsm6dsv320x_reg.c` / `lsm6dsv320x_reg.h`

---

## 1. 驱动上下文（stmdev_ctx_t）

所有 API 都需要传入一个 `stmdev_ctx_t` 上下文结构体：

```c
typedef struct {
    stmdev_write_ptr  write_reg;  /* 必须：I2C/SPI 写回调 */
    stmdev_read_ptr   read_reg;   /* 必须：I2C/SPI 读回调 */
    stmdev_mdelay_ptr mdelay;     /* 可选：毫秒延时回调 */
    void             *handle;     /* 自定义句柄，例如 &hi2c1 */
    void             *priv_data;  /* 私有数据 */
} stmdev_ctx_t;
```

**初始化示例**：
```c
stmdev_ctx_t dev_ctx;
dev_ctx.write_reg = platform_write;
dev_ctx.read_reg  = platform_read;
dev_ctx.mdelay    = platform_delay_ms;
dev_ctx.handle    = &hi2c1;  /* 你的 I2C 句柄 */
```

---

## 2. 设备识别与复位

### 2.1 读取设备 ID

```c
int32_t lsm6dsv320x_device_id_get(const stmdev_ctx_t *ctx, uint8_t *val);
```

**作用**：读取 WHO_AM_I 寄存器（地址 0x0F），验证芯片是否正确连接。

**使用示例**：
```c
uint8_t who_am_i;
lsm6dsv320x_device_id_get(&dev_ctx, &who_am_i);
if (who_am_i != LSM6DSV320X_ID) {  /* LSM6DSV320X_ID = 0x73 */
    /* 芯片未正确识别，检查接线 */
}
```

### 2.2 软件复位

```c
int32_t lsm6dsv320x_sw_reset(const stmdev_ctx_t *ctx);
```

**作用**：执行软件复位，将所有寄存器恢复为默认值。

---

## 3. 自由落体（Free-fall）相关

### 3.1 设置自由落体阈值

```c
int32_t lsm6dsv320x_ff_thresholds_set(const stmdev_ctx_t *ctx,
                                       lsm6dsv320x_ff_thresholds_t val);
```

**作用**：设置合加速度低于多少 mg 时，认为进入自由落体。

**常用参数值**（`lsm6dsv320x_ff_thresholds_t` 枚举）：

| 枚举值 | 对应阈值 | 寄存器值 |
|--------|---------|----------|
| `LSM6DSV320X_156_mg` | 156 mg | 0x0 |
| `LSM6DSV320X_219_mg` | 219 mg | 0x1 |
| `LSM6DSV320X_250_mg` | 250 mg | 0x2 |
| `LSM6DSV320X_312_mg` | 312 mg | 0x3 |
| `LSM6DSV320X_344_mg` | 344 mg | 0x4 |
| `LSM6DSV320X_406_mg` | 406 mg | 0x5 |
| `LSM6DSV320X_469_mg` | 469 mg | 0x6 |
| `LSM6DSV320X_500_mg` | 500 mg | 0x7 |

**使用示例**：
```c
lsm6dsv320x_ff_thresholds_set(&dev_ctx, LSM6DSV320X_250_mg);
```

---

### 3.2 设置自由落体持续时间窗口

```c
int32_t lsm6dsv320x_ff_time_windows_set(const stmdev_ctx_t *ctx, uint8_t val);
```

**作用**：合加速度低于阈值需持续多少个 ODR（采样周期）才触发事件。

**说明**：防止短暂抖动被误判为自由落体。一般设置 4～6 个采样周期即可。

**使用示例**：
```c
lsm6dsv320x_ff_time_windows_set(&dev_ctx, 5);
```

---

## 4. 高 g 冲击（High-g / Impact）相关

### 4.1 配置高 g 通道 ODR 和量程

在使用高 g 功能之前，必须先启用高 g 加速度计：

```c
/* 设置高 g 加速度计数据输出速率（如 480 Hz） */
int32_t lsm6dsv320x_hg_xl_data_rate_set(const stmdev_ctx_t *ctx,
                                          lsm6dsv320x_hg_xl_data_rate_t val);

/* 设置高 g 加速度计量程（如 ±320g） */
int32_t lsm6dsv320x_hg_xl_full_scale_set(const stmdev_ctx_t *ctx,
                                           lsm6dsv320x_hg_xl_full_scale_t val);
```

**高 g ODR 可选值**（`lsm6dsv320x_hg_xl_data_rate_t`）：

| 枚举值 | 频率 |
|--------|------|
| `LSM6DSV320X_HG_XL_ODR_OFF` | 关闭 |
| `LSM6DSV320X_HG_XL_ODR_AT_480Hz` | 480 Hz |
| `LSM6DSV320X_HG_XL_ODR_AT_960Hz` | 960 Hz |
| `LSM6DSV320X_HG_XL_ODR_AT_1920Hz` | 1920 Hz |
| `LSM6DSV320X_HG_XL_ODR_AT_3840Hz` | 3840 Hz |
| `LSM6DSV320X_HG_XL_ODR_AT_7680Hz` | 7680 Hz |

**高 g 量程可选值**（`lsm6dsv320x_hg_xl_full_scale_t`）：

| 枚举值 | 量程 |
|--------|------|
| `LSM6DSV320X_32g` | ±32g |
| `LSM6DSV320X_64g` | ±64g |
| `LSM6DSV320X_128g` | ±128g |
| `LSM6DSV320X_256g` | ±256g |
| `LSM6DSV320X_320g` | ±320g |

**使用示例**：
```c
lsm6dsv320x_hg_xl_data_rate_set(&dev_ctx, LSM6DSV320X_HG_XL_ODR_AT_480Hz);
lsm6dsv320x_hg_xl_full_scale_set(&dev_ctx, LSM6DSV320X_320g);
```

---

### 4.2 配置高 g 唤醒参数

```c
int32_t lsm6dsv320x_hg_wake_up_cfg_set(const stmdev_ctx_t *ctx,
                                        lsm6dsv320x_hg_wake_up_cfg_t val);
```

**作用**：配置高 g 通道的唤醒阈值和冲击持续时间。

**结构体字段**（`lsm6dsv320x_hg_wake_up_cfg_t`）：

| 字段 | 位宽 | 说明 |
|------|------|------|
| `hg_wakeup_ths` | 8 位 | 唤醒阈值（值越大阈值越高） |
| `hg_shock_dur` | 4 位 | 冲击持续时间（防抖） |

**使用示例**：
```c
lsm6dsv320x_hg_wake_up_cfg_t hg_cfg = {0};
hg_cfg.hg_wakeup_ths = 0x20;  /* 具体值参考数据手册换算 */
hg_cfg.hg_shock_dur  = 0x01;
lsm6dsv320x_hg_wake_up_cfg_set(&dev_ctx, hg_cfg);
```

---

### 4.3 启用高 g 中断

```c
int32_t lsm6dsv320x_hg_wu_interrupt_cfg_set(const stmdev_ctx_t *ctx,
                                             lsm6dsv320x_hg_wu_interrupt_cfg_t val);
```

**结构体字段**（`lsm6dsv320x_hg_wu_interrupt_cfg_t`）：

| 字段 | 说明 |
|------|------|
| `hg_interrupts_enable` | 1：启用高 g 中断 |
| `hg_wakeup_int_sel` | 中断选择 |

---

### 4.4 读取高 g 事件状态

```c
int32_t lsm6dsv320x_hg_event_get(const stmdev_ctx_t *ctx,
                                  lsm6dsv320x_hg_event_t *val);
```

**作用**：读取当前高 g 事件状态，判断是否有撞击/唤醒事件。

**结构体字段**（`lsm6dsv320x_hg_event_t`）：

| 字段 | 说明 |
|------|------|
| `hg_event` | 1：有高 g 事件 |
| `hg_wakeup` | 1：有高 g 唤醒事件 |
| `hg_wakeup_x` | 1：X 轴触发唤醒 |
| `hg_wakeup_y` | 1：Y 轴触发唤醒 |
| `hg_wakeup_z` | 1：Z 轴触发唤醒 |
| `hg_wakeup_chg` | 1：唤醒状态变化 |
| `hg_shock` | 1：冲击事件 |
| `hg_shock_change` | 1：冲击状态变化 |

**使用示例**：
```c
lsm6dsv320x_hg_event_t hg_event;
lsm6dsv320x_hg_event_get(&dev_ctx, &hg_event);
if (hg_event.hg_wakeup) {
    /* 发生了高 g 撞击事件 */
    /* 可以进一步检查 hg_event.hg_wakeup_x / y / z */
}
```

---

## 5. 中断路由

### 5.1 将事件路由到 INT1 引脚

```c
int32_t lsm6dsv320x_pin_int1_route_set(const stmdev_ctx_t *ctx,
                                        lsm6dsv320x_pin_int1_route_t *val);
```

**作用**：选择哪些事件通过 INT1 引脚触发外部中断。

**结构体主要字段（掉落检测常用）**（`lsm6dsv320x_pin_int1_route_t`）：

| 字段 | 说明 |
|------|------|
| `drdy_xl` | 1：低 g 加速度数据就绪 |
| `freefall` | 1：自由落体事件（**注意：没有下划线**） |
| `wakeup` | 1：低 g 唤醒事件 |
| `sixd` | 1：6D 方向变化 |
| `single_tap` | 1：单击事件 |
| `double_tap` | 1：双击事件 |

**使用示例**：
```c
lsm6dsv320x_pin_int1_route_t int1_route = {0};
int1_route.freefall = 1;  /* 自由落体触发 INT1 */
lsm6dsv320x_pin_int1_route_set(&dev_ctx, &int1_route);
```

### 5.2 将高 g 事件路由到 INT1

```c
int32_t lsm6dsv320x_pin_int1_route_hg_set(const stmdev_ctx_t *ctx,
                                            lsm6dsv320x_pin_int1_route_hg_t *val);
```

**作用**：将高 g 相关事件单独路由到 INT1 引脚。

---

## 6. 统一事件源查询

### 6.1 读取所有事件源

```c
int32_t lsm6dsv320x_all_sources_get(const stmdev_ctx_t *ctx,
                                     lsm6dsv320x_all_sources_t *val);
```

**作用**：一次性读取芯片所有事件状态。推荐用于轮询模式。

**掉落检测相关字段**（`lsm6dsv320x_all_sources_t`）：

| 字段 | 说明 |
|------|------|
| `free_fall` | 1：自由落体事件（**注意：有下划线**） |
| `hg` | 1：高 g 事件 |
| `wake_up` | 1：低 g 唤醒事件 |
| `wake_up_x/y/z` | 各轴唤醒状态 |
| `drdy_xl` | 低 g 加速度数据就绪 |
| `drdy_xlhgda` | 高 g 加速度数据就绪 |
| `timestamp` | 时间戳事件 |
| `fsm1` ~ `fsm8` | FSM 各通道输出 |

**使用示例**：
```c
lsm6dsv320x_all_sources_t sources;
lsm6dsv320x_all_sources_get(&dev_ctx, &sources);
if (sources.free_fall) {
    /* 检测到自由落体 */
}
if (sources.hg) {
    /* 检测到高 g 事件 */
}
```

> **提示**：`pin_int1_route_t.freefall`（无下划线）和 `all_sources_t.free_fall`（有下划线）命名不一致，这是 ST 驱动代码的实际情况，请注意区分。

---

## 7. 时间戳

### 7.1 启用时间戳

```c
int32_t lsm6dsv320x_timestamp_set(const stmdev_ctx_t *ctx, uint8_t val);
```

**作用**：开启或关闭芯片内部时间戳功能。

**使用示例**：
```c
lsm6dsv320x_timestamp_set(&dev_ctx, 1);  /* 1 = 启用 */
```

---

### 7.2 读取时间戳原始值

```c
int32_t lsm6dsv320x_timestamp_raw_get(const stmdev_ctx_t *ctx, uint32_t *val);
```

**作用**：读取当前时间戳计数值（4 字节，来自 TIMESTAMP0~TIMESTAMP3 寄存器）。

**换算关系**：
- 1 LSB = 25 μs（微秒）
- `时间（秒）= 原始值 × 25e-6`

**使用示例**：
```c
uint32_t ts_raw;
lsm6dsv320x_timestamp_raw_get(&dev_ctx, &ts_raw);
float time_sec = (float)ts_raw * 25e-6f;
```

---

### 7.3 时间戳 LSB 换算

```c
uint64_t lsm6dsv320x_from_lsb_to_nsec(uint32_t lsb);
```

**作用**：将时间戳原始 LSB 值换算为纳秒。返回类型为 `uint64_t`。

---

## 8. 原始加速度读取

### 8.1 低 g 加速度通道

```c
int32_t lsm6dsv320x_acceleration_raw_get(const stmdev_ctx_t *ctx, int16_t *val);
```

**作用**：读取低 g 加速度通道三轴原始数据（`int16_t[3]`）。

**配套换算函数**：

| 函数 | 量程 | 灵敏度 |
|------|------|--------|
| `lsm6dsv320x_from_fs2_to_mg()` | ±2g | 0.061 mg/LSB |
| `lsm6dsv320x_from_fs4_to_mg()` | ±4g | 0.122 mg/LSB |
| `lsm6dsv320x_from_fs8_to_mg()` | ±8g | 0.244 mg/LSB |
| `lsm6dsv320x_from_fs16_to_mg()` | ±16g | 0.488 mg/LSB |

**使用示例**：
```c
int16_t raw[3];
lsm6dsv320x_acceleration_raw_get(&dev_ctx, raw);

float ax_mg = lsm6dsv320x_from_fs4_to_mg(raw[0]);
float ay_mg = lsm6dsv320x_from_fs4_to_mg(raw[1]);
float az_mg = lsm6dsv320x_from_fs4_to_mg(raw[2]);
```

---

### 8.2 高 g 加速度通道

```c
int32_t lsm6dsv320x_hg_acceleration_raw_get(const stmdev_ctx_t *ctx, int16_t *val);
```

**作用**：读取高 g 加速度通道三轴原始数据，适合撞击时读取。

**配套换算函数**：

| 函数 | 量程 | 灵敏度 |
|------|------|--------|
| `lsm6dsv320x_from_fs32_to_mg()` | ±32g | — |
| `lsm6dsv320x_from_fs64_to_mg()` | ±64g | — |
| `lsm6dsv320x_from_fs128_to_mg()` | ±128g | — |
| `lsm6dsv320x_from_fs256_to_mg()` | ±256g | — |
| `lsm6dsv320x_from_fs320_to_mg()` | ±320g | 10.417 mg/LSB |

**使用示例**：
```c
int16_t hg_raw[3];
lsm6dsv320x_hg_acceleration_raw_get(&dev_ctx, hg_raw);

float hg_ax = lsm6dsv320x_from_fs320_to_mg(hg_raw[0]);
float hg_ay = lsm6dsv320x_from_fs320_to_mg(hg_raw[1]);
float hg_az = lsm6dsv320x_from_fs320_to_mg(hg_raw[2]);
```

---

## 9. 数据输出速率和量程配置

### 9.1 低 g 加速度计

```c
/* 设置输出数据速率 */
int32_t lsm6dsv320x_xl_data_rate_set(const stmdev_ctx_t *ctx,
                                      lsm6dsv320x_data_rate_t val);

/* 设置量程 */
int32_t lsm6dsv320x_xl_full_scale_set(const stmdev_ctx_t *ctx,
                                       lsm6dsv320x_xl_full_scale_t val);
```

**低 g ODR 常用值**（`lsm6dsv320x_data_rate_t`）：

| 枚举值 | 频率 |
|--------|------|
| `LSM6DSV320X_ODR_OFF` | 关闭 |
| `LSM6DSV320X_ODR_AT_7Hz5` | 7.5 Hz |
| `LSM6DSV320X_ODR_AT_15Hz` | 15 Hz |
| `LSM6DSV320X_ODR_AT_30Hz` | 30 Hz |
| `LSM6DSV320X_ODR_AT_60Hz` | 60 Hz |
| `LSM6DSV320X_ODR_AT_120Hz` | 120 Hz |
| `LSM6DSV320X_ODR_AT_240Hz` | 240 Hz |
| `LSM6DSV320X_ODR_AT_480Hz` | 480 Hz |
| `LSM6DSV320X_ODR_AT_960Hz` | 960 Hz |
| `LSM6DSV320X_ODR_AT_1920Hz` | 1920 Hz |

> **注意**：没有 `LSM6DSV320X_XL_ODR_AT_104Hz`，最接近的是 `LSM6DSV320X_ODR_AT_120Hz`。

**低 g 量程可选值**（`lsm6dsv320x_xl_full_scale_t`）：

| 枚举值 | 量程 |
|--------|------|
| `LSM6DSV320X_2g` | ±2g |
| `LSM6DSV320X_4g` | ±4g |
| `LSM6DSV320X_8g` | ±8g |
| `LSM6DSV320X_16g` | ±16g |

---

### 9.2 BDU（Block Data Update）

```c
int32_t lsm6dsv320x_block_data_update_set(const stmdev_ctx_t *ctx, uint8_t val);
```

**作用**：启用后，在读取完整一组数据之前，传感器不会更新输出寄存器，避免读到半新半旧的数据。建议始终启用。

---

## 10. FSM 相关

### 10.1 设置 FSM 模式

```c
int32_t lsm6dsv320x_fsm_mode_set(const stmdev_ctx_t *ctx,
                                   lsm6dsv320x_emb_fsm_enable_t val);
```

**使用示例**：
```c
lsm6dsv320x_emb_fsm_enable_t fsm_enable = {0};
fsm_enable.fsm1_en = 1;  /* 启用 FSM1 */
lsm6dsv320x_fsm_mode_set(&dev_ctx, fsm_enable);
```

### 10.2 读取 FSM 输出

```c
int32_t lsm6dsv320x_fsm_out_get(const stmdev_ctx_t *ctx,
                                  lsm6dsv320x_fsm_out_t *val);
```

---

## 11. 关键 API 速查表

| 功能 | 函数名 | 参数类型 |
|------|--------|---------|
| 读取设备 ID | `lsm6dsv320x_device_id_get` | `uint8_t *` |
| 软件复位 | `lsm6dsv320x_sw_reset` | — |
| 启用 BDU | `lsm6dsv320x_block_data_update_set` | `uint8_t` |
| 设置低 g ODR | `lsm6dsv320x_xl_data_rate_set` | `lsm6dsv320x_data_rate_t` |
| 设置低 g 量程 | `lsm6dsv320x_xl_full_scale_set` | `lsm6dsv320x_xl_full_scale_t` |
| 设置高 g ODR | `lsm6dsv320x_hg_xl_data_rate_set` | `lsm6dsv320x_hg_xl_data_rate_t` |
| 设置高 g 量程 | `lsm6dsv320x_hg_xl_full_scale_set` | `lsm6dsv320x_hg_xl_full_scale_t` |
| 设置自由落体阈值 | `lsm6dsv320x_ff_thresholds_set` | `lsm6dsv320x_ff_thresholds_t` |
| 设置自由落体时间窗口 | `lsm6dsv320x_ff_time_windows_set` | `uint8_t` |
| 配置高 g 唤醒阈值 | `lsm6dsv320x_hg_wake_up_cfg_set` | `lsm6dsv320x_hg_wake_up_cfg_t` |
| 启用高 g 中断 | `lsm6dsv320x_hg_wu_interrupt_cfg_set` | `lsm6dsv320x_hg_wu_interrupt_cfg_t` |
| 读取高 g 事件 | `lsm6dsv320x_hg_event_get` | `lsm6dsv320x_hg_event_t *` |
| 设置 INT1 中断路由 | `lsm6dsv320x_pin_int1_route_set` | `lsm6dsv320x_pin_int1_route_t *` |
| 读取所有事件源 | `lsm6dsv320x_all_sources_get` | `lsm6dsv320x_all_sources_t *` |
| 启用时间戳 | `lsm6dsv320x_timestamp_set` | `uint8_t` |
| 读取时间戳 | `lsm6dsv320x_timestamp_raw_get` | `uint32_t *` |
| 读取低 g 原始加速度 | `lsm6dsv320x_acceleration_raw_get` | `int16_t *` |
| 读取高 g 原始加速度 | `lsm6dsv320x_hg_acceleration_raw_get` | `int16_t *` |
| 低 g 量程换算 | `lsm6dsv320x_from_fs4_to_mg` 等 | `int16_t` |
| 高 g 量程换算 | `lsm6dsv320x_from_fs320_to_mg` 等 | `int16_t` |
| 时间戳 LSB → 纳秒 | `lsm6dsv320x_from_lsb_to_nsec` | `uint32_t` |
| 设置 FSM 模式 | `lsm6dsv320x_fsm_mode_set` | `lsm6dsv320x_emb_fsm_enable_t` |
| 读取 FSM 输出 | `lsm6dsv320x_fsm_out_get` | `lsm6dsv320x_fsm_out_t *` |
