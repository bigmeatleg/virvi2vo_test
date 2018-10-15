//#define LOG_NDEBUG 0
#define LOG_TAG "sample_virvi2vo"
#include <utils/plat_log.h>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "menu.h"
#include "common.h"
#include "mpp_menu.h"

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"

#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <confparser.h>
#include "sample_virvi2vo_config.h"
#include "sample_virvi2vo.h"
#include "mpi_isp.h"


SampleVIRVI2VOContext *pSampleVIRVI2VOContext = NULL;

/************************************************************************************************/
/*                                    Function Declarations                                     */
/************************************************************************************************/
static int mpp_menu_isp_set_ispdev(void *pData, char *pTitle);
static int mpp_menu_isp_brightness_set(void *pData, char *pTitle);
static int mpp_menu_isp_brightness_get(void *pData, char *pTitle);
static int mpp_menu_isp_contrast_set(void *pData, char *pTitle);
static int mpp_menu_isp_contrast_get(void *pData, char *pTitle);
static int mpp_menu_isp_saturation_set(void *pData, char *pTitle);
static int mpp_menu_isp_saturation_get(void *pData, char *pTitle);
static int mpp_menu_isp_sharpness_set(void *pData, char *pTitle);
static int mpp_menu_isp_sharpness_get(void *pData, char *pTitle);
static int mpp_menu_isp_flicker_set(void *pData, char *pTitle);
static int mpp_menu_isp_flicker_get(void *pData, char *pTitle);
static int mpp_menu_isp_colorTemp_set(void *pData, char *pTitle);
static int mpp_menu_isp_colorTemp_get(void *pData, char *pTitle);
static int mpp_menu_isp_exposure_set(void *pData, char *pTitle);
static int mpp_menu_isp_exposure_get(void *pData, char *pTitle);
static int mpp_menu_isp_exposure_time_set(void *pData, char *pTitle);
static int mpp_menu_isp_exposure_gain_set(void *pData, char *pTitle);
static int mpp_menu_isp_PltmWDR_set(void *pData, char *pTitle);
static int mpp_menu_isp_PltmWDR_get(void *pData, char *pTitle);
static int mpp_menu_isp_3NRAttr_set(void *pData, char *pTitle);
static int mpp_menu_isp_3NRAttr_get(void *pData, char *pTitle);
static int ircut_control(void *pData, char *pTitle);


/************************************************************************************************/
/*                                      Global Variables                                        */
/************************************************************************************************/
static MENU_INODE g_isp_menu_list[] = {
    /*  (Title),     (Function),    (Data),    (SubMenu)   */
    {(char *)"isp set brightness",    mpp_menu_isp_brightness_set,   NULL, NULL},
    {(char *)"isp set contrast",      mpp_menu_isp_contrast_set,     NULL, NULL},
    {(char *)"isp set saturation",    mpp_menu_isp_saturation_set,   NULL, NULL},
    {(char *)"isp set sharpness",     mpp_menu_isp_sharpness_set,    NULL, NULL},
    {(char *)"isp set flicker",       mpp_menu_isp_flicker_set,      NULL, NULL},
    {(char *)"isp set colorTemp",     mpp_menu_isp_colorTemp_set,    NULL, NULL},
    {(char *)"isp set exposure",      mpp_menu_isp_exposure_set,     NULL, NULL},
    {(char *)"isp set PltmWDR",       mpp_menu_isp_PltmWDR_set,      NULL, NULL},
    {(char *)"isp set 3NRAttr",       mpp_menu_isp_3NRAttr_set,      NULL, NULL},
    {(char *)"Previous Step (Quit ISP setting)", ExitCurrentMenuLevel, NULL, NULL},
    {NULL, NULL, NULL, NULL},
};

int menu_wait;

int g_ae_mode, g_exposure_bias, g_exposure, g_exposure_gain, g_ae_gain, g_awb_mode, g_color_temp, g_brightness, g_contrast, g_staturation, g_sharpness;

/************************************************************************************************/
/*                                    Structure Declarations                                    */
/************************************************************************************************/
/* None */

static ISP_DEV g_isp_dev = 1;

int initSampleVIRVI2VOContext(SampleVIRVI2VOContext *pContext)
{
    memset(pContext, 0, sizeof(SampleVIRVI2VOContext));
    pContext->mUILayer = HLAY(2, 0);
    cdx_sem_init(&pContext->mSemExit, 0);
    return 0;
}

int destroySampleVIRVI2VOContext(SampleVIRVI2VOContext *pContext)
{
    cdx_sem_deinit(&pContext->mSemExit);
    return 0;
}

static ERRORTYPE SampleVIRVI2VO_VOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;

    SampleVIRVI2VOContext *pContext = (SampleVIRVI2VOContext*)cookie;
    if(MOD_ID_VOU == pChn->mModId) {
        if(pChn->mChnId != pContext->mVOChn) {
            aloge("fatal error! VO chnId[%d]!=[%d]", pChn->mChnId, pContext->mVOChn);
        }
        switch(event) {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER: {
            aloge("fatal error! sample_virvi2vo use binding mode!");
            break;
        }
        case MPP_EVENT_SET_VIDEO_SIZE: {
            SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
            alogd("vo report video display size[%dx%d]", pDisplaySize->Width, pDisplaySize->Height);
            break;
        }
        case MPP_EVENT_RENDERING_START: {
            alogd("vo report rendering start");
			menu_wait = 0;
            break;
        }
        default: {
            //postEventFromNative(this, event, 0, 0, pEventData);
            aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
            ret = ERR_VO_ILLEGAL_PARAM;
            break;
        }
        }
    } else {
        aloge("fatal error! why modId[0x%x]?", pChn->mModId);
        ret = FAILURE;
    }

    return ret;
}

static ERRORTYPE SampleVIRVI2VO_CLOCKCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    alogw("clock channel[%d] has some event[0x%x]", pChn->mChnId, event);
    return SUCCESS;
}

