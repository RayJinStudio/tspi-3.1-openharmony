#include <securec.h>
#include <asm/unaligned.h>
#include "osal_mem.h"
#include "osal_io.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "hdf_touch.h"
#include "event_hub.h"
#include "input_i2c_ops.h"
#include "input_config.h"
#include "hdf_input_device_manager.h"
#include "hdf_types.h"
#include "device_resource_if.h"
#include "gpio_if.h"

#include "touch_cst128a.h"

static int32_t ChipInit(ChipDevice *device)
{
    (void)device;
    return HDF_SUCCESS;
}

static int32_t ChipResume(ChipDevice *device)
{
    (void)device;
    return HDF_SUCCESS;
}

static int32_t ChipSuspend(ChipDevice *device)
{
    (void)device;
    return HDF_SUCCESS;
}

static int32_t ChipDetect(ChipDevice *device)
{

    uint8_t regAddr;
    uint8_t regValue[2] = {0};
    uint8_t writeBuf[2];
    int32_t ret;
    InputI2cClient *i2cClient = &device->driver->i2cClient;

    HDF_LOGI("CST128A %s: enter", __func__);

    regAddr = CST128A_REG_FW_VER;
    ret = InputI2cRead(i2cClient, &regAddr, 1, &regValue[0], 1);
    CHIP_CHECK_RETURN(ret);

    regAddr = CST128A_REG_FW_MIN_VER;
    ret = InputI2cRead(i2cClient, &regAddr, 1, &regValue[1], 1);
    CHIP_CHECK_RETURN(ret);

    HDF_LOGI("CST128A %s: Firmware version = %d.%d", __func__, regValue[0], regValue[1]);

    writeBuf[0] = CST128A_DEVIDE_MODE_REG;
    writeBuf[1] = 0x00;
    ret = InputI2cWrite(i2cClient, writeBuf, 2);
    CHIP_CHECK_RETURN(ret);

    writeBuf[0] = CST128A_ID_G_MODE_REG;
    writeBuf[1] = 0x01;
    ret = InputI2cWrite(i2cClient, writeBuf, 2);
    CHIP_CHECK_RETURN(ret);


    (void)ChipInit(device);
    (void)ChipResume(device);
    (void)ChipSuspend(device);

    return HDF_SUCCESS;
}

static int32_t ParsePointData(ChipDevice *device, FrameData *frame, uint8_t pointNum)
{
    int32_t i;
    int32_t ret;
    uint8_t regAddr;
    uint8_t touchId;
    uint8_t buf[6] = { 0 };
    uint8_t temp = 0;
    InputI2cClient *i2cClient = &device->driver->i2cClient;

    for (i = 0; i < pointNum; i++) {
        regAddr = CST128A_REG_X_H + (i * 6);
        ret = InputI2cRead(i2cClient, &regAddr, 1, buf, 4);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("CST128A %s: Unable to fetch PointData, error: %d", __func__, ret);
            return ret;
        }

        touchId = (buf[2] >> 4) & 0x0f;
        if (touchId >= MAX_SUPPORT_POINT) {
            break;
        }

        // X轴
        frame->fingers[i].x = (((buf[0] << 8) | buf[1]) & 0x0fff);
        // Y轴
        frame->fingers[i].y = ((buf[2] << 8) | buf[3]) & 0x0fff;
        // 是否翻转
        if (CST128A_SWAPXY==1)
        {
            temp = frame->fingers[i].y;
            frame->fingers[i].y = frame->fingers[i].x;
            frame->fingers[i].x = temp;
        }
        // 是否X轴反转
        if (CST128A_INVERTX==1) frame->fingers[i].x = device->boardCfg->attr.resolutionX - frame->fingers[i].x - 1;
        // 是否Y轴反转
        if (CST128A_INVERTY==1) frame->fingers[i].y = device->boardCfg->attr.resolutionY - frame->fingers[i].y - 1;
        
        frame->fingers[i].trackId = touchId;
        frame->fingers[i].status = buf[0] >> 6;
        frame->fingers[i].valid = true;
        frame->definedEvent = frame->fingers[i].status;

        if ((frame->fingers[i].status == TOUCH_DOWN || frame->fingers[i].status == TOUCH_CONTACT) && (pointNum == 0)) {
            HDF_LOGE("CST128A %s: abnormal event data from driver chip", __func__);
            return HDF_FAILURE;
        }
    }

    return HDF_SUCCESS;
}

