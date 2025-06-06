/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_video.h"

struct video_entity_info {
    uint8_t bDescriptorSubtype;
    uint8_t bEntityId;
    uint16_t wTerminalType;
};

struct usbd_video_priv {
    struct video_probe_and_commit_controls probe;
    struct video_probe_and_commit_controls commit;
    uint8_t power_mode;
    uint8_t error_code;
    struct video_entity_info info[3];
    uint8_t *ep_buf;
    bool stream_finish;
    uint8_t *stream_buf;
    uint32_t stream_len;
    uint32_t stream_offset;
    uint8_t stream_frameid;
    uint32_t stream_headerlen;
    bool do_copy;
} g_usbd_video[CONFIG_USBDEV_MAX_BUS];

static int usbd_video_control_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t control_selector = (uint8_t)(setup->wValue >> 8);

    (void)busid;

    switch (control_selector) {
        case VIDEO_VC_VIDEO_POWER_MODE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_SET_CUR:
                    break;
                case VIDEO_REQUEST_GET_CUR:
                    break;
                case VIDEO_REQUEST_GET_INFO:
                    break;
                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }

            break;
        case VIDEO_VC_REQUEST_ERROR_CODE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_GET_CUR:
                    (*data)[0] = 0x06;
                    *len = 1;
                    break;
                case VIDEO_REQUEST_GET_INFO:
                    break;
                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }

            break;
        default:
            break;
    }

    return 0;
}