static int ParseCmdLine(int argc, char **argv, SampleVIRVI2VOCmdLineParam *pCmdLinePara)
{
    alogd("sample_virvi2vo path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleVIRVI2VOCmdLineParam));
    while(i < argc) {
        if(!strcmp(argv[i], "-path")) {
            if(++i >= argc) {
                aloge("fatal error! use -h to learn how to set parameter!!!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE) {
                aloge("fatal error! file path[%s] too long: [%d]>=[%d]!", argv[i], strlen(argv[i]), MAX_FILE_PATH_SIZE);
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], MAX_FILE_PATH_SIZE-1);
            pCmdLinePara->mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        } else if(!strcmp(argv[i], "-h")) {
            printf("CmdLine param example:\n"
                   "\t run -path /home/sample_virvi2vo.conf\n");
            ret = 1;
            break;
        } else {
            alogd("ignore invalid CmdLine param:[%s], type -h to get how to set parameter!", argv[i]);
        }
        i++;
    }
    return ret;
}

static ERRORTYPE loadSampleVIRVI2VOConfig(SampleVIRVI2VOConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr;
    if(NULL == conf_path) {
        alogd("user not set config file. use default test parameter!");
        pConfig->mDevNum = 0;
        pConfig->mCaptureWidth = 1920;
        pConfig->mCaptureHeight = 1080;
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        pConfig->mFrameRate = 30;

		pConfig->mDispType = VO_INTF_HDMI;
		pConfig->mDisplayWidth = 1920;
		pConfig->mDisplayHeight = 1080;
		pConfig->mDispSync = VO_OUTPUT_1080P30;
		
        pConfig->mTestDuration = 0;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0) {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleVIRVI2VOConfig));
    pConfig->mDevNum        = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_DEVICE_NUMBER, 0);
    pConfig->mCaptureWidth  = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_CAPTURE_WIDTH, 0);
    pConfig->mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_CAPTURE_HEIGHT, 0);
    pConfig->mDisplayWidth  = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_DISPLAY_WIDTH, 0);
    pConfig->mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_DISPLAY_HEIGHT, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_VIRVI2VO_KEY_PIC_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "yu12")) {
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    } else if(!strcmp(pStrPixelFormat, "yv12")) {
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    } else if(!strcmp(pStrPixelFormat, "nv21")) {
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    } else if(!strcmp(pStrPixelFormat, "nv12")) {
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    } else {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    char *pStrDispType = (char*)GetConfParaString(&stConfParser, SAMPLE_VIRVI2VO_KEY_DISP_TYPE, NULL);
    if (!strcmp(pStrDispType, "hdmi")) {
        pConfig->mDispType = VO_INTF_HDMI;
        if (pConfig->mDisplayWidth > 1920)
            pConfig->mDispSync = VO_OUTPUT_3840x2160_30;
        else if (pConfig->mDisplayWidth > 1280)
            pConfig->mDispSync = VO_OUTPUT_1080P30;
        else
            pConfig->mDispSync = VO_OUTPUT_720P60;
    } else if (!strcmp(pStrDispType, "lcd")) {
        pConfig->mDispType = VO_INTF_LCD;
        pConfig->mDispSync = VO_OUTPUT_NTSC;
    } else if (!strcmp(pStrDispType, "cvbs")) {
        pConfig->mDispType = VO_INTF_CVBS;
        pConfig->mDispSync = VO_OUTPUT_NTSC;
    }

    pConfig->mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_FRAME_RATE, 0);
    pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_TEST_DURATION, 0);

    alogd("pConfig->mCaptureWidth=[%d], pConfig->mCaptureHeight=[%d], pConfig->mDevNum=[%d]",
          pConfig->mCaptureWidth, pConfig->mCaptureHeight, pConfig->mDevNum);
    alogd("pConfig->mDisplayWidth=[%d], pConfig->mDisplayHeight=[%d], pConfig->mDispSync=[%d], pConfig->mDispType=[%d]",
          pConfig->mDisplayWidth, pConfig->mDisplayHeight, pConfig->mDispSync, pConfig->mDispType);
    alogd("pConfig->mFrameRate=[%d], pConfig->mTestDuration=[%d]",
          pConfig->mFrameRate, pConfig->mTestDuration);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != pSampleVIRVI2VOContext) {
        cdx_sem_up(&pSampleVIRVI2VOContext->mSemExit);
    }
}

int setISP_AEMode(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\n**************** Setting AE Mode *************************\n");
        printf(" Please Input 0 or 1(defualte:0) or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0 == value || value == 1) {
			AW_MPI_ISP_AE_SetMode(iISPDev, value);
			AW_MPI_ISP_AE_GetMode(iISPDev, &value);
            printf(" Set AE Mode:%d ok!\n", value);
			g_ae_mode = value;
            //return 0;
        } else {
            printf(" Set AE Mode:%d error!\n", value);
            continue;
        }
    }
}

int setISP_AWBMode(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\n**************** Setting AWB Mode *************************\n");
        printf(" Please Input 0 or 1 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0 == value || value == 1) {
			AW_MPI_ISP_AWB_SetMode(iISPDev, value);
			AW_MPI_ISP_AWB_GetMode(iISPDev, &value);
            printf(" Set AWB Mode:%d ok!\n", value);
			g_awb_mode = value;
            //return 0;
        } else {
            printf(" Set AWB Mode:%d error!\n", value);
            continue;
        }
    }
}


int setISP_ExposureBias(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\r\n**************** Setting Exposure Bias *************************\n");
        printf(" Please Input 1 ~ 8 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0 < value && value <= 8) {
			AW_MPI_ISP_AE_SetExposureBias(iISPDev, value);
			AW_MPI_ISP_AE_GetExposureBias(iISPDev, &value);
            printf(" Set AE ExposureBias:%d ok!\n", value);
			g_exposure_bias = value;
            //return 0;
        } else {
            printf(" Set AE ExposureBias:%d error!\n", value);
            continue;
        }
    }
}

int setISP_Exposure(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\n**************** Setting Exposure Time *************************\n");
        printf(" Please Input exposure time val or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0<= value) {
			AW_MPI_ISP_AE_SetExposure(iISPDev, value);
			AW_MPI_ISP_AE_GetExposure(iISPDev, &value);
            printf(" Set AE Exposure Time:%d ok!\n", value);
			g_exposure = value;
            //return 0;
        } else {
            printf(" Set AE Exposure Time:%d error!\n", value);
            continue;
        }
    }
}

int setISP_ColorTemp(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\r\n**************** Setting Color Temp *************************\n");
		printf("手動白平衡才能調整\n");
		printf("1:正常 2：日光燈 3：螢光燈 4：高亮螢光燈 5：日出 6：日光 7：閃光燈 8：多雲\n");
        printf(" Please Input 2 ~ 8 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0< value && value < 9) {
			AW_MPI_ISP_AWB_SetColorTemp(iISPDev, value);
			AW_MPI_ISP_AWB_GetColorTemp(iISPDev, &value);
            printf(" Set Color Temp:%d ok!\n", value);
			g_color_temp = value;
            //return 0;
        } else {
            printf(" Set Color Temp:%d error!\n", value);
            continue;
        }
    }
}


int setISP_AEGain(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\n**************** Setting AE Gain *************************\n");
        printf(" Please Input 1600 ~ 36000 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0<= value && value <= 65535) {
			AW_MPI_ISP_AE_SetGain(iISPDev, value);
			AW_MPI_ISP_AE_GetGain(iISPDev, &value);
            printf(" Set AE Gain:%d ok!\n", value);
			g_ae_gain = value;
            //return 0;
        } else {
            printf(" Set AE Gain:%d error!\n", value);
            continue;
        }
    }
}

int setISP_Brightness(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\n**************** Setting Brightness *************************\n");
        printf(" Please Input 0 ~ 252 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0<= value && value <= 252) {
			/*input 0~252,but the SetBrightness() need -126~126, so switch value */
            value = value - 126;
			AW_MPI_ISP_SetBrightness(iISPDev, value);
			AW_MPI_ISP_GetBrightness(iISPDev, &value);
            printf(" Set Brightness:%d ok!\n", value);
			g_brightness = value;
            //return 0;
        } else {
            printf(" Set Brightness:%d error!\n", value);
            continue;
        }
    }
}


int setISP_Contrast(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\n**************** Setting Contrast *************************\n");
        printf(" Please Input 0 ~ 252 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0<= value && value <= 252) {
			/*input 0~252,but the SetBrightness() need -126~126, so switch value */
            value = value - 126;
			AW_MPI_ISP_SetContrast(iISPDev, value);
			AW_MPI_ISP_GetContrast(iISPDev, &value);
            printf(" Set Contrast:%d ok!\n", value);
			g_contrast = value;
            //return 0;
        } else {
            printf(" Set Contrast:%d error!\n", value);
            continue;
        }
    }
}

int setISP_Saturation(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\n**************** Setting Saturation *************************\n");
        printf(" Please Input 0 ~ 512 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0<= value && value <= 512) {
			value = value - 256;
			AW_MPI_ISP_SetSaturation(iISPDev, value);
			AW_MPI_ISP_GetSaturation(iISPDev, &value);
            printf(" Set Saturation:%d ok!\n", value);
			g_staturation = value;
            //return 0;
        } else {
            printf(" Set Saturation:%d error!\n", value);
            continue;
        }
    }
}