static int32_t ChipDataHandle(ChipDevice *device)
{
    int32_t ret = HDF_SUCCESS;
    uint8_t regAddr = CST128A_REG_STATUS;
    uint8_t touchStatus = 0;
    static uint8_t lastTouchStatus = 0;
    uint8_t pointNum = 0;

    InputI2cClient *i2cClient = &device->driver->i2cClient;
    FrameData *frame = &device->driver->frameData;  

    ret = InputI2cRead(i2cClient, &regAddr, 1, &touchStatus, 1);
    CHIP_CHECK_RETURN(ret);

    OsalMutexLock(&device->driver->mutex);
    (void)memset_s(frame, sizeof(FrameData), 0, sizeof(FrameData));

    // 触摸点数
    pointNum = touchStatus & GT_FINGER_NUM_MASK;
    if (pointNum == 0) {
        if (lastTouchStatus == 1) {
            lastTouchStatus = 0;
            frame->realPointNum = 0;
            frame->definedEvent = TOUCH_UP;
            ret = HDF_SUCCESS;
        } else {
            ret = HDF_FAILURE;
        }

        OsalMutexUnlock(&device->driver->mutex);
        return ret;
    }

    if (lastTouchStatus != touchStatus) {
        lastTouchStatus = touchStatus;
    }

    frame->realPointNum = pointNum;
    frame->definedEvent = TOUCH_DOWN;

    ret = ParsePointData(device, frame, pointNum);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: ParsePointData fail", __func__);
        OsalMutexUnlock(&device->driver->mutex);
        return ret;
    }

    OsalMutexUnlock(&device->driver->mutex);
    return ret;
}

static int32_t UpdateFirmware(ChipDevice *device)
{
    return HDF_SUCCESS;
}

static void SetAbility(ChipDevice *device)
{
    device->driver->inputDev->abilitySet.devProp[0] = SET_BIT(INPUT_PROP_DIRECT);
    device->driver->inputDev->abilitySet.eventType[0] = SET_BIT(EV_SYN) |
                                                        SET_BIT(EV_KEY) | SET_BIT(EV_ABS);
    device->driver->inputDev->abilitySet.absCode[0] = SET_BIT(ABS_X) | SET_BIT(ABS_Y);
    device->driver->inputDev->abilitySet.absCode[1] = SET_BIT(ABS_MT_POSITION_X) |
                                                      SET_BIT(ABS_MT_POSITION_Y) | SET_BIT(ABS_MT_TRACKING_ID);
    device->driver->inputDev->abilitySet.keyCode[3] = SET_BIT(KEY_UP) | SET_BIT(KEY_DOWN);
    device->driver->inputDev->attrSet.axisInfo[ABS_X].min = 0;
    device->driver->inputDev->attrSet.axisInfo[ABS_X].max = device->boardCfg->attr.resolutionX - 1;
    device->driver->inputDev->attrSet.axisInfo[ABS_X].range = 0;
    device->driver->inputDev->attrSet.axisInfo[ABS_Y].min = 0;
    device->driver->inputDev->attrSet.axisInfo[ABS_Y].max = device->boardCfg->attr.resolutionY - 1;
    device->driver->inputDev->attrSet.axisInfo[ABS_Y].range = 0;
    device->driver->inputDev->attrSet.axisInfo[ABS_MT_POSITION_X].min = 0;
    device->driver->inputDev->attrSet.axisInfo[ABS_MT_POSITION_X].max = device->boardCfg->attr.resolutionX - 1;
    device->driver->inputDev->attrSet.axisInfo[ABS_MT_POSITION_X].range = 0;
    device->driver->inputDev->attrSet.axisInfo[ABS_MT_POSITION_Y].min = 0;
    device->driver->inputDev->attrSet.axisInfo[ABS_MT_POSITION_Y].max = device->boardCfg->attr.resolutionY - 1;
    device->driver->inputDev->attrSet.axisInfo[ABS_MT_POSITION_Y].range = 0;
    device->driver->inputDev->attrSet.axisInfo[ABS_MT_TRACKING_ID].max = MAX_SUPPORT_POINT;
}

