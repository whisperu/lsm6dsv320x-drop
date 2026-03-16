# 掉落检测实现方案

本文档面向初次使用 LSM6DSV320X 的开发者，介绍一套实用的掉落检测软件方案。

---

## 1. 整体事件链

掉落检测本质上是识别一串有顺序的物理事件：

```
正常状态 → 自由落体（失重）→ 撞击（高 g 冲击）→ 撞击后静止
```

每个阶段的特征如下：

| 阶段 | 加速度特征 | 时长参考 |
|------|-----------|----------|
| 正常状态 | 合加速度 ≈ 1g（重力） | — |
| 自由落体 | 合加速度 < 300 mg | 几十到几百 ms |
| 撞击（高 g） | 合加速度 > 4g，瞬间峰值 | 几 ms 到几十 ms |
| 撞击后静止 | 合加速度重新接近 1g | 50～200 ms 后稳定 |

> **初学者提示**：这个四段式状态机是掉落检测的核心。代码逻辑也建议围绕这四个状态来组织。

---

## 2. 各阶段检测方式

### 2.1 正常状态

设备处于静止或正常运动状态，合加速度约为 1g。

- 不需要特别检测，作为默认状态即可。
- 如果采用轮询，此阶段定期读取加速度即可。
- 如果采用中断，此阶段等待 free-fall 中断到来。

### 2.2 自由落体检测

**方式一：使用芯片内置 free-fall 检测单元（推荐初学者）**

芯片可以硬件判断合加速度低于阈值并持续一定时间，然后触发中断。

关键配置（使用 ST 官方 PID 驱动的正确 API）：
```c
/* 设置自由落体阈值（合加速度低于此值触发） */
lsm6dsv320x_ff_thresholds_set(&dev_ctx, LSM6DSV320X_156_mg);

/* 设置持续时间窗口（持续至少几个采样周期才触发） */
lsm6dsv320x_ff_time_windows_set(&dev_ctx, 4);  /* 单位：ODR 周期数 */

/* 将 free-fall 事件路由到 INT1 中断引脚 */
lsm6dsv320x_pin_int1_route_t int1_route = {0};
int1_route.freefall = 1;  /* 注意：字段名是 freefall，不是 free_fall */
lsm6dsv320x_pin_int1_route_set(&dev_ctx, &int1_route);
```

**方式二：使用 `lsm6dsv320x_all_sources_get()` 轮询（推荐调试阶段）**

```c
lsm6dsv320x_all_sources_t sources;
lsm6dsv320x_all_sources_get(&dev_ctx, &sources);
if (sources.free_fall) {
    /* 注意：all_sources_t 里的字段名是 free_fall（有下划线） */
}
```

**方式三：软件轮询合加速度**

如果暂时不想配中断，也可以在主循环里自己计算：
```c
float ax_mg, ay_mg, az_mg;
float magnitude = sqrtf(ax_mg*ax_mg + ay_mg*ay_mg + az_mg*az_mg);
if (magnitude < 300.0f) {
    /* 认为进入自由落体 */
}
```

### 2.3 撞击 / 高 g 检测

撞击时加速度瞬间超过阈值，LSM6DSV320X 的高 g 通道可以捕获此事件。

关键配置（注意正确的结构体字段名）：
```c
/* 配置高 g 唤醒阈值 */
lsm6dsv320x_hg_wake_up_cfg_t hg_cfg = {0};
hg_cfg.hg_wakeup_ths = 0x20;  /* 阈值寄存器值，8 位，参考数据手册换算 */
hg_cfg.hg_shock_dur  = 0x01;  /* 冲击持续时间，4 位 */
lsm6dsv320x_hg_wake_up_cfg_set(&dev_ctx, hg_cfg);

/* 启用高 g 中断 */
lsm6dsv320x_hg_wu_interrupt_cfg_t hg_int_cfg = {0};
hg_int_cfg.hg_interrupts_enable = 1;
lsm6dsv320x_hg_wu_interrupt_cfg_set(&dev_ctx, hg_int_cfg);

/* 读取高 g 事件状态 */
lsm6dsv320x_hg_event_t hg_event;
lsm6dsv320x_hg_event_get(&dev_ctx, &hg_event);
if (hg_event.hg_wakeup) {  /* 注意：字段名是 hg_wakeup，不是 wake_up */
    /* 检测到撞击 */
    /* hg_event.hg_wakeup_x / y / z 可以看哪个轴触发 */
}
```

### 2.4 撞击后静止检测

撞击结束后等待 50～200 ms，读取三轴加速度，判断设备最终朝向。

判断逻辑见第 4 节。

---

## 3. 跌落距离估算

### 3.1 原理

如果已知自由落体的持续时间 `t`，可以用自由落体公式估算高度：

```
h ≈ (1/2) × g × t²
```

其中：
- `h`：近似跌落高度，单位：米
- `g`：重力加速度，取 9.8 m/s²
- `t`：自由落体持续时间，单位：秒

### 3.2 时间获取方式

利用芯片内置时间戳功能，在进入自由落体时记录 `t0`，在检测到撞击时记录 `t1`：