int setISP_Sharpness(int iISPDev)
{
	int  ret       = 0;
    int  value   = 0;
    char str[256]  = {0};
	
	while (1) {
        printf("\n**************** Setting Sharpness *************************\n");
        printf(" Please Input 0 ~ 10 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        value = atoi(str);
        if (0<= value && value <= 10) {
			AW_MPI_ISP_SetSharpness(iISPDev, value);
			AW_MPI_ISP_GetSharpness(iISPDev, &value);
            printf(" Set Sharpness:%d ok!\n", value);
			g_sharpness = value;
            //return 0;
        } else {
            printf(" Set Sharpness:%d error!\n", value);
            continue;
        }
    }
}


int writeConfig()
{
	int output_fd;
	size_t ret_out;
	int size_buf;
	char buf[256];
	
	output_fd = open("./sensor.conf", O_RDWR|O_CREAT|O_TRUNC, 0777);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "[parameter]\n");
	ret_out = write(output_fd, buf, strlen(buf));
	
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", AE_MODE, g_ae_mode);
	ret_out = write(output_fd, buf, strlen(buf));

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", EXPOSURE_BIAS, g_exposure_bias);
	ret_out = write(output_fd, buf, strlen(buf));

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", EXPOSURE_TIME, g_exposure);
	ret_out = write(output_fd, buf, strlen(buf));

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", AE_GAIN, g_ae_gain);
	ret_out = write(output_fd, buf, strlen(buf));

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", AWB_MODE, g_awb_mode);
	ret_out = write(output_fd, buf, strlen(buf));

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", COLOR_TEMP, g_color_temp);
	ret_out = write(output_fd, buf, strlen(buf));

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", BRIGHTNESS, g_brightness);
	ret_out = write(output_fd, buf, strlen(buf));

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", CONTRAST, g_contrast);
	ret_out = write(output_fd, buf, strlen(buf));

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", SATURATION, g_staturation);
	ret_out = write(output_fd, buf, strlen(buf));

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s = %d\n", SHARPNESS, g_sharpness);
	ret_out = write(output_fd, buf, strlen(buf));
	
	close(output_fd);	

	return 0;
}

void LoadConfig(int iIspDev)
{
	int ret;
	if(access("./sensor.conf", F_OK ) == -1 ) return;

	CONFPARSER_S stConfParser;
    ret = createConfParser("./sensor.conf", &stConfParser);
    if(ret < 0) {
        aloge("load conf fail");
        return FAILURE;
    }

	g_ae_mode =  GetConfParaInt(&stConfParser, AE_MODE, 0);
	AW_MPI_ISP_AE_SetMode(iIspDev, g_ae_mode);
    
	g_exposure_bias = GetConfParaInt(&stConfParser, EXPOSURE_BIAS, 0);
	AW_MPI_ISP_AE_SetExposureBias(iIspDev,g_exposure_bias);
	
	g_exposure = GetConfParaInt(&stConfParser, EXPOSURE_TIME, 0);
	AW_MPI_ISP_AE_SetExposure(iIspDev, g_exposure);
	
	g_ae_gain = GetConfParaInt(&stConfParser, AE_GAIN, 0);
	AW_MPI_ISP_AE_SetGain(iIspDev, g_ae_gain);//256
	
	g_awb_mode = GetConfParaInt(&stConfParser, AWB_MODE, 0);
	AW_MPI_ISP_AWB_SetMode(iIspDev, g_awb_mode);//0,1
	
	g_color_temp = GetConfParaInt(&stConfParser, COLOR_TEMP, 0);
	AW_MPI_ISP_AWB_SetColorTemp(iIspDev, g_color_temp);//0,1,2,3,4,5,6,7,
	
	g_brightness = GetConfParaInt(&stConfParser, BRIGHTNESS, 0);
	AW_MPI_ISP_SetBrightness(iIspDev,g_brightness);//0~256
	
	g_contrast = GetConfParaInt(&stConfParser, CONTRAST, 0);
	AW_MPI_ISP_SetContrast(iIspDev,g_contrast);//0~256
	
	g_staturation = GetConfParaInt(&stConfParser, SATURATION, 0);
	AW_MPI_ISP_SetSaturation(iIspDev,g_staturation);//0~256
	
	g_sharpness = GetConfParaInt(&stConfParser, SHARPNESS, 0);
	AW_MPI_ISP_SetSharpness(iIspDev, g_sharpness);//0~256
	

	destroyConfParser(&stConfParser);
}

int ListMenu()
{
	
printf("\033[33m");
printf("===========Sensor Test Mode=========\n");
printf("input <99> & ctrl+c to exit!\n");
printf("input <1-10> to test\n");
printf("\033[0m");
printf("\n");
printf("1: Set AE Mode\n");
printf("2: Set Set Exposure Bias\n");
printf("3: Set Set Exposure Time\n");
printf("4: Set AE Gain\n");
printf("5: Set AWB Mode\n");
printf("6: Set Color Temperture\n");
printf("7: Set Brightness\n");
printf("8: Set Contrast\n");
printf("9: Set Saturation\n");
printf("10: Set Sharpness\n");
printf("11: Save Configuration\n");
printf("12: Load Configuration\n");

printf("\n");
printf("AE_Mode[%d], ExposureBias[%d], ExposureTime[%d], AE_Gain[%d], AWB_Mode[%d]\n", g_ae_mode, g_exposure_bias, g_exposure, g_ae_gain, g_awb_mode);
printf("Color_Temp[%d], Brightness[%d], Contrast[%d], Staturation[%d], Sharpness[%d]\n", g_color_temp, g_brightness, g_contrast, g_staturation, g_sharpness);


printf("\n");
printf("\033[33m");
printf("============================\n");
printf("input control number:");
printf("\033[0m");
}

void getSettingValue(int iIspDev)
{
	int vl;
	
	AW_MPI_ISP_AE_GetMode(iIspDev,&vl);
	g_ae_mode = vl;

    AW_MPI_ISP_AE_GetExposureBias(iIspDev,&vl);
	g_exposure_bias = vl;
	
    AW_MPI_ISP_AE_GetExposure(iIspDev, &vl);
	g_exposure = vl;
	
    AW_MPI_ISP_AE_GetGain(iIspDev,&vl);//256
    g_ae_gain = vl;

    AW_MPI_ISP_AWB_GetMode(iIspDev,&vl);//0,1
    g_awb_mode = vl;

	AW_MPI_ISP_AWB_GetColorTemp(iIspDev,&vl);//0,1,2,3,4,5,6,7,
    g_color_temp = vl;

	AW_MPI_ISP_GetBrightness(iIspDev,&vl);//0~256
	g_brightness = vl; 

    AW_MPI_ISP_GetContrast(iIspDev,&vl);//0~256
    g_contrast = vl;

	AW_MPI_ISP_GetSaturation(iIspDev,&vl);//0~256
	g_staturation = vl;
    
    AW_MPI_ISP_GetSharpness(iIspDev,&vl);//0~256
    g_sharpness = vl;
	
}