static struct TouchChipOps g_cst128aChipOps = {
    .Init = ChipInit,
    .Detect = ChipDetect,
    .Resume = ChipResume,
    .Suspend = ChipSuspend,
    .DataHandle = ChipDataHandle,
    .UpdateFirmware = UpdateFirmware,
    .SetAbility = SetAbility,
};

static void FreeChipConfig(TouchChipCfg *config)
{
    if (config->pwrSeq.pwrOn.buf != NULL)
    {
        OsalMemFree(config->pwrSeq.pwrOn.buf);
    }

    if (config->pwrSeq.pwrOff.buf != NULL)
    {
        OsalMemFree(config->pwrSeq.pwrOff.buf);
    }

    if (config->pwrSeq.resume.buf != NULL)
    {
        OsalMemFree(config->pwrSeq.resume.buf);
    }

    if (config->pwrSeq.suspend.buf != NULL)
    {
        OsalMemFree(config->pwrSeq.suspend.buf);
    }

    OsalMemFree(config);
}

static TouchChipCfg *ChipConfigInstance(struct HdfDeviceObject *device)
{
    TouchChipCfg *chipCfg = (TouchChipCfg *)OsalMemAlloc(sizeof(TouchChipCfg));
    if (chipCfg == NULL)
    {
        HDF_LOGE("CST128A %s: instance chip config failed", __func__);
        return NULL;
    }
    (void)memset_s(chipCfg, sizeof(TouchChipCfg), 0, sizeof(TouchChipCfg));

    if (ParseTouchChipConfig(device->property, chipCfg) != HDF_SUCCESS)
    {
        HDF_LOGE("CST128A %s: parse chip config failed", __func__);
        OsalMemFree(chipCfg);
        chipCfg = NULL;
    }
    return chipCfg;
}

static ChipDevice *ChipDeviceInstance(void)
{
    ChipDevice *chipDev = (ChipDevice *)OsalMemAlloc(sizeof(ChipDevice));
    if (chipDev == NULL)
    {
        HDF_LOGE("CST128A %s: instance chip device failed", __func__);
        return NULL;
    }
    (void)memset_s(chipDev, sizeof(ChipDevice), 0, sizeof(ChipDevice));
    return chipDev;
}

static int32_t HdfCstFtChipInit(struct HdfDeviceObject *device)
{
    TouchChipCfg *chipCfg = NULL;
    ChipDevice *chipDev = NULL;

    HDF_LOGI("CST128A %s: enter", __func__);
    if (device == NULL)
    {
        HDF_LOGE("CST128A %s: device is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    chipCfg = ChipConfigInstance(device);
    if (chipCfg == NULL)
    {
        HDF_LOGE("CST128A %s: ChipConfigInstance is NULL", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    chipDev = ChipDeviceInstance();
    if (chipDev == NULL)
    {
        HDF_LOGE("CST128A %s: ChipDeviceInstance fail", __func__);
        FreeChipConfig(chipCfg);
        return HDF_FAILURE;
    }

    chipDev->chipCfg = chipCfg;
    chipDev->ops = &g_cst128aChipOps;
    chipDev->chipName = chipCfg->chipName;
    chipDev->vendorName = chipCfg->vendorName;
    device->priv = (void *)chipDev;

    if (RegisterTouchChipDevice(chipDev) != HDF_SUCCESS)
    {
        HDF_LOGE("CST128A %s: RegisterTouchChipDevice fail", __func__);
        OsalMemFree(chipDev);
        FreeChipConfig(chipCfg);
        return HDF_FAILURE;
    }

    HDF_LOGI("CST128A %s: exit succ, chipName = %s", __func__, chipCfg->chipName);
    return HDF_SUCCESS;
}

static void HdfCstFtChipRelease(struct HdfDeviceObject *device)
{
    HDF_LOGI("CST128A %s: enter", __func__);
    if (device == NULL || device->priv == NULL)
    {
        HDF_LOGE("CST128A %s: param is null", __func__);
        return;
    }
    HDF_LOGI("CST128A %s: chip is release", __func__);
}

struct HdfDriverEntry g_touchCstFtChipEntry = {
    .moduleVersion = 1,
    .moduleName = "HDF_TOUCH_CST128A",
    .Init = HdfCstFtChipInit,
    .Release = HdfCstFtChipRelease,
};

HDF_INIT(g_touchCstFtChipEntry);
