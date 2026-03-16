# GitHub 参考资料整理

本文档整理了与 LSM6DSV320X 掉落检测相关的 GitHub 参考资料，主要来源于 ST 官方仓库。

> **注意**：GitHub 代码搜索结果有返回条数限制，本文档所列内容可能不完整。建议你在 GitHub 上自行搜索补充：
> `https://github.com/search?q=LSM6DSV320X+freefall&type=code`

---

## 1. 最值得参考的仓库

### STMicroelectronics/X-CUBE-MEMS1

| 类型 | 链接 |
|------|------|
| 仓库主页 | https://github.com/STMicroelectronics/X-CUBE-MEMS1 |

这是 ST 官方的 MEMS 传感器软件扩展包，包含：
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

以下是各开发板对应的工程路径：

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

路径：
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

### 3.3 BSP 驱动文件

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

## 4. ST 官方相关驱动仓库

| 仓库 | 说明 |
|------|------|
| https://github.com/STMicroelectronics/lsm6dsv320x-pid | 纯寄存器层驱动（独立版本，不依赖 STM32 HAL）|

> **说明**：`lsm6dsv320x-pid` 仓库包含可移植的寄存器层驱动，适合移植到非 STM32 平台。如果你的硬件平台不是 STM32，建议从这里取驱动，再自己实现 I2C/SPI 读写接口。

---

## 5. 参考代码片段示例

### FSM 初始化（来自 app_mems.c）

```c
void FSM_Init(void)
{
    int i;
    int length = sizeof(highglowg_fsm) / sizeof(ucf_line_ext_t);
    for (i = 0; i < length; i++) {
        LSM6DSV320X_Write_Reg(dev, highglowg_fsm[i].address, highglowg_fsm[i].data);
    }
}
```

### FSM 输出读取（来自 app_mems.c）

```c
void FSM_Handler(void)
{
    uint8_t fsm_out1, fsm_out2;
    /* 切换到 embedded function 内存区域 */
    LSM6DSV320X_Write_Reg(dev, LSM6DSV320X_FUNC_CFG_ACCESS,
                          LSM6DSV320X_EMBED_FUNC_MEM_BANK << 7);
    /* 读取 FSM 输出寄存器 */
    LSM6DSV320X_Read_Reg(dev, LSM6DSV320X_FSM_OUTS1, &fsm_out1);
    LSM6DSV320X_Read_Reg(dev, LSM6DSV320X_FSM_OUTS2, &fsm_out2);
    /* 切回主内存区域 */
    LSM6DSV320X_Write_Reg(dev, LSM6DSV320X_FUNC_CFG_ACCESS,
                          LSM6DSV320X_MAIN_MEM_BANK << 7);
}
```

### 单位换算（来自 lsm6dsv320x_reg.c）

```c
/* 低 g 通道，量程 ±16g，1 LSB = 0.488 mg */
float mg = lsm6dsv320x_from_fs16_to_mg(raw_value);

/* 高 g 通道，量程 ±320g，1 LSB = 10.417 mg */
float mg = lsm6dsv320x_from_fs320_to_mg(raw_value);
```

---

## 6. 推荐阅读顺序

如果你是第一次接触这个芯片，建议按照以下顺序阅读参考资料：

1. **先看 `lsm6dsv320x.h`** —— 了解有哪些 BSP 级别的函数
2. **再看 `app_mems.c` 里的 `FSM_Init` 和 `Enable_HighG`** —— 了解初始化流程
3. **再看 `highglowg_fsm.h`** —— 了解 FSM 配置内容
4. **最后看 `lsm6dsv320x_reg.c` 里的量程换算函数** —— 了解单位换算方法

---

## 7. 搜索建议

在 GitHub 上继续搜索时，推荐的关键词组合：

- `LSM6DSV320X freefall`
- `LSM6DSV320X high-g impact`
- `LSM6DSV320X FSM drop`
- `lsm6dsv320x_ff_thresholds_set`
- `lsm6dsv320x_hg_event_get`