int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
    int result = 0;
    int iIspDev;

    SampleVIRVI2VOContext stContext;

    printf("sample_virvi2vo running!\n");
    initSampleVIRVI2VOContext(&stContext);
    pSampleVIRVI2VOContext = &stContext;

    /* parse command line param */
    if(ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0) {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(stContext.mCmdLinePara.mConfigFilePath) > 0) {
        pConfigFilePath = stContext.mCmdLinePara.mConfigFilePath;
    } else {
        pConfigFilePath = NULL;
    }

    /* parse config file. */
    if(loadSampleVIRVI2VOConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS) {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    /* init mpp system */
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    result = AW_MPI_SYS_Init();
    if (result) {
        aloge("fatal error! AW_MPI_SYS_Init failed");
        goto sys_init_err;
    }

    /* create vi channel */
    stContext.mVIDev = stContext.mConfigPara.mDevNum;
    stContext.mVIChn = 0;
    alogd("Vipp dev[%d] vir_chn[%d]", stContext.mVIDev, stContext.mVIChn);
    ERRORTYPE eRet = AW_MPI_VI_CreateVipp(stContext.mVIDev);
    if (eRet != SUCCESS) {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
        result = eRet;
        goto vi_create_vipp_err;
    }
    VI_ATTR_S stAttr;
    eRet = AW_MPI_VI_GetVippAttr(stContext.mVIDev, &stAttr);
    if (eRet != SUCCESS) {
        aloge("fatal error! AW_MPI_VI GetVippAttr failed");
    }
    memset(&stAttr, 0, sizeof(VI_ATTR_S));
    stAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    stAttr.memtype = V4L2_MEMORY_MMAP;
    stAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(stContext.mConfigPara.mPicFormat);
    stAttr.format.field = V4L2_FIELD_NONE;
    // stAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    stAttr.format.width = stContext.mConfigPara.mCaptureWidth;
    stAttr.format.height = stContext.mConfigPara.mCaptureHeight;
    stAttr.nbufs = 10;
    stAttr.nplanes = 2;
    /* do not use current param, if set to 1, all this configuration will
     * not be used.
     */
    stAttr.use_current_win = 0;
    stAttr.fps = stContext.mConfigPara.mFrameRate;
    eRet = AW_MPI_VI_SetVippAttr(stContext.mVIDev, &stAttr);
    if (eRet != SUCCESS) {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }
    eRet = AW_MPI_VI_EnableVipp(stContext.mVIDev);
    if(eRet != SUCCESS) {
        aloge("fatal error! enableVipp fail!");
        result = eRet;
        goto vi_enable_vipp_err;
    }
#define ISP_RUN 1
#if ISP_RUN
    /* open isp */
    if (stContext.mVIDev == 0 || stContext.mVIDev == 2) {
        iIspDev = 1;
    } else if (stContext.mVIDev == 1 || stContext.mVIDev == 3) {
        iIspDev = 0;
    }
    AW_MPI_ISP_Init();
    AW_MPI_ISP_Run(iIspDev);
    AW_MPI_ISP_SetMirror(stContext.mVIDev,0);//0,1
    // AW_MPI_ISP_SetFlip(2,0);//0,1
    // AW_MPI_ISP_SetFlip(2,1);//0,1
#endif
    eRet = AW_MPI_VI_CreateVirChn(stContext.mVIDev, stContext.mVIChn, NULL);
    if(eRet != SUCCESS) {
        aloge("fatal error! createVirChn[%d] fail!", stContext.mVIChn);
    }
    /* enable vo dev */
    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer); /* close ui layer. */
    /* enable vo layer */
    int hlay0 = 0;
    while(hlay0 < VO_MAX_LAYER_NUM) {
        if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0)) {
            break;
        }
        hlay0++;
    }
    if(hlay0 >= VO_MAX_LAYER_NUM) {
        aloge("fatal error! enable video layer fail!");
    }

#if 1
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(stContext.mVoDev, &spPubAttr);
    spPubAttr.enIntfType = stContext.mConfigPara.mDispType;
    spPubAttr.enIntfSync = stContext.mConfigPara.mDispSync;
    AW_MPI_VO_SetPubAttr(stContext.mVoDev, &spPubAttr);
#endif

    stContext.mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 0;
    stContext.mLayerAttr.stDispRect.Width = stContext.mConfigPara.mDisplayWidth;
    stContext.mLayerAttr.stDispRect.Height = stContext.mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);

    /* create vo channel and clock channel.
    (because frame information has 'pts', there is no need clock channel now)
    */
    BOOL bSuccessFlag = FALSE;
    stContext.mVOChn = 0;
    while(stContext.mVOChn < VO_MAX_CHN_NUM) {
        eRet = AW_MPI_VO_EnableChn(stContext.mVoLayer, stContext.mVOChn);
        if(SUCCESS == eRet) {
            bSuccessFlag = TRUE;
            alogd("create vo channel[%d] success!", stContext.mVOChn);
            break;
        } else if(ERR_VO_CHN_NOT_DISABLE == eRet) {
            alogd("vo channel[%d] is exist, find next!", stContext.mVOChn);
            stContext.mVOChn++;
        } else {
            aloge("fatal error! create vo channel[%d] ret[0x%x]!", stContext.mVOChn, eRet);
            break;
        }
    }
    if(FALSE == bSuccessFlag) {
        stContext.mVOChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        result = -1;
        goto vo_create_chn_err;
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleVIRVI2VO_VOCallbackWrapper;
    AW_MPI_VO_RegisterCallback(stContext.mVoLayer, stContext.mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(stContext.mVoLayer, stContext.mVOChn, 2);

    /* bind clock,vo, viChn
    (because frame information has 'pts', there is no need to bind clock channel now)
    */
    MPP_CHN_S VOChn = {MOD_ID_VOU, stContext.mVoLayer, stContext.mVOChn};
    MPP_CHN_S VIChn = {MOD_ID_VIU, stContext.mVIDev, stContext.mVIChn};
    AW_MPI_SYS_Bind(&VIChn, &VOChn);

    /* start vo, vi_channel. */
    eRet = AW_MPI_VI_EnableVirChn(stContext.mVIDev, stContext.mVIChn);
    if(eRet != SUCCESS) {
        aloge("fatal error! enableVirChn fail!");
        result = eRet;
        goto vi_enable_virchn_err;
    }
    AW_MPI_VO_StartChn(stContext.mVoLayer, stContext.mVOChn);

	menu_wait = 1;
	while(menu_wait){
		sleep(1);
	}

	getSettingValue(iIspDev);
    char str[256]  = {0};
    int num = 0, vl = 0;

    while (1) {
		ListMenu();
        memset(str, 0, sizeof(str));
        fgets(str, 256, stdin);
        num = atoi(str);
		
        if (99 == num) {
            printf("break test.\n");
            printf("enter ctrl+c to exit this program.\n");
            break;
        }

        switch (num) {
        case 1:
            //AW_MPI_ISP_AE_SetMode(iIspDev,0);//0 ,1---ok
            //AW_MPI_ISP_AE_GetMode(iIspDev,&vl);
            printf("[0]Auto exposure \n");
        	printf("[1]Manual exposure \n");
            printf("AE mode: current value = %d.\r\n", g_ae_mode);
			setISP_AEMode(iIspDev);
            break;
        case 2:
            //AW_MPI_ISP_AE_SetExposureBias(iIspDev,4);//0~8---ok
            //AW_MPI_ISP_AE_GetExposureBias(iIspDev,&vl);
            printf("AE exposure bias: current value = %d.\r\n", g_exposure_bias);
			setISP_ExposureBias(iIspDev);
            break;
        case 3:
            //AW_MPI_ISP_AE_SetExposure(iIspDev,1000);//
            AW_MPI_ISP_AE_GetExposure(iIspDev, &vl);
            printf("AE exposure time: current value = %d.\r\n", g_exposure);
			setISP_Exposure(iIspDev);
            break;
        case 4:
            //AW_MPI_ISP_AE_SetGain(iIspDev,256);//256
            AW_MPI_ISP_AE_GetGain(iIspDev,&vl);//256
            printf("AE gain: current value = %d.\r\n", g_ae_gain);
			setISP_AEGain(iIspDev);
            break;
        case 5:
            //AW_MPI_ISP_AWB_SetMode(iIspDev,0);//0,1---ok
            AW_MPI_ISP_AWB_GetMode(iIspDev,&vl);//0,1
            printf("AWB mode: current value = %d.\r\n", g_awb_mode);
			setISP_AWBMode(iIspDev);
            break;
        case 6:
            //AW_MPI_ISP_AWB_SetColorTemp(iIspDev,4);//0,1,2,3,4,5,6,7, ---ok
            AW_MPI_ISP_AWB_GetColorTemp(iIspDev,&vl);//0,1,2,3,4,5,6,7,
            printf("AWB clolor temperature: current value = %d.\r\n", g_color_temp);
			setISP_ColorTemp(iIspDev);
            break;
        case 7:
			//AW_MPI_ISP_SetBrightness(iIspDev,100);//0~256
            AW_MPI_ISP_GetBrightness(iIspDev,&vl);//0~256
            printf("brightness: current value = %d.\r\n", g_brightness);
			setISP_Brightness(iIspDev);
			break;
        case 8:
            //AW_MPI_ISP_SetContrast(iIspDev,100);//0~256
            AW_MPI_ISP_GetContrast(iIspDev,&vl);//0~256
            printf("contrast: current value = %d.\r\n", g_contrast);
			setISP_Contrast(iIspDev);
            break;
        case 9:
            //AW_MPI_ISP_SetSaturation(iIspDev,100);//0~256
            AW_MPI_ISP_GetSaturation(iIspDev,&vl);//0~256
            printf("saturation: current value = %d.\r\n", g_staturation);
			setISP_Saturation(iIspDev);
            break;
        case 10:
            //AW_MPI_ISP_SetSharpness(iIspDev,100);//0~256
            AW_MPI_ISP_GetSharpness(iIspDev,&vl);//0~256
            printf("sharpness: current value = %d.\r\n", g_sharpness);
			setISP_Sharpness(iIspDev);
            break;
		case 11:
			writeConfig();
			break;
		case 12:
			LoadConfig(iIspDev);
			break;
        default:
            printf("intput error.\r\n");
            break;
        }
    }

    if(stContext.mConfigPara.mTestDuration > 0) {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mTestDuration*1000);
    } else {
        cdx_sem_down(&stContext.mSemExit);
    }
    /* stop vo channel, vi channel */
    AW_MPI_VO_StopChn(stContext.mVoLayer, stContext.mVOChn);
    AW_MPI_VI_DisableVirChn(stContext.mVIDev, stContext.mVIChn);

vi_enable_virchn_err:
    AW_MPI_VO_DisableChn(stContext.mVoLayer, stContext.mVOChn);
    stContext.mVOChn = MM_INVALID_CHN;

vo_create_chn_err:
    AW_MPI_VI_DestoryVirChn(stContext.mVIDev, stContext.mVIChn);

#if ISP_RUN
    AW_MPI_ISP_Stop(iIspDev);
    AW_MPI_ISP_Exit();
#endif
    AW_MPI_VI_DisableVipp(stContext.mVIDev);
    AW_MPI_VI_DestoryVipp(stContext.mVIDev);
    stContext.mVIDev = MM_INVALID_DEV;
    stContext.mVIChn = MM_INVALID_CHN;

    /* disable vo layer */
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;

vi_enable_vipp_err:
vi_create_vipp_err:
    /* exit mpp system */
    AW_MPI_SYS_Exit();
sys_init_err:
_exit:
    destroySampleVIRVI2VOContext(&stContext);
    if (result == 0) {
        printf("sample_virvi2vo exit!\n");
    }

    return result;
}


