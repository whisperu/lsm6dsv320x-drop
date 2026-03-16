# GitHub 参考资料整理

本文档整理了与 LSM6DSV320X 掉落检测相关的 GitHub 参考资料，主要来源于 ST 官方仓库。

> **注意**：GitHub 代码搜索结果有返回条数限制，本文档所列内容可能不完整。建议你在 GitHub 上自行搜索补充：
> `https://github.com/search?q=LSM6DSV320X+freefall&type=code`

---

## 1. 最值得参考的仓库

### 1.1 STMicroelectronics/lsm6dsv320x-pid

| 类型 | 说明 |
|------|------|
| 仓库地址 | https://github.com/STMicroelectronics/lsm6dsv320x-pid |
| 用途 | **纯寄存器层驱动**（平台无关，Standard C，符合 MISRA C） |
| 核心文件 | `lsm6dsv320x_reg.c` / `lsm6dsv320x_reg.h` |

**为什么重要**：
- 这是 ST 官方提供的**平台无关驱动**（PID = Platform Independent Driver）。
- 所有函数签名和结构体定义以此仓库为准。
- 不依赖 STM32 HAL，可移植到任何平台（ESP32、NRF、Linux 等）。
- 只需要你实现 `write_reg` / `read_reg` / `mdelay` 三个回调即可。

### 1.2 STMicroelectronics/X-CUBE-MEMS1

| 类型 | 说明 |
|------|------|
| 仓库地址 | https://github.com/STMicroelectronics/X-CUBE-MEMS1 |
| 用途 | ST 官方 MEMS 传感器软件扩展包（含完整工程和 BSP） |

**包含内容**：
- LSM6DSV320X 完整 BSP 驱动
- 针对多个 NUCLEO 开发板的应用示例
- FSM / MLC 配置示例
- FIFO、中断、时间戳等使用示例

---

## 2. 最相关的示例工程路径

### HighGLowGFusion_LSM6DSV320X

这是与掉落检测最接近的现成工程，已经实现了：
- 高 g / 低 g 融合事件识别
- FSM 配置下发
- FSM 输出读取
- 中断驱动事件处理

以下是各开发板对应的工程路径（在 X-CUBE-MEMS1 仓库中）：

| 开发板 | 路径 |
|--------|------|
| NUCLEO-F401RE | `Projects/NUCLEO-F401RE/Applications/CUSTOM/HighGLowGFusion_LSM6DSV320X/` |
| NUCLEO-L073RZ | `Projects/NUCLEO-L073RZ/Applications/CUSTOM/HighGLowGFusion_LSM6DSV320X/` |
| NUCLEO-U575ZI-Q | `Projects/NUCLEO-U575ZI-Q/Applications/CUSTOM/HighGLowGFusion_LSM6DSV320X/` |
| NUCLEO-L152RE | `Projects/NUCLEO-L152RE/Applications/CUSTOM/HighGLowGFusion_LSM6DSV320X/` |

**为什么值得看**：这个示例已经完整演示了 FSM 初始化、高 g 通道启用、以及事件读取的完整流程，是做掉落检测的最佳起点。

---

## 3. 关键文件说明

### 3.1 FSM 配置文件

路径（以 F401RE 为例）：
```
Projects/NUCLEO-F401RE/Applications/CUSTOM/HighGLowGFusion_LSM6DSV320X/Inc/highglowg_fsm.h
```

说明：
- 包含一张 `ucf_line_ext_t highglowg_fsm[]` 数组
- 这是 FSM 的初始化配置序列（寄存器地址 + 值）
- 直接调用 `lsm6dsv320x_write_reg()` 逐条写入即可下发给芯片
- 不需要手写寄存器，直接复用

### 3.2 应用主文件

路径：
```
Projects/NUCLEO-F401RE/Applications/CUSTOM/HighGLowGFusion_LSM6DSV320X/Src/app_mems.c
```

