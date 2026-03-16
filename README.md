# LSM6DSV320X 掉落检测资料整理

这个仓库用于整理 LSM6DSV320X 芯片做掉落检测（drop detection / free-fall detection）的入门资料、GitHub 参考代码、关键 API 说明，以及一个最小化的软件框架示例。

## 这个芯片能做什么

LSM6DSV320X 是一颗 6 轴 IMU，包含：
- 低 g 加速度计（用于常规姿态、运动检测）
- 高 g 加速度通道（适合冲击 / 撞击检测）
- 陀螺仪
- FSM（Finite State Machine，有限状态机）
- MLC（Machine Learning Core）
- 时间戳、FIFO、中断路由等功能

对“掉落检测”来说，最有价值的是：
1. Free-fall（自由落体）检测
2. High-g（撞击/冲击）检测
3. FSM / 中断路由
4. 时间戳 + 原始加速度数据

## 建议的实现路线

对于初学者，建议按下面顺序理解：

1. **先做事件检测**
   - 自由落体开始
   - 撞击发生
   - 撞击后静止

2. **再做方向判断**
   - 通过撞击瞬间三轴峰值判断主撞击方向
   - 通过静止后姿态判断最终朝向

3. **最后做距离估算**
   - 优先用自由落体时间估算：h = 1/2 * g * t^2
   - 不建议一开始就做双积分

## 重点结论

- GitHub 上能找到 **LSM6DSV320X 的驱动和接近掉落检测的示例**。
- ST 官方仓库 `STMicroelectronics/X-CUBE-MEMS1` 最值得参考。
- 现成代码更接近“自由落体 + 高 g 冲击 + FSM 事件检测”。
- “跌落距离”和“跌落方向”通常还需要自己在应用层补算法。

## 推荐优先阅读

- `docs/chip-overview.md`
- `docs/drop-detection-plan.md`
- `docs/github-reference.md`
- `docs/api-notes.md`
- `docs/register-notes.md`
- `examples/minimal_drop_detect.c`

## 后续建议

后续可以继续补充：
- 真实寄存器初始化序列
- 基于中断的状态机
- FIFO 记录撞击前后数据
- 距离估算参数标定方法
- 不同安装方向下的坐标映射说明