/************************************************************************************************/
/*                                     Function Definitions                                     */
/************************************************************************************************/
int mpp_menu_isp_get(PMENU_INODE *pmenu_list_isp)
{
    if (NULL == pmenu_list_isp) {
        DB_PRT("Input menu_list_isp is NULL!\n");
        return -1;
    }

    *pmenu_list_isp = g_isp_menu_list;

    return 0;
}

static int mpp_menu_isp_set_ispdev(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  isp_dev   = 0;
    char str[256]  = {0};

    while (1) {
        printf("\n**************** Setting ISP device Number *************************\n");
        printf(" Please Input ISP_DEV 0 or 1(defualte:0) or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        isp_dev = atoi(str);
        if (0 == isp_dev || isp_dev == 1) {
            g_isp_dev = isp_dev;
            printf(" Setting ISP_DEV:%d ok!\n", isp_dev);
            return 0;
        } else {
            printf(" Input ISP_DEV:%d error!\n", isp_dev);
            continue;
        }
    }

    return 0;
}


static int mpp_menu_isp_brightness_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};

	mpp_menu_isp_brightness_get(pData, pTitle);
    while (1) {
        printf("\n**************** Setting ISP brightness *************************\n");
        printf(" Please Input brightness val 0~252 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 252) {
            /*input 0~252,but the SetBrightness() need -126~126, so switch value */
            val = val - 126;
            ret = AW_MPI_ISP_SetBrightness(g_isp_dev, val);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_SetBrightness succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, val, ret);
                return 0;
            } else {
                ERR_PRT("Do AW_MPI_ISP_SetBrightness fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
        } else {
            printf(" Input Brightness value:%d error!\n", val);
            continue;
        }
    }
}

static int mpp_menu_isp_brightness_get(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;

    printf("\n**************** Get ISP device Number *************************\n");
    ret = AW_MPI_ISP_GetBrightness(g_isp_dev, &val);
	printf("Current Birghtness Value: %d\n", val+126);
    if (0 == ret) {
        DB_PRT("Do AW_MPI_ISP_GetBrightness succeed isp_dev:%d  (value:%d)  ret:%d \n",
               g_isp_dev, (val+126), ret);
        return 0;
    } else {
        ERR_PRT("Do AW_MPI_ISP_GetBrightness fail isp_dev:%d  (value:%d)  ret:%d \n",
                g_isp_dev, (val+126), ret);
        return -1;
    }
}

static int mpp_menu_isp_contrast_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};

	mpp_menu_isp_contrast_get(pData, pTitle);
    while (1) {
        printf("\n**************** Setting ISP contrast *************************\n");
        printf(" Please Input contrast val 0~252 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 252) {
            /*input 0~252,but the SetContrast() need -126~126, so switch value */
            val = val - 126;
            ret = AW_MPI_ISP_SetContrast(g_isp_dev, val);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetContrast succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, val, ret);
                return 0;
            } else {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetContrast fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
        } else {
            printf(" Input Contrast value:%d error!\n", val);
            continue;
        }
    }
}

static int mpp_menu_isp_contrast_get(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;

    printf("\n**************** Get ISP contrast *************************\n");
    ret = AW_MPI_ISP_GetContrast(g_isp_dev, &val);
	printf("Current Contrast Value: %d\n", val+126);
    if (0 == ret) {
        DB_PRT("Do AW_MPI_ISP_GetContrast succeed isp_dev:%d  (value:%d)  ret:%d \n",
               g_isp_dev, (val+126), ret);
        return 0;
    } else {
        ERR_PRT("Do AW_MPI_ISP_GetBrightness fail isp_dev:%d  (value:%d)  ret:%d \n",
                g_isp_dev, (val+126), ret);
        return -1;
    }
}

static int mpp_menu_isp_saturation_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};
    while (1) {
        printf("\n**************** Setting ISP saturation *************************\n");
        printf(" Please Input saturation val 0~512 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 512) {
            /*input 0~200,but the SetContrast() need -100~100, so switch value */
            val = val - 256;
            ret = AW_MPI_ISP_SetSaturation(g_isp_dev, val);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetSaturation succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, (val+100), ret);
                return 0;
            } else {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetSaturation fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, (val+100), ret);
                return -1;
            }
        } else {
            printf(" Input Saturation value:%d error!\n", val);
            continue;
        }
    }
}

static int mpp_menu_isp_saturation_get(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;

    printf("\n**************** Get ISP saturation *************************\n");
    ret = AW_MPI_ISP_GetSaturation(g_isp_dev, &val);
    if (0 == ret) {
        DB_PRT("Do AW_MPI_ISP_GetSaturation succeed isp_dev:%d  (value:%d)  ret:%d \n",
               g_isp_dev, (val+100), ret);
        return 0;
    } else {
        ERR_PRT("Do AW_MPI_ISP_GetSaturation fail isp_dev:%d  (value:%d)  ret:%d \n",
                g_isp_dev, (val+100), ret);
        return -1;
    }
}
static int mpp_menu_isp_sharpness_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};
    while (1) {
        printf("\n**************** Setting ISP sharpness *************************\n");
        printf(" Please Input sharpness val 0~10 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 10) {
            ret = AW_MPI_ISP_SetSharpness(g_isp_dev, val);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetSharpness succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, val, ret);
                return 0;
            } else {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetSharpness fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
        } else {
            printf(" Input sharpness value:%d error!\n", val);
            continue;
        }
    }
}