说明：
- 包含 `FSM_Init()` —— 把 FSM 配置序列写入芯片
- 包含 `FSM_Handler()` —— 读取 FSM 输出寄存器
- 包含 `Enable_HighG()` —— 启用高 g 加速度通道
- 是整个应用的核心入口，建议重点阅读

### 3.3 BSP 驱动文件（X-CUBE-MEMS1 中的版本）

路径：
```
Drivers/BSP/Components/lsm6dsv320x/lsm6dsv320x.c
Drivers/BSP/Components/lsm6dsv320x/lsm6dsv320x.h
Drivers/BSP/Components/lsm6dsv320x/lsm6dsv320x_reg.c
Drivers/BSP/Components/lsm6dsv320x/lsm6dsv320x_reg.h
```

说明：
- `lsm6dsv320x_reg.c / .h`：寄存器层驱动，包含所有寄存器读写函数
- `lsm6dsv320x.c / .h`：BSP 中间层，封装了 free-fall / high-g / 中断等配置接口

和掉落检测相关的函数（详见 `docs/api-notes.md`）均来自这两组文件。

---

## 4. 参考代码片段示例

### FSM 初始化（来自 app_mems.c）

```c
/* 注意：这里使用的是 BSP 层封装函数 LSM6DSV320X_Write_Reg()。
 * 如果你使用 PID 驱动，等价调用为 lsm6dsv320x_write_reg()。 */
void FSM_Init(void)
{
    int i;
    int length = sizeof(highglowg_fsm) / sizeof(ucf_line_ext_t);
    for (i = 0; i < length; i++) {
        LSM6DSV320X_Write_Reg(dev, highglowg_fsm[i].address, highglowg_fsm[i].data);
    }
}
```

### 启用高 g 通道（来自 app_mems.c）

```c
void Enable_HighG(void)
{
    (void)LSM6DSV320X_ACC_HG_Enable(MotionCompObj[CUSTOM_LSM6DSV320X_0]);
    (void)LSM6DSV320X_ACC_HG_SetOutputDataRate(MotionCompObj[CUSTOM_LSM6DSV320X_0], HighGODR);
    HighGEnable = 1;
}
```

> 如果你不使用 BSP 层，等价的 PID 驱动调用为：
> ```c
> lsm6dsv320x_hg_xl_data_rate_set(&dev_ctx, LSM6DSV320X_HG_XL_ODR_AT_480Hz);
> lsm6dsv320x_hg_xl_full_scale_set(&dev_ctx, LSM6DSV320X_320g);
> ```

### 单位换算（来自 lsm6dsv320x_reg.c）

```c
/* 低 g 通道，量程 ±16g，1 LSB = 0.488 mg */
float mg = lsm6dsv320x_from_fs16_to_mg(raw_value);

/* 高 g 通道，量程 ±320g，1 LSB = 10.417 mg */
float mg = lsm6dsv320x_from_fs320_to_mg(raw_value);
```

---

## 5. 推荐阅读顺序

如果你是第一次接触这个芯片，建议按照以下顺序阅读参考资料：

1. **先看 `lsm6dsv320x_reg.h`（lsm6dsv320x-pid 仓库）** —— 了解所有函数签名和结构体定义
2. **再看 `app_mems.c` 里的 `FSM_Init` 和 `Enable_HighG`** —— 了解初始化流程
3. **再看 `highglowg_fsm.h`** —— 了解 FSM 配置内容
4. **最后看 `lsm6dsv320x_reg.c` 里的量程换算函数** —— 了解单位换算方法

---

## 6. 搜索建议

在 GitHub 上继续搜索时，推荐的关键词组合：

- `LSM6DSV320X freefall`
- `LSM6DSV320X high-g impact`
- `LSM6DSV320X FSM drop`
- `lsm6dsv320x_ff_thresholds_set`
- `lsm6dsv320x_hg_event_get`
- `lsm6dsv320x_all_sources_get`
