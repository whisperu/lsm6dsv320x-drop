# LSM6DSV320X 掉落检测相关 API 说明

本文档从 ST 官方驱动中提取与掉落检测最相关的 API，适合初学者快速上手。

> 驱动来源：`STMicroelectronics/X-CUBE-MEMS1`，路径：
> `Drivers/BSP/Components/lsm6dsv320x/lsm6dsv320x_reg.c`

---

## 1. 自由落体（Free-fall）相关

### 1.1 设置自由落体阈值

```c
int32_t lsm6dsv320x_ff_thresholds_set(stmdev_ctx_t *ctx,
                                       lsm6dsv320x_ff_thresholds_t val);
```

**作用**：设置合加速度低于多少 mg 时，认为进入自由落体。

**常用参数值**：

| 枚举值 | 对应阈值 |
|--------|---------|
| `LSM6DSV320X_156_mg` | 156 mg |
| `LSM6DSV320X_219_mg` | 219 mg |
| `LSM6DSV320X_250_mg` | 250 mg |
| `LSM6DSV320X_312_mg` | 312 mg |
| `LSM6DSV320X_344_mg` | 344 mg |
| `LSM6DSV320X_406_mg` | 406 mg |
| `LSM6DSV320X_469_mg` | 469 mg |
| `LSM6DSV320X_500_mg` | 500 mg |

**使用示例**：
```c
lsm6dsv320x_ff_thresholds_set(&dev_ctx, LSM6DSV320X_250_mg);
```

---

### 1.2 设置自由落体持续时间窗口

```c
int32_t lsm6dsv320x_ff_time_windows_set(stmdev_ctx_t *ctx, uint8_t val);
```

**作用**：合加速度低于阈值需持续多少个 ODR（采样周期）才触发事件。

**说明**：防止短暂抖动被误判为自由落体。一般设置 4～6 个采样周期即可。

**使用示例**：
```c
lsm6dsv320x_ff_time_windows_set(&dev_ctx, 5);
```

---

## 2. 高 g 冲击（High-g / Impact）相关

### 2.1 配置高 g 唤醒参数

```c
int32_t lsm6dsv320x_hg_wake_up_cfg_set(stmdev_ctx_t *ctx,
                                         lsm6dsv320x_hg_wake_up_cfg_t val);
```

**作用**：配置高 g 通道的唤醒阈值和相关参数。

**结构体主要字段**：

| 字段 | 说明 |
|------|------|
| `wake_ths` | 唤醒阈值，值越大阈值越高 |
| `wake_dur` | 持续时间，防抖 |

**使用示例**：
```c
lsm6dsv320x_hg_wake_up_cfg_t hg_cfg = {0};
hg_cfg.wake_ths = 0x20;  /* 具体值参考数据手册换算 */
lsm6dsv320x_hg_wake_up_cfg_set(&dev_ctx, hg_cfg);
```

---

### 2.2 读取高 g 事件状态

```c
int32_t lsm6dsv320x_hg_event_get(stmdev_ctx_t *ctx,
                                   lsm6dsv320x_hg_wu_event_t *val);
```

**作用**：读取当前高 g 事件状态，判断是否有撞击/唤醒事件。

**结构体主要字段**：

| 字段 | 说明 |
|------|------|
| `wake_up` | 1 表示有高 g 唤醒事件 |
| `wake_up_x` | X 轴触发 |
| `wake_up_y` | Y 轴触发 |
| `wake_up_z` | Z 轴触发 |

**使用示例**：
```c
lsm6dsv320x_hg_wu_event_t hg_event;
lsm6dsv320x_hg_event_get(&dev_ctx, &hg_event);
if (hg_event.wake_up) {
    /* 发生了高 g 撞击事件 */
}
```

---

### 2.3 配置高 g 中断行为

```c
int32_t lsm6dsv320x_hg_wu_interrupt_cfg_set(stmdev_ctx_t *ctx,
                                              lsm6dsv320x_hg_wu_interrupt_cfg_t val);
```

**作用**：配置高 g 中断触发方式（脉冲或电平）。

---

## 3. 中断路由

### 3.1 将事件路由到 INT1 引脚

```c
int32_t lsm6dsv320x_pin_int1_route_set(stmdev_ctx_t *ctx,
                                         lsm6dsv320x_pin_int1_route_t val);
```

**作用**：选择哪些事件（free-fall、high-g 等）通过 INT1 引脚触发外部中断。

**结构体主要字段（掉落检测常用）**：

| 字段 | 说明 |
|------|------|
| `free_fall` | 1：自由落体事件路由到 INT1 |
| `wu` | 1：低 g 唤醒事件路由到 INT1 |

**使用示例**：
```c
lsm6dsv320x_pin_int1_route_t int1_route = {0};
int1_route.free_fall = 1;  /* 自由落体触发 INT1 */
lsm6dsv320x_pin_int1_route_set(&dev_ctx, &int1_route);
```

---

### 3.2 将 embedded 事件（FSM）路由到 INT1

```c
int32_t lsm6dsv320x_pin_int1_route_embedded_set(stmdev_ctx_t *ctx,
                                                  lsm6dsv320x_pin_int1_route_embedded_t val);
```

**作用**：将 FSM / MLC 事件路由到 INT1 引脚。

---

## 4. 时间戳

### 4.1 启用时间戳

```c
int32_t lsm6dsv320x_timestamp_set(stmdev_ctx_t *ctx, uint8_t val);
```

**作用**：开启或关闭芯片内部时间戳功能。

**使用示例**：
```c
lsm6dsv320x_timestamp_set(&dev_ctx, 1);  /* 1 = 启用 */
```

