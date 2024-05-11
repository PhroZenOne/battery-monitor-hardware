#include "ina226.h"

HAL_StatusTypeDef INA226_getBusV(I2C_HandleTypeDef *I2CHandler, double *out) {
    uint8_t setBusVoltageReg[1] = {INA226_BUSV};
    HAL_StatusTypeDef transmit = HAL_I2C_Master_Transmit(I2CHandler, INA226_ADDRESS, setBusVoltageReg, 1,
                                                         INA226_I2CTIMEOUT);
    if (transmit != HAL_OK) {
        return transmit;
    }

    uint8_t receiveBusVoltage[2] = {0, 0};
    HAL_StatusTypeDef receive = HAL_I2C_Master_Receive(I2CHandler, INA226_ADDRESS, receiveBusVoltage, 2,
                                                       INA226_I2CTIMEOUT);
    if (receive != HAL_OK) {
        return receive;
    }

    uint16_t x = receiveBusVoltage[0];
    x <<= 8;
    x |= receiveBusVoltage[1];
    *out = x * INA_226_MIN_BUS_VOLTAGE;
    return HAL_OK;
}

HAL_StatusTypeDef setConfig(I2C_HandleTypeDef *I2CHandler, uint8_t config) {
    uint8_t setConfigReg[1] = {INA226_CONFIG};
    HAL_StatusTypeDef transmit = HAL_I2C_Master_Transmit(I2CHandler, INA226_ADDRESS, setConfigReg, 1,
                                                         INA226_I2CTIMEOUT);
    if (transmit != HAL_OK) {
        return transmit;
    }
    uint8_t configValues[2] = {0, 0};
    HAL_StatusTypeDef receive = HAL_I2C_Master_Receive(I2CHandler, INA226_ADDRESS, configValues, 2, INA226_I2CTIMEOUT);
    if (receive != HAL_OK) {
        return receive;
    }
    configValues[1] = (configValues[1] & ~INA226_CONFIG_MASK) | config; // 7 is the last three bits
    uint8_t newConfigReg[3] = {INA226_CONFIG, configValues[0], configValues[1]};
    transmit = HAL_I2C_Master_Transmit(I2CHandler, INA226_ADDRESS, newConfigReg, 3, INA226_I2CTIMEOUT);
    if (transmit != HAL_OK) {
        return transmit;
    }
    return HAL_OK;
}

HAL_StatusTypeDef INA226_standBy(I2C_HandleTypeDef *I2CHandler) {
    return setConfig(I2CHandler, INA226_MODE_POWER_DOWN);
}

HAL_StatusTypeDef INA226_wakeUpAndSmellTheVoltage(I2C_HandleTypeDef *I2CHandler) {
    return setConfig(I2CHandler, INA226_MODE_TRIG_BUS_VOLTAGE);
}
