#include "stm32l0xx_hal.h"
#include "stm32l0xx_hal_i2c.h"

#ifndef INA226_ADDRESS
#define INA226_ADDRESS	0x40 << 1
#endif

#define INA_226_MIN_BUS_VOLTAGE 1.25e-3
#define INA226_I2CTIMEOUT		10

#define INA226_CONFIG		0x00 // Configuration Register (R/W)
#define INA226_BUSV			0x02 // Bus Voltage (R)
#define INA226_CONFIG_MASK 0x07

#define INA226_MODE_POWER_DOWN			0x00 // Power-Down
#define INA226_MODE_TRIG_BUS_VOLTAGE	0x02 // Bus Voltage, Triggered


HAL_StatusTypeDef INA226_getBusV(I2C_HandleTypeDef *I2CHandler,  double *out);
HAL_StatusTypeDef INA226_standBy(I2C_HandleTypeDef *I2CHandler);
HAL_StatusTypeDef INA226_wakeUpAndSmellTheVoltage(I2C_HandleTypeDef *I2CHandler);