---

### 4.2 读取时间戳原始值

```c
int32_t lsm6dsv320x_timestamp_raw_get(stmdev_ctx_t *ctx, uint32_t *val);
```

**作用**：读取当前时间戳计数值。

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

### 4.3 时间戳 LSB 换算

```c
float_t lsm6dsv320x_from_lsb_to_nsec(uint32_t lsb);
```

**作用**：将时间戳原始 LSB 值换算为纳秒。

---

## 5. 原始加速度读取

### 5.1 低 g 加速度通道

```c
int32_t lsm6dsv320x_acceleration_raw_get(stmdev_ctx_t *ctx, int16_t *val);
```

**作用**：读取低 g 加速度通道三轴原始数据（int16_t[3]）。

**配套换算函数**：

```c
float_t lsm6dsv320x_from_fs2_to_mg(int16_t lsb);   /* ±2g 量程，0.061 mg/LSB */
float_t lsm6dsv320x_from_fs4_to_mg(int16_t lsb);   /* ±4g 量程，0.122 mg/LSB */
float_t lsm6dsv320x_from_fs8_to_mg(int16_t lsb);   /* ±8g 量程，0.244 mg/LSB */
float_t lsm6dsv320x_from_fs16_to_mg(int16_t lsb);  /* ±16g 量程，0.488 mg/LSB */
```

**使用示例**：
```c
int16_t raw[3];
lsm6dsv320x_acceleration_raw_get(&dev_ctx, raw);

float ax_mg = lsm6dsv320x_from_fs4_to_mg(raw[0]);
float ay_mg = lsm6dsv320x_from_fs4_to_mg(raw[1]);
float az_mg = lsm6dsv320x_from_fs4_to_mg(raw[2]);
```

---

### 5.2 高 g 加速度通道

```c
int32_t lsm6dsv320x_hg_acceleration_raw_get(stmdev_ctx_t *ctx, int16_t *val);
```

**作用**：读取高 g 加速度通道三轴原始数据，适合撞击时读取。

**配套换算函数**：

```c
float_t lsm6dsv320x_from_fs32_to_mg(int16_t lsb);   /* ±32g 量程 */
float_t lsm6dsv320x_from_fs64_to_mg(int16_t lsb);   /* ±64g 量程 */
float_t lsm6dsv320x_from_fs128_to_mg(int16_t lsb);  /* ±128g 量程 */
float_t lsm6dsv320x_from_fs256_to_mg(int16_t lsb);  /* ±256g 量程 */
float_t lsm6dsv320x_from_fs320_to_mg(int16_t lsb);  /* ±320g 量程，10.417 mg/LSB */
```

**使用示例**：
```c
int16_t hg_raw[3];
lsm6dsv320x_hg_acceleration_raw_get(&dev_ctx, hg_raw);

float hg_ax = lsm6dsv320x_from_fs320_to_mg(hg_raw[0]);
float hg_ay = lsm6dsv320x_from_fs320_to_mg(hg_raw[1]);
float hg_az = lsm6dsv320x_from_fs320_to_mg(hg_raw[2]);
```

---

## 6. FSM 相关

### 6.1 设置 FSM 模式

```c
int32_t lsm6dsv320x_fsm_mode_set(stmdev_ctx_t *ctx,
                                   lsm6dsv320x_emb_fsm_enable_t val);
```

**作用**：启用或禁用 FSM 功能。

**使用示例**：
```c
lsm6dsv320x_emb_fsm_enable_t fsm_enable = {0};
fsm_enable.fsm1_en = 1;  /* 启用 FSM1 */
lsm6dsv320x_fsm_mode_set(&dev_ctx, fsm_enable);
```

---

### 6.2 读取 FSM 输出

```c
int32_t lsm6dsv320x_fsm_out_get(stmdev_ctx_t *ctx,
                                  lsm6dsv320x_fsm_out_t *val);
```

**作用**：读取 FSM 的当前输出状态，用于判断 FSM 识别到了什么事件。

**使用示例**：
```c
lsm6dsv320x_fsm_out_t fsm_out;
lsm6dsv320x_fsm_out_get(&dev_ctx, &fsm_out);
/* fsm_out.fsm_outs1 等字段包含各 FSM 的输出结果 */
```

---

## 7. 关键 API 速查表

| 功能 | 函数名 |
|------|--------|
| 设置自由落体阈值 | `lsm6dsv320x_ff_thresholds_set` |
| 设置自由落体时间窗口 | `lsm6dsv320x_ff_time_windows_set` |
| 配置高 g 唤醒阈值 | `lsm6dsv320x_hg_wake_up_cfg_set` |
| 读取高 g 事件 | `lsm6dsv320x_hg_event_get` |
| 设置 INT1 中断路由 | `lsm6dsv320x_pin_int1_route_set` |
| 启用时间戳 | `lsm6dsv320x_timestamp_set` |
| 读取时间戳 | `lsm6dsv320x_timestamp_raw_get` |
| 读取低 g 原始加速度 | `lsm6dsv320x_acceleration_raw_get` |
| 读取高 g 原始加速度 | `lsm6dsv320x_hg_acceleration_raw_get` |
| 低 g 量程换算 | `lsm6dsv320x_from_fs4_to_mg` 等 |
| 高 g 量程换算 | `lsm6dsv320x_from_fs320_to_mg` 等 |
| 设置 FSM 模式 | `lsm6dsv320x_fsm_mode_set` |
| 读取 FSM 输出 | `lsm6dsv320x_fsm_out_get` |
