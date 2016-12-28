测试 msm8916->pm8916->mmp-MPP2 (多功能引脚)ADC 功能。

MPP引脚可以作为一下功能使用：
Pulse width modulator Dimming control of external WLED driver （pwm）;
Home row LED driver Current sink through even MPPs(sink);
Other current drivers Even MPPs can be configured to sink up to 40 mA
ATC indicator (see input power management features);
Vibration motor driver 1.2 to 3.1 V in 100 mV increments(power);
ADC;
I/O;

详细餐卡pm8916的spec lm80-p0436-35_a_pm8916-pm8916-1_device_spec.pdf
https://www.google.com.hk/search?newwindow=1&safe=strict&biw=1440&bih=794&q=lm80-p0436-35_a_pm8916-pm8916-1_device_spec&oq=lm80-p0436-35_a_pm8916-pm8916-1_device_spec&gs_l=serp.3..30i10k1.75134020.75134020.1.75134853.1.1.0.0.0.0.484.484.4-1.1.0....0...1c.1.64.serp..0.1.483.yXczY79XSw0&bav=on.2,or.&bvm=bv.142059868,d.c2I&ion=1&ech=1&psi=sTRiWMeGMoPxvgTR-pqoBg.1482831021314.5&ei=sjRiWIC7IMnjvgS5nr2IBg&emsg=NCSR&noj=1&google_abuse=GOOGLE_ABUSE_EXEMPTION%3DID%3D5bf31a1b003e5439:TM%3D1482906162:C%3Dr:IP%3D116.247.69.94-:S%3DAPGng0sJ5JRowJ36jmrKEcdd6BQ0ZT5fQQ%3B+path%3D/%3B+domain%3Dgoogle.com%3B+expires%3DWed,+28-Dec-2016+09:22:42+GMT
