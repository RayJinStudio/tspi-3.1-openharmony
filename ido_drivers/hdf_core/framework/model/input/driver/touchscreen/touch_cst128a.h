#ifndef TOUCH_CST128A_H
#define TOUCH_CST128A_H

#define CST128A_SWAPXY 0
#define CST128A_INVERTY 0
#define CST128A_INVERTX 1

/* 寄存器地址 */
#define CST128A_DEVIDE_MODE_REG     0x00    // 工作模式寄存器，[6:4]，000正常，001系统信息模式，100测试模式
#define CST128A_REG_FW_VER          0xA1    // FW 版本高字节
#define CST128A_REG_FW_MIN_VER      0xA2    // FW 版本低字节
#define CST128A_ID_G_MODE_REG       0xA4    // 中断模式寄存器
#define CST128A_REG_STATUS          0x02    // 触摸状态寄存器，记录有多少个触摸点
#define CST128A_REG_X_H             0x03    // 第一个触摸点X坐标高位

/* 触摸时间标志 */
#define TOUCH_EVENT_DOWN        0x00    // 按下
#define TOUCH_EVENT_UP          0x01    // 抬起
#define TOUCH_EVENT_ON          0x02    // 接触
#define TOUCH_EVENT_RESERVED    0x03    // 保留

#define MAX_SUPPORT_POINT 5

#define GT_FINGER_NUM_MASK    0x0F

#endif