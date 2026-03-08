/*
 * PWM LED driver data - see drivers/leds/leds-pwm.c
 */
#ifndef __LINUX_LEDS_PWM_H
#define __LINUX_LEDS_PWM_H

struct led_pwm {
	const char	*name;
	const char	*default_trigger;
	unsigned	pwm_id __deprecated;
	u8 		active_low;
	unsigned 	max_brightness;
	unsigned	pwm_period_ns;
};

struct led_pwm_platform_data {
	int			num_leds;
	struct led_pwm	*leds;
};

struct led_pwm_data {
	struct led_classdev	cdev;
	struct pwm_device	*pwm;
	struct work_struct	work;
	struct delayed_work     led_work;
	unsigned int		active_low;
	unsigned int		period;
	int			duty;
	bool			can_sleep;
	struct device   *dev;
};
void led_pattern_init(struct led_classdev *led_cdev);
void led_pattern_exit(struct led_classdev *led_cdev);
#endif
