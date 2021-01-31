FRESULT fe_load(const TCHAR *path, UINT max_length, transfer_function_t transfer_function) {
    FRESULT fresult;
    FATFS *fs;
    FIL fil;
    UINT bytes_to_read;

    FIL *fp = &fil;

    UINT cluster_sector;
    DWORD cluster;
    LBA_t starting_sector;
    UINT sector_count;
    UINT sector_remainder;
    UINT bytes_processed;


    if ((fresult = f_open(fp, path, FA_READ)) != FR_OK) {
        return fresult;
    }

    fresult = validate(&fp->obj, &fs);
    if (fresult != FR_OK || (fresult = (FRESULT) fp->err) != FR_OK) {
        LEAVE_FF(fs, fresult);
    }

    bytes_to_read = (fp->obj.objsize > max_length) ? max_length : fp->obj.objsize;
    bytes_to_read += ((bytes_to_read % SS(fs)) != 0) ? (SS(fs) - (bytes_to_read % SS(fs))) : 0;

    while (bytes_to_read) {
        cluster_sector = (UINT) (fp->fptr / SS(fs) & (fs->csize - 1));

        if (cluster_sector == 0) {
            if (fp->fptr == 0) {
                cluster = fp->obj.sclust;
            } else {
#if FF_USE_FASTSEEK
                if (fp->cltbl) {
                    cluster = clmt_clust(fp, fp->fptr);
                } else
#endif
                {
                    cluster = get_fat(&fp->obj, fp->clust);
                }
            }

            if (cluster < 2) {
                LEAVE_FF(fs, FR_INT_ERR);
            }

            if (cluster == 0xFFFFFFFF) {
                LEAVE_FF(fs, FR_DISK_ERR);
            }

            fp->clust = cluster;
        }

        starting_sector = clst2sect(fs, fp->clust);

        if (starting_sector == 0) {
            LEAVE_FF(fs, FR_INT_ERR);
        }

        starting_sector += cluster_sector;

        sector_count = bytes_to_read / SS(fs);

        if (cluster_sector + sector_count > fs->csize) {
            sector_count = fs->csize - cluster_sector;
        }

        if (transfer_function(fs->pdrv, fp->fptr, starting_sector, sector_count) != RES_OK) {
            LEAVE_FF(fs, FR_DISK_ERR);
        }

        fp->sect = starting_sector;

        bytes_processed = sector_count * SS(fs);
        fp->fptr += bytes_processed;
        bytes_to_read -= bytes_processed;
    }

    LEAVE_FF(fs, FR_OK);
}