static int usbd_video_control_unit_terminal_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t entity_id = (uint8_t)(setup->wIndex >> 8);
    uint8_t control_selector = (uint8_t)(setup->wValue >> 8);

    for (uint8_t i = 0; i < 3; i++) {
        struct video_entity_info *entity_info = &g_usbd_video[busid].info[i];
        if (entity_info->bEntityId == entity_id) {
            switch (entity_info->bDescriptorSubtype) {
                case VIDEO_VC_HEADER_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_INPUT_TERMINAL_DESCRIPTOR_SUBTYPE:
                    if (entity_info->wTerminalType == VIDEO_ITT_CAMERA) {
                        switch (control_selector) {
                            case VIDEO_CT_AE_MODE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR:
                                        (*data)[0] = 0x08;
                                        *len = 1;
                                        break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint32_t dwExposureTimeAbsolute = 2500;
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    case VIDEO_REQUEST_GET_MIN: {
                                        uint32_t dwExposureTimeAbsolute = 5; //0.0005sec
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    case VIDEO_REQUEST_GET_MAX: {
                                        uint32_t dwExposureTimeAbsolute = 2500; //0.2500sec
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    case VIDEO_REQUEST_GET_RES: {
                                        uint32_t dwExposureTimeAbsolute = 5; //0.0005sec
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    case VIDEO_REQUEST_GET_INFO:
                                        (*data)[0] = 0x03; //struct video_camera_capabilities
                                        *len = 1;
                                        break;
                                    case VIDEO_REQUEST_GET_DEF: {
                                        uint32_t dwExposureTimeAbsolute = 2500; //0.2500sec
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_FOCUS_ABSOLUTE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint16_t wFocusAbsolute = 0x0080;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MIN: {
                                        uint16_t wFocusAbsolute = 0;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MAX: {
                                        uint16_t wFocusAbsolute = 0x00ff;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_RES: {
                                        uint16_t wFocusAbsolute = 0x0001;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_INFO:
                                        (*data)[0] = 0x03; //struct video_camera_capabilities
                                        *len = 1;
                                        break;
                                    case VIDEO_REQUEST_GET_DEF: {
                                        uint16_t wFocusAbsolute = 0x0080;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_ZOOM_ABSOLUTE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint16_t wObjectiveFocalLength = 0x0064;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MIN: {
                                        uint16_t wObjectiveFocalLength = 0x0064;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MAX: {
                                        uint16_t wObjectiveFocalLength = 0x00c8;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_RES: {
                                        uint16_t wObjectiveFocalLength = 0x0001;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_INFO:
                                        (*data)[0] = 0x03; //struct video_camera_capabilities
                                        *len = 1;
                                        break;
                                    case VIDEO_REQUEST_GET_DEF: {
                                        uint16_t wObjectiveFocalLength = 0x0064;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_ROLL_ABSOLUTE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint16_t wRollAbsolute = 0x0000;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MIN: {
                                        uint16_t wRollAbsolute = 0x0000;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MAX: {
                                        uint16_t wRollAbsolute = 0x00ff;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_RES: {
                                        uint16_t wRollAbsolute = 0x0001;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_INFO:
                                        (*data)[0] = 0x03; //struct video_camera_capabilities
                                        *len = 1;
                                        break;
                                    case VIDEO_REQUEST_GET_DEF: {
                                        uint16_t wRollAbsolute = 0x0000;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_FOCUS_AUTO_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint16_t wFocusAuto = 0x0000;
                                        memcpy(*data, (uint8_t *)&wFocusAuto, 2);
                                        *len = 2;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            default:
                                USB_LOG_WRN("Unhandled Video Class control selector 0x%02x\r\n", control_selector);
                                return -1;
                        }
                    } else {
                        USB_LOG_WRN("Unhandled Video Class wTerminalType 0x%02x\r\n", entity_info->wTerminalType);
                        return -2;
                    }
                    break;
                case VIDEO_VC_OUTPUT_TERMINAL_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_SELECTOR_UNIT_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_PROCESSING_UNIT_DESCRIPTOR_SUBTYPE:
                    switch (control_selector) {
                        case VIDEO_PU_BACKLIGHT_COMPENSATION_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wBacklightCompensation = 0x0004;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wBacklightCompensation = 0;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wBacklightCompensation = 8;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wBacklightCompensation = 1;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wBacklightCompensation = 4;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_BRIGHTNESS_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_SET_CUR: {
                                    //uint16_t wBrightness = (uint16_t)(*data)[1] << 8 | (uint16_t)(*data)[0];
                                } break;
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wBrightness = 0x0080;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wBrightness = 0x0001;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wBrightness = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wBrightness = 0x0001;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wBrightness = 0x0080;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_CONTRAST_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wContrast = 0x0080;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wContrast = 0x0001;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wContrast = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wContrast = 0x0001;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wContrast = 0x0080;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_HUE_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wHue = 0x0080;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wHue = 0x0001;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wHue = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wHue = 0x0001;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wHue = 0x0080;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_SATURATION_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wSaturation = 0x0001;
                                    memcpy(*data, (uint8_t *)&wSaturation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wSaturation = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wSaturation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wSaturation = 0x0001;
                                    memcpy(*data, (uint8_t *)&wSaturation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wSaturation = 0x0080;
                                    memcpy(*data, (uint8_t *)&wSaturation, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_SHARPNESS_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wSharpness = 0x0001;
                                    memcpy(*data, (uint8_t *)&wSharpness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wSharpness = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wSharpness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wSharpness = 0x0001;
                                    memcpy(*data, (uint8_t *)&wSharpness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wSharpness = 0x0080;
                                    memcpy(*data, (uint8_t *)&wSharpness, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_GAIN_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wGain = 0;
                                    memcpy(*data, (uint8_t *)&wGain, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wGain = 255;
                                    memcpy(*data, (uint8_t *)&wGain, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wGain = 1;
                                    memcpy(*data, (uint8_t *)&wGain, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wGain = 255;
                                    memcpy(*data, (uint8_t *)&wGain, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_WHITE_BALANCE_TEMPERATURE_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wWhiteBalance_Temprature = 417;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wWhiteBalance_Temprature = 300;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wWhiteBalance_Temprature = 600;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wWhiteBalance_Temprature = 1;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wWhiteBalance_Temprature = 417;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wWhiteBalance_Temprature_Auto = 1;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature_Auto, 1);
                                    *len = 1;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        default:
                            g_usbd_video[busid].error_code = 0x06;
                            USB_LOG_WRN("Unhandled Video Class control selector 0x%02x\r\n", control_selector);
                            return -1;
                    }
                    break;
                case VIDEO_VC_EXTENSION_UNIT_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_ENCODING_UNIT_DESCRIPTOR_SUBTYPE:
                    break;

                default:
                    break;
            }
        }
    }
    return 0;
}

static int usbd_video_stream_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t control_selector = (uint8_t)(setup->wValue >> 8);

    switch (control_selector) {
        case VIDEO_VS_PROBE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_SET_CUR:
                    //memcpy((uint8_t *)&g_usbd_video[busid].probe, *data, setup->wLength);
                    break;
                case VIDEO_REQUEST_GET_CUR:
                    memcpy(*data, (uint8_t *)&g_usbd_video[busid].probe, setup->wLength);
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;

                case VIDEO_REQUEST_GET_MIN:
                case VIDEO_REQUEST_GET_MAX:
                case VIDEO_REQUEST_GET_RES:
                case VIDEO_REQUEST_GET_DEF:
                    memcpy(*data, (uint8_t *)&g_usbd_video[busid].probe, setup->wLength);
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;
                case VIDEO_REQUEST_GET_LEN:
                    (*data)[0] = sizeof(struct video_probe_and_commit_controls);
                    *len = 1;
                    break;

                case VIDEO_REQUEST_GET_INFO:
                    (*data)[0] = 0x03;
                    *len = 1;
                    break;

                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }
            break;
        case VIDEO_VS_COMMIT_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_SET_CUR:
                    //memcpy((uint8_t *)&g_usbd_video[busid].commit, *data, setup->wLength);
                    break;
                case VIDEO_REQUEST_GET_CUR:
                    memcpy(*data, (uint8_t *)&g_usbd_video[busid].commit, setup->wLength);
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;
                case VIDEO_REQUEST_GET_MIN:
                case VIDEO_REQUEST_GET_MAX:
                case VIDEO_REQUEST_GET_RES:
                case VIDEO_REQUEST_GET_DEF:
                    memcpy(*data, (uint8_t *)&g_usbd_video[busid].commit, setup->wLength);
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;

                case VIDEO_REQUEST_GET_LEN:
                    (*data)[0] = sizeof(struct video_probe_and_commit_controls);
                    *len = 1;
                    break;

                case VIDEO_REQUEST_GET_INFO:
                    (*data)[0] = 0x03;
                    *len = 1;
                    break;

                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }
            break;
        case VIDEO_VS_STREAM_ERROR_CODE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_GET_CUR:
                    (*data)[0] = g_usbd_video[busid].error_code;
                    *len = 1;
                    break;
                case VIDEO_REQUEST_GET_INFO:
                    (*data)[0] = 0x01;
                    *len = 1;
                    break;
                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }
            break;
        default:
            break;
    }

    return 0;
}

static int video_class_interface_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("Video Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    uint8_t intf_num = (uint8_t)setup->wIndex;
    uint8_t entity_id = (uint8_t)(setup->wIndex >> 8);

    if (intf_num == 0) { /* Video Control Interface */
        if (entity_id == 0) {
            return usbd_video_control_request_handler(busid, setup, data, len); /* Interface Control Requests */
        } else {
            return usbd_video_control_unit_terminal_request_handler(busid, setup, data, len); /* Unit and Terminal Requests */
        }
    } else if (intf_num == 1) {                                            /* Video Stream Inteface */
        return usbd_video_stream_request_handler(busid, setup, data, len); /* Interface Stream Requests */
    }
    return -1;
}

static void video_notify_handler(uint8_t busid, uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            g_usbd_video[busid].error_code = 0;
            g_usbd_video[busid].power_mode = 0;
            break;

        case USBD_EVENT_SET_INTERFACE: {
            struct usb_interface_descriptor *intf = (struct usb_interface_descriptor *)arg;
            if (intf->bAlternateSetting == 1) {
                usbd_video_open(busid, intf->bInterfaceNumber);
            } else {
                usbd_video_close(busid, intf->bInterfaceNumber);
            }
        }

        break;
        default:
            break;
    }
}

static void usbd_video_probe_and_commit_controls_init(uint8_t busid, uint32_t dwFrameInterval, uint32_t dwMaxVideoFrameSize, uint32_t dwMaxPayloadTransferSize)
{
    g_usbd_video[busid].probe.hintUnion.bmHint = 0x01;
    g_usbd_video[busid].probe.hintUnion1.bmHint = 0;
    g_usbd_video[busid].probe.bFormatIndex = 1;
    g_usbd_video[busid].probe.bFrameIndex = 1;
    g_usbd_video[busid].probe.dwFrameInterval = dwFrameInterval;
    g_usbd_video[busid].probe.wKeyFrameRate = 0;
    g_usbd_video[busid].probe.wPFrameRate = 0;
    g_usbd_video[busid].probe.wCompQuality = 0;
    g_usbd_video[busid].probe.wCompWindowSize = 0;
    g_usbd_video[busid].probe.wDelay = 0;
    g_usbd_video[busid].probe.dwMaxVideoFrameSize = dwMaxVideoFrameSize;
    g_usbd_video[busid].probe.dwMaxPayloadTransferSize = dwMaxPayloadTransferSize;
    g_usbd_video[busid].probe.dwClockFrequency = 0;
    g_usbd_video[busid].probe.bmFramingInfo = 0;
    g_usbd_video[busid].probe.bPreferedVersion = 0;
    g_usbd_video[busid].probe.bMinVersion = 0;
    g_usbd_video[busid].probe.bMaxVersion = 0;

    g_usbd_video[busid].commit.hintUnion.bmHint = 0x01;
    g_usbd_video[busid].commit.hintUnion1.bmHint = 0;
    g_usbd_video[busid].commit.bFormatIndex = 1;
    g_usbd_video[busid].commit.bFrameIndex = 1;
    g_usbd_video[busid].commit.dwFrameInterval = dwFrameInterval;
    g_usbd_video[busid].commit.wKeyFrameRate = 0;
    g_usbd_video[busid].commit.wPFrameRate = 0;
    g_usbd_video[busid].commit.wCompQuality = 0;
    g_usbd_video[busid].commit.wCompWindowSize = 0;
    g_usbd_video[busid].commit.wDelay = 0;
    g_usbd_video[busid].commit.dwMaxVideoFrameSize = dwMaxVideoFrameSize;
    g_usbd_video[busid].commit.dwMaxPayloadTransferSize = dwMaxPayloadTransferSize;
    g_usbd_video[busid].commit.dwClockFrequency = 0;
    g_usbd_video[busid].commit.bmFramingInfo = 0;
    g_usbd_video[busid].commit.bPreferedVersion = 0;
    g_usbd_video[busid].commit.bMinVersion = 0;
    g_usbd_video[busid].commit.bMaxVersion = 0;

    g_usbd_video[busid].stream_frameid = 0;
    g_usbd_video[busid].stream_headerlen = 12;
}

struct usbd_interface *usbd_video_init_intf(uint8_t busid,
                                            struct usbd_interface *intf,
                                            uint32_t dwFrameInterval,
                                            uint32_t dwMaxVideoFrameSize,
                                            uint32_t dwMaxPayloadTransferSize)
{
    intf->class_interface_handler = video_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = video_notify_handler;

    g_usbd_video[busid].info[0].bDescriptorSubtype = VIDEO_VC_INPUT_TERMINAL_DESCRIPTOR_SUBTYPE;
    g_usbd_video[busid].info[0].bEntityId = 0x01;
    g_usbd_video[busid].info[0].wTerminalType = VIDEO_ITT_CAMERA;
    g_usbd_video[busid].info[1].bDescriptorSubtype = VIDEO_VC_OUTPUT_TERMINAL_DESCRIPTOR_SUBTYPE;
    g_usbd_video[busid].info[1].bEntityId = 0x03;
    g_usbd_video[busid].info[1].wTerminalType = 0x00;
    g_usbd_video[busid].info[2].bDescriptorSubtype = VIDEO_VC_PROCESSING_UNIT_DESCRIPTOR_SUBTYPE;
    g_usbd_video[busid].info[2].bEntityId = 0x02;
    g_usbd_video[busid].info[2].wTerminalType = 0x00;

    usbd_video_probe_and_commit_controls_init(busid, dwFrameInterval, dwMaxVideoFrameSize, dwMaxPayloadTransferSize);
    return intf;
}

bool usbd_video_stream_split_transfer(uint8_t busid, uint8_t ep)
{
    struct video_payload_header *header;
    static uint32_t offset = 0;
    static uint32_t len = 0;

    if (g_usbd_video[busid].stream_finish) {
        g_usbd_video[busid].stream_finish = false;
        return true;
    }

    offset = g_usbd_video[busid].stream_offset;

    len = MIN(g_usbd_video[busid].stream_len,
              g_usbd_video[busid].probe.dwMaxPayloadTransferSize -
                  g_usbd_video[busid].stream_headerlen);

    if (g_usbd_video[busid].do_copy) {
        header = (struct video_payload_header *)&g_usbd_video[busid].ep_buf[0];
        usb_memcpy(&g_usbd_video[busid].ep_buf[g_usbd_video[busid].stream_headerlen], &g_usbd_video[busid].stream_buf[offset], len);
    } else {
        header = (struct video_payload_header *)&g_usbd_video[busid].stream_buf[offset - g_usbd_video[busid].stream_headerlen];
    }

    memset(header, 0, g_usbd_video[busid].stream_headerlen);
    header->bHeaderLength = g_usbd_video[busid].stream_headerlen;
    header->headerInfoUnion.bmheaderInfo = 0;
    header->headerInfoUnion.headerInfoBits.endOfHeader = 1;
    header->headerInfoUnion.headerInfoBits.endOfFrame = 0;
    header->headerInfoUnion.headerInfoBits.frameIdentifier = g_usbd_video[busid].stream_frameid;

    g_usbd_video[busid].stream_offset += len;
    g_usbd_video[busid].stream_len -= len;

    if (g_usbd_video[busid].stream_len == 0) {
        header->headerInfoUnion.headerInfoBits.endOfFrame = 1;
        g_usbd_video[busid].stream_frameid ^= 1;
        g_usbd_video[busid].stream_finish = true;
    }

    if (g_usbd_video[busid].do_copy) {
        usbd_ep_start_write(busid, ep,
                            g_usbd_video[busid].ep_buf,
                            g_usbd_video[busid].stream_headerlen + len);
    } else {
        usbd_ep_start_write(busid, ep,
                            &g_usbd_video[busid].stream_buf[offset - g_usbd_video[busid].stream_headerlen],
                            g_usbd_video[busid].stream_headerlen + len);
    }

    return false;
}

int usbd_video_stream_start_write(uint8_t busid, uint8_t ep, uint8_t *ep_buf, uint8_t *stream_buf, uint32_t stream_len, bool do_copy)
{
    struct video_payload_header *header;

    if ((usb_device_is_configured(busid) == 0) || (stream_len == 0)) {
        return -1;
    }

    g_usbd_video[busid].ep_buf = ep_buf;
    g_usbd_video[busid].stream_buf = stream_buf;
    g_usbd_video[busid].stream_len = stream_len;
    g_usbd_video[busid].stream_offset = 0;
    g_usbd_video[busid].stream_finish = false;
    g_usbd_video[busid].do_copy = do_copy;

    uint32_t len = MIN(g_usbd_video[busid].stream_len,
                       g_usbd_video[busid].probe.dwMaxPayloadTransferSize -
                           g_usbd_video[busid].stream_headerlen);

    header = (struct video_payload_header *)&ep_buf[0];
    header->bHeaderLength = g_usbd_video[busid].stream_headerlen;
    header->headerInfoUnion.bmheaderInfo = 0;
    header->headerInfoUnion.headerInfoBits.endOfHeader = 1;
    header->headerInfoUnion.headerInfoBits.endOfFrame = 0;
    header->headerInfoUnion.headerInfoBits.frameIdentifier = g_usbd_video[busid].stream_frameid;

    usb_memcpy(&ep_buf[g_usbd_video[busid].stream_headerlen], stream_buf, len);
    g_usbd_video[busid].stream_offset += len;
    g_usbd_video[busid].stream_len -= len;

    usbd_ep_start_write(busid, ep, ep_buf, g_usbd_video[busid].stream_headerlen + len);
    return 0;
}

__WEAK void usbd_video_open(uint8_t busid, uint8_t intf)
{
    (void)busid;
    (void)intf;
}

__WEAK void usbd_video_close(uint8_t busid, uint8_t intf)
{
    (void)busid;
    (void)intf;
}