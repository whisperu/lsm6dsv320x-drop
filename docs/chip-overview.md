# LSM6DSV320X 芯片入门

## 1. 芯片定位

LSM6DSV320X 是 ST 的 6 轴惯性传感器（IMU），适合做：
- 姿态检测
- 运动检测
- 跌落 / 掉落检测
- 冲击检测
- 基于 FSM / MLC 的本地事件识别

它的核心价值在于：
- 普通低 g 加速度计适合姿态和日常运动分析
- 高 g 加速度通道适合冲击/撞击采样
- 陀螺仪适合旋转检测
- FSM / MLC 可以把部分识别逻辑前移到芯片内部

---

## 2. 和掉落检测相关的模块

### 2.1 低 g 加速度计
主要用途：
- 判断是否进入失重 / 自由落体
- 判断静止后的姿态
- 做基础三轴加速度分析

常用数据读取函数：
- `lsm6dsv320x_acceleration_raw_get()`
- `lsm6dsv320x_from_fs2_to_mg()`
- `lsm6dsv320x_from_fs4_to_mg()`
- `lsm6dsv320x_from_fs8_to_mg()`
- `lsm6dsv320x_from_fs16_to_mg()`

### 2.2 高 g 加速度通道
主要用途：
- 检测撞击峰值
- 识别大冲击事件
- 在掉落落地时判断主冲击方向

常用数据读取函数：
- `lsm6dsv320x_hg_acceleration_raw_get()`
- `lsm6dsv320x_from_fs32_to_mg()`
- `lsm6dsv320x_from_fs64_to_mg()`
- `lsm6dsv320x_from_fs128_to_mg()`
- `lsm6dsv320x_from_fs256_to_mg()`
- `lsm6dsv320x_from_fs320_to_mg()`

### 2.3 Free-fall 检测单元
芯片支持硬件自由落体判断。

常用配置函数：
- `lsm6dsv320x_ff_thresholds_set()`
- `lsm6dsv320x_ff_time_windows_set()`

### 2.4 High-g 事件检测
芯片支持高 g 唤醒 / 冲击相关检测。

常用配置函数：
- `lsm6dsv320x_hg_wake_up_cfg_set()`
- `lsm6dsv320x_hg_wu_interrupt_cfg_set()`
- `lsm6dsv320x_hg_event_get()`

### 2.5 时间戳
做跌落距离估算时很重要，因为可以计算自由落体持续时间。

常用函数：
- `lsm6dsv320x_timestamp_set()`
- `lsm6dsv320x_timestamp_raw_get()`
- `lsm6dsv320x_from_lsb_to_nsec()`

### 2.6 FSM / MLC
高级玩法。适合把“自由落体 -> 撞击 -> 姿态恢复”做成事件流。

常用函数：
- `lsm6dsv320x_fsm_mode_set()`
- `lsm6dsv320x_fsm_out_get()`
- `lsm6dsv320x_pin_int1_route_embedded_set()`
- `lsm6dsv320x_mlc_set()`

---

## 3. 掉落检测里最实用的能力

如果目标只是先做一个能工作的版本，优先使用：

1. Free-fall 阈值 + 时间窗
2. High-g 撞击检测
3. 原始加速度读取
4. 时间戳
5. 中断输出

这样就能先搭一个最小闭环：
- 检测到失重开始
- 检测到撞击结束
- 计算大致跌落时间
- 估算跌落高度
- 判断撞击方向

---

## 4. 初学者建议

不要一开始就：
- 直接写复杂 FSM
- 直接做双积分算位移
- 直接追求“高精度高度”

建议按下面顺序：
1. 跑通传感器初始化
2. 能读出加速度和时间戳
3. 能触发 free-fall 中断
4. 能触发 high-g 事件
5. 最后再做方向和距离估算

---

## 5. 你现在最需要理解的几个概念

- **free-fall**：合加速度接近 0g 的阶段
- **impact / high-g**：落地撞击瞬间的大加速度
- **rest orientation**：撞击后静止姿态
- **fall duration**：自由落体开始到撞击的时间
- **estimated drop height**：基于时间估算的近似跌落高度
