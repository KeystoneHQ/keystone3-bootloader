#include "define.h"
#include "user_fatfs.h"
#include "drv_sdcard.h"
#include "drv_sys.h"
#include "log_print.h"
#include "user_memory.h"
#include "diskio.h"
#include "drv_gd25qxx.h"
#include "drv_sdcard.h"
#include "err_code.h"
#include "draw_on_lcd.h"

LV_FONT_DECLARE(openSans_24);

typedef struct FatfsMountParam {
    char *name;
    FATFS *fs;
    char *volume;
    BYTE opt;
} FatfsMountParam_t;

static FATFS g_sdFs;
static FATFS g_flashUsbFs;

FatfsMountParam_t g_fsMountParamArray[DEV_BUTT] = {
    {"SD", &g_sdFs, "0:", 1},
    {"USB", &g_flashUsbFs, "1:", 1},
};

int FatfsTouchFile(const TCHAR* path)
{
    FIL fp;
    FRESULT res = f_open(&fp, path, FA_CREATE_ALWAYS);
    if (res) {
        FatfsError(res);
        return RES_ERROR;
    }
    f_close(&fp);
    return RES_OK;
}


int FatfsFileWrite(const TCHAR* path, const uint8_t *data, uint32_t len)
{
    FIL fp;
    FRESULT res = f_open(&fp, path, FA_OPEN_ALWAYS | FA_WRITE);
    if (res) {
        FatfsError(res);
        return RES_ERROR;
    }

    UINT writeBytes;
    res = f_write(&fp, (void*)data, len, &writeBytes);
    if (res) {
        FatfsError(res);
        return RES_ERROR;
    }
    f_close(&fp);
    printf("write %s %d btyes success\r\n", path, writeBytes);

    return RES_OK;
}

int FatfsFileCreate(const TCHAR* path)
{
    FIL fp;
    FRESULT res = f_open(&fp, path, FA_CREATE_ALWAYS);
    if (res) {
        FatfsError(res);
        return RES_ERROR;
    }
    f_close(&fp);
    return RES_OK;
}

int FatfsFileAppend(const TCHAR* path, const uint8_t *data, uint32_t len)
{
    FIL fp;
    FRESULT res = f_open(&fp, path, FA_OPEN_APPEND | FA_WRITE);
    if (res) {
        printf("open err=%d\n", res);
        FatfsError(res);
        return RES_ERROR;
    }

    UINT writeBytes;
    res = f_write(&fp, (void*)data, len, &writeBytes);
    if (res) {
        printf("write err=%d\n", res);
        FatfsError(res);
        return RES_ERROR;
    }
    f_close(&fp);
    printf("write %s %d btyes success\r\n", path, writeBytes);

    return RES_OK;
}

int FatfsFileDelete(const TCHAR* path)
{
    FRESULT res = f_unlink(path);
    if (res) {
        FatfsError(res);
        return RES_ERROR;
    }

    printf("delete %s success\r\n", path);
    return RES_OK;
}