static int mpp_menu_isp_sharpness_get(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;

    printf("\n**************** Get ISP sharpness *************************\n");
    ret = AW_MPI_ISP_GetSharpness(g_isp_dev, &val);
    if (0 == ret) {
        DB_PRT("Do AW_MPI_ISP_GetSharpness succeed isp_dev:%d  (value:%d)  ret:%d \n",
               g_isp_dev, val, ret);
        return 0;
    } else {
        ERR_PRT("Do AW_MPI_ISP_GetSharpness fail isp_dev:%d  (value:%d)  ret:%d \n",
                g_isp_dev, val, ret);
        return -1;
    }
}

static int mpp_menu_isp_flicker_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};
    while (1) {
        printf("\n**************** Setting ISP flicker *************************\n");
        printf("[0]-Disable\n");
        printf("[1]-50Hz\n");
        printf("[2]-60Hz\n");
        printf("[3]-Auto\n");
        printf(" Please Input  flip value [0]~[3] or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 3) {
            ret = AW_MPI_ISP_SetFlicker(g_isp_dev, val);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_AW_MPI_ISPSetFlicker succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, val, ret);
                return 0;
            } else {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetFlicker fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
        } else {
            printf(" Input flicker value:%d error!\n", val);
            continue;
        }
    }
}

static int mpp_menu_isp_flicker_get(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;

    printf("\n**************** Get ISP mirror *************************\n");
    ret = AW_MPI_ISP_GetFlicker(g_isp_dev, &val);
    if (0 == ret) {
        DB_PRT("Do AW_MPI_ISP_GetFlicker succeed isp_dev:%d  (value:%d)  ret:%d \n",
               g_isp_dev, val, ret);
        switch(val) {
        case 0: {
            printf("flicker status : Disable\n");
            break;
        }
        case 1: {
            printf("flicker status : 50Hz\n");
            break;
        }
        case 2: {
            printf("flicker status : 60Hz\n");
            break;
        }
        case 3: {
            printf("flicker status : Auto\n");
            break;
        }
        default:
            break;
        }
        return 0;
    } else {
        ERR_PRT("Do AW_MPI_ISP_GetFlicker fail isp_dev:%d  (value:%d)  ret:%d \n",
                g_isp_dev, val, ret);
        return -1;
    }
}

static int mpp_menu_isp_colorTemp_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};
    ISP_MODULE_ONOFF ModuleOnOff;

    while(1) {
        printf("\n**************** Setting ISP colorTemp on/off *************************\n");
        printf("[0]-OFF\n");
        printf("[1]-ON\n");
        printf(" Please Input colorTemp on/off 0 or 1 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 == val || val == 1) {
            /*first get all-On/Off status*/
            ret = AW_MPI_ISP_GetModuleOnOff(g_isp_dev, &ModuleOnOff);
            if (0 != ret) {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_GetModuleOnOff fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }

            /*then,set whilte balance On/Off status*/
            /*judge input value is deiffent from current value.if same,pass setting*/
            if(ModuleOnOff.wb != val) {
                ModuleOnOff.manual = 1;
                ModuleOnOff.wb     = val;
                ret = AW_MPI_ISP_SetModuleOnOff(g_isp_dev, &ModuleOnOff);
                if (0 == ret) {
                    DB_PRT("Do AW_MPI_ISP_AW_MPI_SetModuleOnOff succeed isp_dev:%d  value:%d  ret:%d \n",
                           g_isp_dev, val, ret);
                } else {
                    ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetColorTemp on/off fail  isp_dev:%d  value:%d  ret:%d \n",
                            g_isp_dev, val, ret);
                    return -1;
                }
            }

            if (1 == val) { //manual model
                /*Into model of setting, when enabled whilte balance.*/
                goto set_colorTem_model;
            } else {
                return 0;
            }

        } else {
            printf(" Input colorTemp value:%d error!\n", val);
            continue;
        }
    }

set_colorTem_model:
    while (1) {
        printf("\n**************** Setting ISP colorTemp model *************************\n");
        printf("[0]-Manual\n");
        printf("[1]-Auto\n");
        printf("[2]-Incandescent\n");
        printf("[3]-fluorescent\n");
        printf("[4]-fluorescent_h\n");
        printf("[5]-Horizon\n");
        printf("[6]-Daylight\n");
        printf("[7]-Flash\n");
        printf("[8]-Cloudy\n");
        printf("[9]-Shade\n");
        printf(" Please Input colorTemp value [0]~[9] or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 9) {
            ret = AW_MPI_ISP_AWB_SetColorTemp(g_isp_dev, val);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_AW_MPI_SetColorTemp model succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, val, ret);
                return 0;
            } else {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetColorTemp model fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
        } else {
            printf(" Input colorTemp value:%d error!\n", val);
            continue;
        }
    }
}

static int mpp_menu_isp_colorTemp_get(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    ISP_MODULE_ONOFF ModuleOnOff;
    char *module[] = {"Manual","Auto","Incandescent","fluorescent",
                      "fluorescent_h","Horizon","Daylight","Flash","Cloudy","Shade"
                     };


    printf("\n**************** Get ISP colorTemp *************************\n");

    /*first get all-On/Off status*/
    ret = AW_MPI_ISP_GetModuleOnOff(g_isp_dev, &ModuleOnOff);
    if (0 == ret) {
        DB_PRT("Do AW_MPI_ISP_GetColorTemp succeed isp_dev:%d  (value:%d)  ret:%d \n",
               g_isp_dev, val, ret);

        if(1 == ModuleOnOff.wb) {
            /*then,get whilte balance model,when enabled whilte balance.*/
            ret = AW_MPI_ISP_AWB_GetColorTemp(g_isp_dev, &val);
            if(0 == ret) {
                printf("colorTemp on/off status : ON \n");
                printf("colorTemp model : %s\n",module[val]);
                return 0;
            } else {
                ERR_PRT("Do AW_MPI_ISP_GetColorTemp module on/off fail isp_dev:%d  (value:%d)  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }

        } else {
            printf("colorTemp on/off status : OFF\n");
        }

    } else {
        ERR_PRT("Do AW_MPI_ISP_GetColorTemp fail isp_dev:%d  (value:%d)  ret:%d \n",
                g_isp_dev, val, ret);
        return -1;
    }

    return 0;
}

static int mpp_menu_isp_exposure_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    int  curval    = 0;
    char str[256]  = {0};
    ISP_MODULE_ONOFF ModuleOnOff;

    while(1) {
        printf("\n**************** Setting ISP exposure on/off *************************\n");
        printf("[0]-OFF\n");
        printf("[1]-ON\n");
        printf(" Please Input exposure on/off val [0]~[1] or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 1) {
            /*first get all-On/Off status*/
            ret = AW_MPI_ISP_GetModuleOnOff(g_isp_dev, &ModuleOnOff);
            if (0 != ret) {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetExposure on/off fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }

            /*judge input value is deiffent from current value.if same,pass setting*/
            if(ModuleOnOff.ae != val) {
                ModuleOnOff.manual = 1;
                ModuleOnOff.ae = val;
                ret = AW_MPI_ISP_SetModuleOnOff(g_isp_dev, &ModuleOnOff);
                if (0 == ret) {
                    DB_PRT("Do AW_MPI_ISP_AW_MPI_SetExposure on/off succeed isp_dev:%d  value:%d  ret:%d \n",
                           g_isp_dev, val, ret);
                } else {
                    ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetExposure on/off fail  isp_dev:%d  value:%d  ret:%d \n",
                            g_isp_dev, val, ret);
                    return -1;
                }
            }

            if (1 == val) { // if Module is on
                /*Into model of setting, when enabled Exposure.*/
                goto set_exposure_model;
            } else {
                return 0;
            }

        } else {
            printf(" Input exposure value:%d error!\n", val);
            continue;
        }
    }

set_exposure_model:
    while(1) {
        printf("\n**************** Setting ISP exposure model *************************\n");
        printf("[0]Auto exposure \n");
        printf("[1]Manual exposure \n");
        printf(" Please Input exposure model val [0]~[1] or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 1) {
            ret = AW_MPI_ISP_AE_GetMode(g_isp_dev, &curval);
            if(0 != ret) {
                ERR_PRT("Do AW_MPI_ISP_AE_GetMode fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
            /*judge input value is deiffent from current value.if same,pass setting*/
            if(curval != val) {
                ret = AW_MPI_ISP_AE_SetMode(g_isp_dev, val);
                if (0 == ret) {
                    DB_PRT("Do AW_MPI_ISP_SetMode succeed isp_dev:%d  value:%d  ret:%d \n",
                           g_isp_dev, val, ret);
                } else {
                    ERR_PRT("Do AW_MPI_ISP_SetMode fail isp_dev:%d  value:%d  ret:%d \n",
                            g_isp_dev, val, ret);
                    return -1;
                }
            }

            if (1 == val) { //if mode is menual
                /*Into option of setting, when set Manual exposure.*/
                goto set_exposure_option;
            } else {
                return 0;
            }
        } else {
            printf(" Input exposure model value:%d error!\n", val);
            continue;
        }
    }

set_exposure_option:
    while(1) {
        printf("\n**************** Setting ISP exposure exposure option *************************\n");
        printf("[0]set exposure time \n");
        printf("[1]set exposure gain \n");
        printf(" Please Input exposure option val [0]~[1] or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 == val) {
            ret = mpp_menu_isp_exposure_time_set(NULL,NULL);
            return ret;
        } else if (1 == val) {
            ret = mpp_menu_isp_exposure_gain_set(NULL,NULL);
            return ret;
        } else {
            printf(" Input exposure value:%d error!\n", val);
            continue;
        }
    }
}

static int mpp_menu_isp_exposure_time_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};
    while (1) {
        printf("\n**************** Setting ISP exposure time *************************\n");
        printf(" Please Input exposure time val  or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if  (0 <= val ) {
            ret = AW_MPI_ISP_AE_SetExposure(g_isp_dev, val);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_SetExposure succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, val, ret);
                return 0;
            } else {
                ERR_PRT("Do AW_MPI_ISP_SetExposure fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
        } else {
            printf(" Input exposure time value:%d error!\n", val);
            continue;
        }
    }
}