```c
uint32_t ts_raw_t0, ts_raw_t1;

/* 进入自由落体时 */
lsm6dsv320x_timestamp_raw_get(&dev_ctx, &ts_raw_t0);

/* 检测到撞击时 */
lsm6dsv320x_timestamp_raw_get(&dev_ctx, &ts_raw_t1);

/* 换算为秒（芯片时间戳分辨率为 25 μs/LSB） */
float t0_sec = (float)ts_raw_t0 * 25e-6f;
float t1_sec = (float)ts_raw_t1 * 25e-6f;

float fall_duration = t1_sec - t0_sec;
float estimated_height = 0.5f * 9.8f * fall_duration * fall_duration;
```

> 也可以用官方换算函数 `lsm6dsv320x_from_lsb_to_nsec()` 获得纳秒值再转换。

### 3.3 为什么这只是近似值

这个方法有以下局限：

1. **有初速度的情况**：如果设备是被抛出的，而不是从静止开始下落，公式不再准确。
2. **斜抛运动**：非垂直下落时，自由落体时间对应的并不是垂直高度。
3. **碰撞前接触**：如果下落过程中设备先碰到斜面或边缘，会提前结束失重状态。
4. **旋转和振动**：设备旋转时，合加速度的计算会引入误差。
5. **中途翻滚**：失重判定可能在翻滚中抖动，导致 `t0` 识别偏差。

> **工程建议**：在最终输出时，请务必标注为「近似跌落高度」或「estimated fall distance」，不要标注为精确测量值。

---

## 4. 跌落方向判断

方向判断分两个层面输出：

### 4.1 撞击方向（impact direction）

在撞击发生的时间窗口内，读取高 g 三轴峰值，找出绝对值最大的轴：

```c
/* 读取撞击瞬间三轴原始值（使用高 g 通道） */
int16_t hg_raw[3];
lsm6dsv320x_hg_acceleration_raw_get(&dev_ctx, hg_raw);

float ax = lsm6dsv320x_from_fs320_to_mg(hg_raw[0]);
float ay = lsm6dsv320x_from_fs320_to_mg(hg_raw[1]);
float az = lsm6dsv320x_from_fs320_to_mg(hg_raw[2]);

/* 找出主撞击方向 */
float abs_ax = fabsf(ax);
float abs_ay = fabsf(ay);
float abs_az = fabsf(az);

if (abs_ax >= abs_ay && abs_ax >= abs_az) {
    /* 主撞击方向为 X 轴，ax > 0 为 +X，ax < 0 为 -X */
} else if (abs_ay >= abs_ax && abs_ay >= abs_az) {
    /* 主撞击方向为 Y 轴 */
} else {
    /* 主撞击方向为 Z 轴 */
}
```

> 也可以结合 `hg_event.hg_wakeup_x / y / z` 判断哪个轴触发了唤醒。

### 4.2 最终静止朝向（rest orientation）

撞击后等待静止，读取低 g 三轴平均加速度，判断哪个轴朝上（受重力分量最大）：

```c
/* 撞击后等待约 100 ms，读取低 g 三轴 */
int16_t raw[3];
lsm6dsv320x_acceleration_raw_get(&dev_ctx, raw);

float ax = lsm6dsv320x_from_fs4_to_mg(raw[0]);
float ay = lsm6dsv320x_from_fs4_to_mg(raw[1]);
float az = lsm6dsv320x_from_fs4_to_mg(raw[2]);

/* 判断哪轴受重力（接近 ±1000 mg 的轴为竖直方向） */
float abs_x = fabsf(ax), abs_y = fabsf(ay), abs_z = fabsf(az);
if (abs_z >= abs_y && abs_z >= abs_x) {
    /* Z 轴竖直，az > 0 为 Z 朝上，az < 0 为 Z 朝下 */
} else if (abs_x >= abs_y && abs_x >= abs_z) {
    /* X 轴竖直 */
} else {
    /* Y 轴竖直 */
}
```

> **注意**：`撞击方向` 和 `最终朝向` 是两个不同的概念，建议分开输出。

---

## 5. 推荐的状态机结构

```
+-------------+
|  STATE_IDLE |  ← 正常状态，等待 free-fall 触发
+------+------+
       | free-fall 检测到（合加速度 < 阈值）
       ↓
+------------------+
| STATE_FREE_FALL  |  ← 记录 t0，等待撞击
+--------+---------+
         | 高 g 事件触发，或超时（未落地则复位）
         ↓
+---------------+
| STATE_IMPACT  |  ← 记录 t1，记录三轴峰值
+-------+-------+
        | 等待约 100 ms
        ↓
+----------------+
| STATE_POST_REST|  ← 读取静止姿态，计算结果
+-------+--------+
        | 结果输出，状态复位
        ↓
+-------------+
|  STATE_IDLE |  ← 回到初始
+-------------+
```

---

## 6. 初学者建议步骤

1. 先完成芯片初始化，验证 `WHO_AM_I`（应返回 `0x73`）。
2. 能读出加速度，验证合加速度在静止时约等于 1000 mg。
3. 手动摇晃传感器，观察三轴数据变化。
4. 配置 free-fall 检测，测试中断是否能触发。
5. 配置 high-g 检测，测试冲击事件是否触发。
6. 加入时间戳，测量自由落体持续时间。
7. 计算近似跌落高度，和实际高度比较。
8. 加入方向判断，验证识别是否准确。
