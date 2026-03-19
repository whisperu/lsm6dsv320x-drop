# LSM6DSV320X 关键寄存器说明

本文档整理了与掉落检测最相关的寄存器地址和位域定义，供调试时快速查阅。

> 来源：`STMicroelectronics/lsm6dsv320x-pid` → `lsm6dsv320x_reg.h`

---

## 1. 设备标识

| 寄存器名 | 地址 | 说明 |
|----------|------|------|
| `WHO_AM_I` | 0x0F | 设备 ID，固定值 `0x73` |

读到 `0x73` 说明芯片正确接线，可以继续初始化。

---

## 2. 控制和配置寄存器

| 寄存器名 | 地址 | 说明 |
|----------|------|------|
| `FUNC_CFG_ACCESS` | 0x01 | 功能配置入口，切换 embedded function 内存区域 |
| `CTRL1` | 0x10 | 低 g 加速度计 ODR 和模式控制 |
| `CTRL2` | 0x11 | 陀螺仪 ODR 和模式控制 |
| `CTRL3` | 0x12 | BDU、软件复位、自动递增等通用控制 |
| `CTRL1_XL_HG` | 0x4E | 高 g 加速度计 ODR 和量程控制 |
| `CTRL2_XL_HG` | 0x4D | 高 g 加速度计自测试和用户偏移控制 |

---

## 3. 自由落体相关寄存器

| 寄存器名 | 地址 | 位域 | 说明 |
|----------|------|------|------|
| `FREE_FALL` | 0x5D | `ff_ths[2:0]` | 自由落体阈值（3 位） |
| | | `ff_dur[7:3]` | 自由落体持续时间窗口（5 位，单位：ODR 周期） |
| `MD1_CFG` | 0x5E | `int1_ff` (bit4) | 将 free-fall 事件路由到 INT1 |

**ff_ths 阈值对应关系**：

| ff_ths 值 | 阈值 |
|-----------|------|
| 0 (000) | 156 mg |
| 1 (001) | 219 mg |
| 2 (010) | 250 mg |
| 3 (011) | 312 mg |
| 4 (100) | 344 mg |
| 5 (101) | 406 mg |
| 6 (110) | 469 mg |
| 7 (111) | 500 mg |

---

## 4. 高 g 唤醒 / 冲击相关寄存器

| 寄存器名 | 地址 | 位域 | 说明 |
|----------|------|------|------|
| `HG_WAKE_UP_SRC` | 0x4C | `hg_z_wu` (bit0) | Z 轴高 g 唤醒 |
| | | `hg_y_wu` (bit1) | Y 轴高 g 唤醒 |
| | | `hg_x_wu` (bit2) | X 轴高 g 唤醒 |
| | | `hg_wu_ia` (bit3) | 高 g 唤醒中断激活 |
| | | `hg_wu_change_ia` (bit4) | 高 g 唤醒状态变化 |
| | | `hg_shock_state` (bit5) | 冲击状态 |
| | | `hg_shock_change_ia` (bit6) | 冲击状态变化 |
| `HG_WAKE_UP_THS` | 0x53 | `hg_wk_ths[7:0]` | 高 g 唤醒阈值（8 位完整字节） |

**`HG_WAKE_UP_SRC` 读取示例**（通过 PID 驱动）：

```c
lsm6dsv320x_hg_event_t hg_event;
lsm6dsv320x_hg_event_get(&dev_ctx, &hg_event);
/* hg_event.hg_wakeup    → hg_wu_ia    (bit3) */
/* hg_event.hg_wakeup_x  → hg_x_wu     (bit2) */
/* hg_event.hg_wakeup_y  → hg_y_wu     (bit1) */
/* hg_event.hg_wakeup_z  → hg_z_wu     (bit0) */
/* hg_event.hg_shock     → hg_shock_state (bit5) */
```

---

## 5. 时间戳寄存器

| 寄存器名 | 地址 | 说明 |
|----------|------|------|
| `TIMESTAMP0` | 0x40 | 时间戳 byte 0（最低位） |
| `TIMESTAMP1` | 0x41 | 时间戳 byte 1 |
| `TIMESTAMP2` | 0x42 | 时间戳 byte 2 |
| `TIMESTAMP3` | 0x43 | 时间戳 byte 3（最高位） |

**换算**：
- 4 个字节组成 32 位无符号整数
- 1 LSB = 25 μs
- `时间（秒）= 原始值 × 25e-6`
- `时间（纳秒）= lsm6dsv320x_from_lsb_to_nsec(原始值)`

---

## 6. 加速度数据寄存器

### 6.1 低 g 加速度

| 寄存器名 | 地址 | 说明 |
|----------|------|------|
| `OUTX_L_A` | 0x28 | X 轴低字节 |
| `OUTX_H_A` | 0x29 | X 轴高字节 |
| `OUTY_L_A` | 0x2A | Y 轴低字节 |
| `OUTY_H_A` | 0x2B | Y 轴高字节 |
| `OUTZ_L_A` | 0x2C | Z 轴低字节 |
| `OUTZ_H_A` | 0x2D | Z 轴高字节 |