static int mpp_menu_isp_exposure_gain_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};
    while (1) {
        printf("\n**************** Setting ISP exposure gain *************************\n");
        printf(" Please Input exposure time val 1600~36000 or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (1600 <= val && val <= 36000) {
            ret = AW_MPI_ISP_AE_SetGain(g_isp_dev, val);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_SetGain succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, val, ret);
                return 0;
            } else {
                ERR_PRT("Do AW_MPI_ISP_SetGain fail isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
        } else {
            printf(" Input exposure gain value:%d error!\n", val);
            continue;
        }
    }

}


static int mpp_menu_isp_exposure_get(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    ISP_MODULE_ONOFF ModuleOnOff;

    printf("\n**************** Get ISP exposure *************************\n");

    /*first get all-On/Off status*/
    ret = AW_MPI_ISP_GetModuleOnOff(g_isp_dev, &ModuleOnOff);
    if (0 == ret) {
        DB_PRT("Do AW_MPI_ISP_GetModuleOnOff succeed isp_dev:%d  (value:%d)  ret:%d \n",
               g_isp_dev, val, ret);

        if (1 == ModuleOnOff.ae) {
            printf("exposure on/off status : ON \n");

            /*second,get exposure model,when enabled exposure.*/
            ret = AW_MPI_ISP_AE_GetMode(g_isp_dev, &val);
            if (0 == ret) {
                /*third,get exposure time/gain value,when model was manual*/
                if (1 == val) {
                    printf("exposure model : Manual \n");
                    ret = AW_MPI_ISP_AE_GetExposure(g_isp_dev, &val);
                    if (0 == ret) {
                        DB_PRT("Do AW_MPI_ISP_GetExposure succeed isp_dev:%d  (value:%d)  ret:%d \n",
                               g_isp_dev, val, ret);
                        printf("exposure time : %d \n", val);
                    } else {
                        ERR_PRT("Do AW_MPI_ISP_GetExposure fail isp_dev:%d  (value:%d)  ret:%d \n",
                                g_isp_dev, val, ret);
                        return -1;
                    }

                    ret = AW_MPI_ISP_AE_GetGain(g_isp_dev, &val);
                    if (0 == ret) {
                        DB_PRT("Do AW_MPI_ISP_GetExposure succeed isp_dev:%d  (value:%d)  ret:%d \n",
                               g_isp_dev, val, ret);
                        printf("exposure gain : %d \n", val);
                    } else {
                        ERR_PRT("Do AW_MPI_ISP_GetExposure fail isp_dev:%d  (value:%d)  ret:%d \n",
                                g_isp_dev, val, ret);
                        return -1;
                    }

                    return 0;
                } else {
                    printf("exposure model : Auto \n");
                    return 0;
                }

            } else {
                ERR_PRT("Do AW_MPI_ISP_GetColorTemp module on/off fail isp_dev:%d  (value:%d)  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
        } else {
            printf("exposure on/off status : OFF\n");
        }
    } else {
        ERR_PRT("Do AW_MPI_ISP_GetColorTemp fail isp_dev:%d  (value:%d)  ret:%d \n",
                g_isp_dev, val, ret);
        return -1;
    }

    return 0;
}

static int mpp_menu_isp_PltmWDR_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};
    ISP_MODULE_ONOFF ModuleOnOff;

    while(1) {
        printf("\n**************** Setting ISP PltmWDR *************************\n");
        printf(" Please Input PltmWDR val[0~255] or (q-Quit): ");

        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 255) {
            /*first,get ModuleOnOff status*/
            ret = AW_MPI_ISP_GetModuleOnOff(g_isp_dev, &ModuleOnOff);
            if (0 != ret) {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetModuleOnOff on/off fail  isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
            /*second,set paramters depend your inpunt value*/
            /*set PltmWDR on/off */
            ret = AW_MPI_ISP_SetModuleOnOff(g_isp_dev, &ModuleOnOff);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_AW_MPI_SetModuleOnOff on/off succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, val, ret);
                /*set PltmWDR level */
                ret = AW_MPI_ISP_SetPltmWDR(g_isp_dev, val);
                if (0 == ret) {
                    DB_PRT("Do AW_MPI_ISP_AW_MPI_SetPltmWDR succeed isp_dev:%d  value:%d  ret:%d \n",
                           g_isp_dev, val, ret);
                    return 0;

                } else {
                    ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetPltmWDR fail  isp_dev:%d  value:%d  ret:%d \n",
                            g_isp_dev, val, ret);
                    return -1;
                }
            } else {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetModuleOnOff on/off fail  isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }

        } else {
            printf(" Input PltmWDR value:%d error!\n", val);
            continue;
        }
    }
}

static int mpp_menu_isp_PltmWDR_get(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    int  WDR_level = 0;
    ISP_MODULE_ONOFF   ModuleOnOff;

    printf("\n**************** Get ISP PltmWDR *************************\n");
    ret = AW_MPI_ISP_GetModuleOnOff(g_isp_dev, &ModuleOnOff);
    if (0 == ret) {
        DB_PRT("Do AW_MPI_ISP_GetModuleOnOff succeed isp_dev:%d  (value:%d)  ret:%d \n",
               g_isp_dev, val, ret);

        ret = AW_MPI_ISP_GetPltmWDR(g_isp_dev, &WDR_level);
        if (0 == ret) {
            DB_PRT("Do AW_MPI_ISP_SetPltmWDR succeed isp_dev:%d  (value:%d)  ret:%d \n",
                   g_isp_dev, val, ret);
            if (1 == ModuleOnOff.pltm && 1 == ModuleOnOff.manual ) {
                printf("PltmWDR On/Off status : ON \n");
                printf("PltmWDR level : %d \n", WDR_level);
            } else {
                printf("PltmWDR On/Off status : OFF \n");
            }
            return 0;
        } else {
            ERR_PRT("Do AW_MPI_ISP_SetPltmWDR fail isp_dev:%d  (value:%d)  ret:%d \n",
                    g_isp_dev, val, ret);
            return -1;
        }
    } else {
        ERR_PRT("Do AW_MPI_ISP_GetModuleOnOff fail isp_dev:%d  (value:%d)  ret:%d \n",
                g_isp_dev, val, ret);
        return -1;
    }

}

