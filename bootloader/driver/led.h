#ifndef __LED_H__
#define __LED_H__

struct led_desc;
typedef struct led_desc *led_desc_t;

void led_init(led_desc_t led);

#endif /* __LED_H__ */