#define FATFS_COPY_BUFFER_SIZE              4096
int FatfsFileCopy(const TCHAR* source, const TCHAR* dest)
{
    FRESULT res = FR_INT_ERR;
    FIL fpSource = {0}, fpDest = {0};
    uint8_t sourceOpened = 0, destOpened = 0;
    uint32_t fileSize, actualSize, copyOffset, totalSize, requestSize;
    uint8_t *data = NULL;
    uint8_t oldPercent = 0;
    char percentStr[16];

    printf("file copy from %s to %s\n", source, dest);
    do {
        res = f_open(&fpSource, source, FA_OPEN_EXISTING | FA_READ);
        if (res) {
            FatfsError(res);
            break;
        }
        sourceOpened = 1;
        res = f_open(&fpDest, dest, FA_CREATE_ALWAYS | FA_WRITE);
        if (res) {
            FatfsError(res);
            break;
        }
        destOpened = 1;
        fileSize = f_size(&fpSource);
        if (fileSize == 0) {
            res = FR_OK;
            break;
        }
        data = pvPortMalloc(FATFS_COPY_BUFFER_SIZE);
        if (data == NULL) {
            res = FR_NOT_ENOUGH_CORE;
            break;
        }
        totalSize = 0;
        for (copyOffset = 0; copyOffset < fileSize; copyOffset += FATFS_COPY_BUFFER_SIZE) {
            if (!SdCardInsert()) {
                res = ERR_GENERAL_FAIL;
                break;
            }

            uint8_t percent = totalSize * 100 / fileSize;
            if (oldPercent != percent) {
                printf("==========copy %d%%==========\n", percent);
                oldPercent = percent;
                sprintf(percentStr, "%d%%", percent);
                DrawStringOnLcd(215, 620, percentStr, 0xFFFF, &openSans_24);
                DrawProgressBarOnLcd(80, 594, 320, 9, percent, 0x21F4);
            }

            requestSize = (fileSize - copyOffset > FATFS_COPY_BUFFER_SIZE) ? FATFS_COPY_BUFFER_SIZE : (fileSize - copyOffset);
            res = f_read(&fpSource, data, requestSize, &actualSize);
            if (res) {
                FatfsError(res);
                break;
            }
            if (actualSize == 0) {
                res = FR_DISK_ERR;
                break;
            }
            totalSize += actualSize;
            // printf("offset=%d,actualRead=%d,totalSize=%d\n", copyOffset, actualSize, totalSize);
            res = f_write(&fpDest, data, actualSize, &actualSize);
            if (res) {
                FatfsError(res);
                break;
            }
        }

    } while (0);
    if (data != NULL) {
        vPortFree(data);
    }
    if (sourceOpened) {
        f_close(&fpSource);
    }
    if (destOpened) {
        f_close(&fpDest);
    }
    printf("copy done\n");
    return res;
}

bool FatfsFileExist(const TCHAR* path)
{
    FILINFO fno;
    FRESULT res = f_stat(path, &fno);
    if (res == FR_OK) {
        return true;
    }

    return false;
}

void FatfsShowVolumeStatus(char *ptr)
{
    FRESULT res;
    long p1;
    FATFS *fs;
    static const char *ft[] = {"", "FAT12", "FAT16", "FAT32", "exFAT"};

    if (ptr == NULL) {
        return;
    }

    res = f_getfree(ptr, (DWORD*)&p1, &fs);
    if (res) {
        FatfsError(res);
        return;
    }
    printf("FAT type = %s\r\n", ft[fs->fs_type]);
    printf("Bytes/Cluster = %lu\r\n", (DWORD)fs->csize * 512);
    printf("Number of FATs = %u\r\n", fs->n_fats);
    if (fs->fs_type < FS_FAT32) printf("Root DIR entries = %u\r\n", fs->n_rootdir);
    printf("Sectors/FAT = %lu\r\n", fs->fsize);
    printf("Number of clusters = %lu\r\n", (DWORD)fs->n_fatent - 2);
    printf("Volume start (lba) = %lu\r\n", fs->volbase);
    printf("FAT start (lba) = %lu\r\n", fs->fatbase);
    printf("DIR start (lba,clustor) = %lu\r\n", fs->dirbase);
    printf("Data start (lba) = %lu\r\n\r\n", fs->database);

}