每轴 16 位有符号整数，需配合量程换算函数使用。

### 6.2 高 g 加速度

| 寄存器名 | 地址 | 说明 |
|----------|------|------|
| `HG_OUTX_L_A` | — | X 轴高 g 低字节 |
| `HG_OUTX_H_A` | — | X 轴高 g 高字节 |
| `HG_OUTY_L_A` | — | Y 轴高 g 低字节 |
| `HG_OUTY_H_A` | — | Y 轴高 g 高字节 |
| `HG_OUTZ_L_A` | — | Z 轴高 g 低字节 |
| `HG_OUTZ_H_A` | — | Z 轴高 g 高字节 |

> 高 g 加速度数据寄存器地址请参考数据手册中的完整寄存器映射表。

---

## 7. 状态和事件寄存器

| 寄存器名 | 地址 | 说明 |
|----------|------|------|
| `ALL_INT_SRC` | 0x1D | 所有中断源汇总 |
| `STATUS_REG` | — | 数据就绪状态 |
| `EMB_FUNC_STATUS_MAINPAGE` | 0x49 | 嵌入式功能状态（FSM/MLC 等） |
| `HG_WAKE_UP_SRC` | 0x4C | 高 g 唤醒事件源（详见第 4 节） |

---

## 8. 嵌入式功能（Embedded Function）寄存器

| 寄存器名 | 地址 | 说明 |
|----------|------|------|
| `FUNC_CFG_ACCESS` | 0x01 | 切换到嵌入式功能内存区域 |
| `EMB_FUNC_EN_A` | 0x04 | 嵌入式功能使能 A |
| `EMB_FUNC_EN_B` | 0x05 | 嵌入式功能使能 B |
| `EMB_FUNC_INT1` | 0x0A | 嵌入式功能中断路由到 INT1 |
| `EMB_FUNC_INT2` | 0x0E | 嵌入式功能中断路由到 INT2 |
| `EMB_FUNC_STATUS` | 0x12 | 嵌入式功能状态 |
| `EMB_FUNC_CFG` | 0x63 | 嵌入式功能配置 |

> 这些寄存器在 embedded function 内存区域中，需要通过 `FUNC_CFG_ACCESS` 切换 bank 后访问。
> PID 驱动中可以用 `lsm6dsv320x_mem_bank_set()` 来切换。

---

## 9. FSM 输出寄存器

| 寄存器名 | 地址 | 说明 |
|----------|------|------|
| `FSM_OUTS1` | — | FSM 通道 1 输出 |
| `FSM_OUTS2` | — | FSM 通道 2 输出 |
| ... | ... | ... |
| `FSM_OUTS8` | — | FSM 通道 8 输出 |

> 同样在 embedded function 内存区域中，PID 驱动提供 `lsm6dsv320x_fsm_out_get()` 封装。

---

## 10. 常用配置流程（寄存器视角）

以下是从寄存器角度看的初始化流程，对应 PID 驱动函数：

```
1. 读取 WHO_AM_I (0x0F)，确认返回 0x73
   → lsm6dsv320x_device_id_get()

2. 执行软件复位
   → lsm6dsv320x_sw_reset()

3. 启用 BDU（CTRL3 寄存器）
   → lsm6dsv320x_block_data_update_set(&dev_ctx, 1)

4. 设置低 g ODR（CTRL1 寄存器）
   → lsm6dsv320x_xl_data_rate_set(&dev_ctx, LSM6DSV320X_ODR_AT_120Hz)

5. 设置低 g 量程（CTRL8 寄存器）
   → lsm6dsv320x_xl_full_scale_set(&dev_ctx, LSM6DSV320X_4g)

6. 启用高 g 通道（CTRL1_XL_HG 寄存器）
   → lsm6dsv320x_hg_xl_data_rate_set(&dev_ctx, LSM6DSV320X_HG_XL_ODR_AT_480Hz)
   → lsm6dsv320x_hg_xl_full_scale_set(&dev_ctx, LSM6DSV320X_320g)

7. 配置 free-fall 阈值和时间窗口（FREE_FALL 0x5D）
   → lsm6dsv320x_ff_thresholds_set(&dev_ctx, LSM6DSV320X_250_mg)
   → lsm6dsv320x_ff_time_windows_set(&dev_ctx, 5)

8. 配置高 g 唤醒阈值（HG_WAKE_UP_THS 0x53）
   → lsm6dsv320x_hg_wake_up_cfg_set(&dev_ctx, hg_cfg)

9. 启用高 g 中断
   → lsm6dsv320x_hg_wu_interrupt_cfg_set(&dev_ctx, hg_int_cfg)

10. 启用时间戳
    → lsm6dsv320x_timestamp_set(&dev_ctx, 1)

11. 配置 INT1 中断路由（MD1_CFG 0x5E）
    → lsm6dsv320x_pin_int1_route_set(&dev_ctx, &int1_route)
```
