在/sys 文件系统下面创建一个节点用于控制GPIO4/5的状态

可以在系统起来后动态配置GPIO状态。

// echo 0 > /sys/bus/platform/devices/gpio_uart_exchage.67/gp_ex //设置为普通GPIO
// echo 1 > /sys/bus/platform/devices/gpio_uart_exchage.67/gp_ex //设置为UART模式