void FatfsDirectoryListing(char *ptr)
{
    DIR Dir;
    FILINFO Finfo;
    FRESULT res;
    UINT acc_files, acc_dirs;
    QWORD acc_size;

    if (ptr == NULL) {
        return;
    }

    while (*ptr == ' ') ptr++;
    res = f_opendir(&Dir, ptr);
    if (res) {
        FatfsError(res);
        return;
    }
    acc_size = acc_dirs = acc_files = 0;
    for (;;) {
        res = f_readdir(&Dir, &Finfo);
        if ((res != FR_OK) || !Finfo.fname[0]) break;
        if (Finfo.fattrib & AM_DIR) {
            acc_dirs++;
        } else {
            acc_files++;
            acc_size += Finfo.fsize;
        }

        printf("%s:\n", Finfo.fname);
        printf("%c%c%c%c%c   %u/%02u/%02u   %02u:%02u %9lu\r\n",
               (Finfo.fattrib & AM_DIR) ? 'd' : '-',
               (Finfo.fattrib & AM_RDO) ? 'r' : '-',
               (Finfo.fattrib & AM_HID) ? 'h' : '-',
               (Finfo.fattrib & AM_SYS) ? 's' : '-',
               (Finfo.fattrib & AM_ARC) ? 'a' : '-',
               (Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
               (Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63,
               Finfo.fsize);
    }
#if 0
    printf("%4u File(s),%10llu bytes total\r\n%4u Dir(s)", acc_files, acc_size, acc_dirs);
    res = f_getfree(ptr, &dw, &fs);
    if (res == FR_OK) {
        printf(", %10llu bytes free\r\n", (QWORD)dw * fs->csize * 512);
    } else {
        put_rc(res);
    }
#endif
}

int MMC_disk_initialize(void)
{
    if (SD_OK != SD_Init()) {
        return RES_NOTRDY;
    }
    return RES_OK;
}

int MMC_disk_read(
    BYTE *buff,     /* Data buffer to store read data */
    LBA_t sector,   /* Start sector in LBA */
    UINT count      /* Number of sectors to read */
)
{
    uint32_t i;
    DRESULT status = RES_PARERR;
    SD_Error SD_state = SD_OK;

    if ((DWORD)buff & 3) {
        DWORD scratch[BLOCK_SIZE / 4];

        i = 0;
        while (i < count) {
            SD_state = SD_ReadBlock((void *)scratch, (sector + i) * BLOCK_SIZE, BLOCK_SIZE);
            if (SD_state != SD_OK)
                return RES_PARERR;

            SD_state = SD_WaitReadOperation();
            //while(SD_GetStatus() != SD_TRANSFER_OK);

            if (SD_state != SD_OK)
                status = RES_PARERR;
            else
                status = RES_OK;
            memcpy(buff, scratch, BLOCK_SIZE);
            buff += BLOCK_SIZE;
            i++;
        }
    } else {
        for (i = 0; i < count; i++) {
            SD_state = SD_ReadBlock(buff + i * BLOCK_SIZE,
                                    (sector + i) * BLOCK_SIZE, BLOCK_SIZE);
            if (SD_state != SD_OK)
                return RES_PARERR;

            // printf("disk readblock SDCARD state %d\r\n", SD_state);
            SD_state = SD_WaitReadOperation();
            //while(SD_GetStatus() != SD_TRANSFER_OK);

            if (SD_state != SD_OK)
                status = RES_PARERR;
            else
                status = RES_OK;
        }
    }

    // printf("disk readblock state %d\r\n", SD_state);
    return status;
}

int MMC_disk_write(
    const BYTE *buff, /* Data to be written */
    LBA_t sector,       /* Start sector in LBA */
    UINT count          /* Number of sectors to write */
)
{
    uint32_t i;
    DRESULT status = RES_PARERR;
    SD_Error SD_state = SD_OK;

    if ((DWORD)buff & 3) {
        DWORD scratch[BLOCK_SIZE / 4];

        i = 0;
        while (i < count) {
            memcpy(scratch, buff, BLOCK_SIZE);
            SD_state = SD_WriteBlock((void *)scratch, (sector + i) * BLOCK_SIZE, BLOCK_SIZE);
            if (SD_state != SD_OK)
                return RES_PARERR;

            SD_state = SD_WaitWriteOperation();
            //while(SD_GetStatus() != SD_TRANSFER_OK);

            if (SD_state != SD_OK)
                status = RES_PARERR;
            else
                status = RES_OK;

            buff += BLOCK_SIZE;
            i++;
        }
    } else {
        for (i = 0; i < count; i++) {
            SD_state = SD_WriteBlock((void *)(buff + i * BLOCK_SIZE),
                                     (sector + i) * BLOCK_SIZE, BLOCK_SIZE);
            if (SD_state != SD_OK)
                return RES_PARERR;

            printf("disk writeblock SDCARD state %d\r\n", SD_state);
            SD_state = SD_WaitWriteOperation();
            //while(SD_GetStatus() != SD_TRANSFER_OK);

            if (SD_state != SD_OK)
                status = RES_PARERR;
            else
                status = RES_OK;
        }
    }
    return status;
}


int USB_disk_initialize(void)
{
    return RES_OK;
}

int USB_disk_read(
    BYTE *buff,     /* Data buffer to store read data */
    LBA_t sector,   /* Start sector in LBA */
    UINT count      /* Number of sectors to read */
)
{
    for (uint16_t i = 0; i < count; i++) {
        Gd25FlashReadBuffer((sector + i) * GD25QXX_SECTOR_SIZE, buff + i * GD25QXX_SECTOR_SIZE, GD25QXX_SECTOR_SIZE);
    }

    return RES_OK;
}

int USB_disk_write(
    const BYTE *buff, /* Data to be written */
    LBA_t sector,       /* Start sector in LBA */
    UINT count          /* Number of sectors to write */
)
{
    for (uint16_t i = 0; i < count; i++) {
        Gd25FlashSectorErase((sector + i) * GD25QXX_SECTOR_SIZE);
        Gd25FlashWriteBuffer((sector + i) * GD25QXX_SECTOR_SIZE, buff + i * GD25QXX_SECTOR_SIZE, GD25QXX_SECTOR_SIZE);
    }

    return RES_OK;
}

int FatfsMount(void)
{
    FRESULT res;
    FatfsMountParam_t *fs;
    for (uint8_t i = 0; i < NUMBER_OF_ARRAYS(g_fsMountParamArray); i++) {
        fs = &g_fsMountParamArray[i];
        res = f_mount(fs->fs, fs->volume, fs->opt);
        if (res == FR_NO_FILESYSTEM) {
            BYTE work[FF_MAX_SS];
            f_mkfs(fs->volume, 0, work, sizeof work);
            f_mount(NULL, fs->volume, fs->opt);
            res = f_mount(fs->fs, fs->volume, fs->opt);
            printf("%s:", fs->name);
            FatfsError(res);
        }
        printf("%s:", fs->name);
        FatfsError(res);
    }
    return res;
}


int MountUsbFatfs(void)
{
    FRESULT res;
    FatfsMountParam_t *fs = &g_fsMountParamArray[DEV_USB];
    res = f_mount(fs->fs, fs->volume, fs->opt);
    if (res == FR_NO_FILESYSTEM) {
        BYTE work[FF_MAX_SS];
        f_mkfs(fs->volume, 0, work, sizeof work);
        f_mount(NULL, fs->volume, fs->opt);
        res = f_mount(fs->fs, fs->volume, fs->opt);
        printf("%s:", fs->name);
        FatfsError(res);
    }
    printf("%s:", fs->name);
    FatfsError(res);
    return res;
}


int MountSdFatfs(void)
{
    FRESULT res;
    FatfsMountParam_t *fs = &g_fsMountParamArray[DEV_MMC];
    res = f_mount(fs->fs, fs->volume, fs->opt);
    if (res == FR_NO_FILESYSTEM) {
        BYTE work[FF_MAX_SS];
        f_mkfs(fs->volume, 0, work, sizeof work);
        f_mount(NULL, fs->volume, fs->opt);
        res = f_mount(fs->fs, fs->volume, fs->opt);
        printf("%s:", fs->name);
        FatfsError(res);
    }
    printf("%s:", fs->name);
    FatfsError(res);
    return res;
}


void FatfsError(FRESULT errNum)
{
    const char *str =
        "OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
        "INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
        "INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
        "LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0" "INVALID_PARAMETER\0";
    FRESULT i;

    for (i = FR_OK; i != errNum && *str; i++) {
        while (*str++) ;
    }
    printf("errNum = %u FR_%s\n", (UINT)errNum, str);
}

