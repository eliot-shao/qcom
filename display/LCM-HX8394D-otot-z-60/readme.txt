输入：

LCM的规格书，包含分辨率,720x1280,rgb888,3lane_dsi；
porch值，该值已经被模组厂验证过：hbp/hfp/hpw/vbp/vhp/vpw；
lcm ic初始化指令，这个是LCM能正常工作的一些寄存器值，由模组厂提供；

通过使用Excel计算文档：80-NH713-1 DSI TIMING PARAMETERS USER INTERACTIVE SPREADSHEET.xlsm
填入分辨率，porch，dsilane数，pixel format ，计算出mipi输出的时钟，以及设置时钟的寄存器值。

在device\qcom\common\display\tools下面有一个perl工具脚本，用于生成LK和kernel需要使用的配置，
所以调屏主要任务就是收集和屏与dsi相关信息，填入到xml文件里面。

输出：

perl parser.pl panel_xxx_720p_video.xml panel

生成两个配置文件，用于点亮显示屏。