static int mpp_menu_isp_3NRAttr_set(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    char str[256]  = {0};
    ISP_MODULE_ONOFF ModuleOnOff;

    while(1) {
        printf("\n**************** Setting ISP 3NRAttr *************************\n");
        printf(" Please Input saturation val[0~100]or (q-Quit): ");


        memset(str, 0, sizeof(str));
        gets(str);
        printf("\n");
        if (0 == str[0]) {
            continue;
        }
        if ('q' == str[0]) {
            return 0;
        }

        ret = is_digit_str(str);
        if (ret) {
            printf(" Input %s error.\n\n", str);
            continue;
        }

        val = atoi(str);
        if (0 <= val && val <= 100) {
            /*first ,get ModuleOnOff status*/
            ret = AW_MPI_ISP_GetModuleOnOff(g_isp_dev, &ModuleOnOff);
            if (0 != ret) {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetModuleOnOff on/off fail  isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }
            /*second, set 3NR on/off paramters depend your input value*/
            /*set 3NRAttr on/off */
            ret = AW_MPI_ISP_SetModuleOnOff(g_isp_dev, &ModuleOnOff);
            if (0 == ret) {
                DB_PRT("Do AW_MPI_ISP_AW_MPI_SetModuleOnOff on/off succeed isp_dev:%d  value:%d  ret:%d \n",
                       g_isp_dev, val, ret);
                /*set PltmWDR level depend your input value*/
                ret = AW_MPI_ISP_Set3NRAttr(g_isp_dev, val);
                if (0 == ret) {
                    DB_PRT("Do AW_MPI_ISP_AW_MPI_Set3NRAttr succeed isp_dev:%d  value:%d  ret:%d \n",
                           g_isp_dev, val, ret);
                    return 0;

                } else {
                    ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_Set3NRAttr fail  isp_dev:%d  value:%d  ret:%d \n",
                            g_isp_dev, val, ret);
                    return -1;
                }
            } else {
                ERR_PRT("Do AW_MPI_ISP_AW_MPI_ISP_SetModuleOnOff on/off fail  isp_dev:%d  value:%d  ret:%d \n",
                        g_isp_dev, val, ret);
                return -1;
            }

        } else {
            printf(" Input 3NRAttr value:%d error!\n", val);
            continue;
        }
    }

}

static int mpp_menu_isp_3NRAttr_get(void *pData, char *pTitle)
{
    int  ret       = 0;
    int  val       = 0;
    ISP_MODULE_ONOFF   ModuleOnOff;

    printf("\n**************** Get ISP PltmWDR *************************\n");
    ret = AW_MPI_ISP_GetModuleOnOff(g_isp_dev, &ModuleOnOff);
    if (0 == ret) {
        DB_PRT("Do AW_MPI_ISP_GetModuleOnOff succeed isp_dev:%d  (value:%d)  ret:%d \n",
               g_isp_dev, val, ret);
        ret = AW_MPI_ISP_Get3NRAttr(g_isp_dev, &val);
        if (0 == ret) {
            DB_PRT("Do AW_MPI_ISP_Get3NRAttr succeed isp_dev:%d  (value:%d)  ret:%d \n",
                   g_isp_dev, val, ret);
            return 0;
        } else {
            ERR_PRT("Do AW_MPI_ISP_Get3NRAttr fail isp_dev:%d  (value:%d)  ret:%d \n",
                    g_isp_dev, val, ret);
            return -1;
        }
    } else {
        ERR_PRT("Do AW_MPI_ISP_GetModuleOnOff fail isp_dev:%d  (value:%d)  ret:%d \n",
                g_isp_dev, val, ret);
        return -1;
    }

}


#define AW_BC_IR_IO_1_NAME "231"
#define AW_BC_IR_IO_2_NAME "232"

#define AW_BC_IR_IO_PATH_1 "/sys/class/gpio/gpio"AW_BC_IR_IO_1_NAME"/value"
#define AW_BC_IR_IO_PATH_2 "/sys/class/gpio/gpio"AW_BC_IR_IO_2_NAME"/value"

#define AW_BC_IR_IO_EXPORT_PATH "/sys/class/gpio/export"

#define AW_BC_IR_IO_DIRECTION_PATH_1 "/sys/class/gpio/gpio"AW_BC_IR_IO_1_NAME"/direction"
#define AW_BC_IR_IO_DIRECTION_PATH_2 "/sys/class/gpio/gpio"AW_BC_IR_IO_2_NAME"/direction"

#define AW_BC_IO_PULL_STR "1"
#define AW_BC_IO_DOWN_STR "0"

#define AW_BC_IR_IO_DIFF_TIME  120 * 1000

static int g_ircut_init_flag = 0;

int write_str_to_path(const char* path, const char* str)
{
    FILE *fd = fopen(path, "w");
    if (fd == NULL) {
        printf("%s open failed!\n", path);
        return -1;
    }

    int ret = fwrite(str, strlen(str), 1, fd);
    if (ret == -1) {
        printf("write to path %s, value %s failed!\n", path, str);
    }

    fclose(fd);
    return 0;
}

int aw_bc_ir_init()
{
    int ret = write_str_to_path(AW_BC_IR_IO_EXPORT_PATH, AW_BC_IR_IO_1_NAME);
    if (ret == -1) {
        printf("export IO[%s] failed!", AW_BC_IR_IO_1_NAME);
        return -1;
    }

    ret = write_str_to_path(AW_BC_IR_IO_EXPORT_PATH, AW_BC_IR_IO_2_NAME);
    if (ret == -1) {
        printf("export IO[%s] failed!", AW_BC_IR_IO_2_NAME);
        return -1;
    }

    ret = write_str_to_path(AW_BC_IR_IO_DIRECTION_PATH_1, "out");
    if (ret == -1) {
        return -1;
    }

    return write_str_to_path(AW_BC_IR_IO_DIRECTION_PATH_2, "out");
}

int aw_bc_ir_on_off()
{
    static char last_value;

    const char *first  = last_value ? AW_BC_IO_PULL_STR : AW_BC_IO_DOWN_STR;
    const char *second = last_value ? AW_BC_IO_DOWN_STR : AW_BC_IO_PULL_STR;

    printf("start make ir gpio different\n");
    int ret = write_str_to_path(AW_BC_IR_IO_PATH_1, first);
    if (ret == -1) {
        return -1;
    }

    ret = write_str_to_path(AW_BC_IR_IO_PATH_2, second);
    if (ret == -1) {
        return -1;
    }

    usleep(AW_BC_IR_IO_DIFF_TIME);

    printf("stop gpio different\n");
    ret = write_str_to_path(AW_BC_IR_IO_PATH_1, AW_BC_IO_DOWN_STR);
    if (ret == -1) {
        return -1;
    }

    ret = write_str_to_path(AW_BC_IR_IO_PATH_2, AW_BC_IO_DOWN_STR);
    if (ret == -1) {
        return -1;
    }

    last_value = last_value ? 0 : 1;
    printf("last_value %d\n", last_value);

    return 0;
}

static int ircut_control(void *pData, char *pTitle)
{
    if (0 == g_ircut_init_flag) {
        aw_bc_ir_init();
        g_ircut_init_flag = 1;
    }

    aw_bc_ir_on_off();

    return 0;
}


