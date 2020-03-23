/*
 * libfds, used to store data in the on chip flash of a MCU. It shall NOT be a 
 * full blown file system but more than just a simple EEPROM emulation.
 *
 * Copyright (C) 2020 Julian Friedrich
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>. 
 *
 * You can file issues at https://github.com/fjulian79/libfds
 */

#include "fds/fds.hpp"
#include "generic/generic.h"
#include "generic/crc8.hpp"
#include "logging/logging.h"

#include <string.h>
#include <stdio.h>

/**
 * @brief The module name used for logging
 */
#define MODULENAME                      "libfds"

/**
 * @brief Defines the page number of the furst flash page used by libfds
 */
#define FDS_FIRSTFLASHPAGE              (BSP_FLASH_NUMPAGES - FDS_NUM_PAGES)

/**
 * @brief Defines the magic used in page header.
 */
#define FDS_PAGEMAGIC                   (0xAA)

/**
 * @brief Defines the magic used in the header for data records.
 */
#define FDS_DATAMAGIC                   (0x55)

/**
 * @brief Defines the magic used in the header for removing data.
 */
#define FDS_DELMAGIC                    (0x7E)

Fds* Fds::pInstance = 0;

Fds::Fds():
     InitDone(false),
     pWrite(0)
{

}

Fds::~Fds()
{

}

Fds* Fds::getInstance(void)
{
    if(pInstance == 0)
    {
        pInstance = new Fds();
    }

    return pInstance;
}

fdsStatus_t Fds::init(bool doReset)
{
    fdsStatus_t retval = FDS_OK;
    uint16_t pageId = 0;
    uint16_t delta = 0;
    bool updateWritePtr = true;

    if (InitDone == false)
    {
        memset(&pRecords, 0, sizeof(pRecords));
        pWrite = 0;

        for (uint16_t page = 0; page < FDS_NUM_PAGES; page++)
        {
            pageId = getPageid(page);
            delta = getPageid(wrapInc(page, 1, FDS_NUM_PAGES)) - pageId;

            if (pageId == 0xFFFF)
            {
                continue;
            }
            else if (delta > 0)
            {
                retval = readPage(page, updateWritePtr);
                updateWritePtr = delta > 2 ? false : true;
            }
            else
            {
                /* delta == 0 --> this is forbidden!! */
                retval = FDS_ERR;
            }

            if(retval != FDS_OK)
            {
                logErr("Error %d while reading the Flash\n", retval);
                break;
            }
        }
    }

    if((retval != FDS_OK) || (pWrite == 0))
    {
        if(doReset == true)
        {
            logInfo("Erasing fds flash.\n");
            retval = format();
        }
        else
        {
            logDebug("Erasing fds flash supressed.\n");
        }
    }
    else
    {
        InitDone = true;
    }

    return retval;
}

fdsStatus_t Fds::info(void)
{
    fdsStatus_t retval = FDS_OK;
    uint8_t str[(FDS_NUM_RECORDS * 4)+1];
    uint8_t offs = 0;
    uint8_t cnt = 0;

    if (!InitDone)
    {
        retval = init();
        if(retval != FDS_OK)
        {
            return retval;
        }
    }

    printf("  First page: %u 0x%08lX\n", 
        FDS_FIRSTFLASHPAGE, (uint32_t)BSP_FLASH_PAGETOADDR(FDS_FIRSTFLASHPAGE));
    printf("  Num pages: %u\n", FDS_NUM_PAGES);
    printf("  Num supported id's: %u\n", FDS_NUM_RECORDS);
    printf("  pWrite on page %ld @ 0x%08lX\n", 
        BSP_FLASH_ADDRTOPAGE(pWrite) - FDS_FIRSTFLASHPAGE, (uint32_t)pWrite);
    
    for (uint8_t id = 0; id < FDS_NUM_RECORDS; id++)
    {
        if (pRecords[id] != 0)
        {
            cnt++;
            offs += snprintf((char*)&str[offs], sizeof(str)-offs, "%u ", id);
        }
    }

    printf("  Data available for %u id's", cnt);
    if(cnt != 0)
    {
        offs--;
        str[offs] = 0;
        printf(":\n  [%s]\n", str);
    }
    else
    {
        printf(".\n");
    }

    return retval;
}

fdsStatus_t Fds::write(uint8_t uid, void* pData, size_t numBytes)
{
    fdsStatus_t retval = FDS_OK;
    uint16_t *pStart = pWrite;
    size_t siz = 0;
    fdsDataHdr_t hdr;
    fdsDataFtr_t ftr;
    crc8 crc;

    if ((numBytes == 0) || (numBytes > FDS_MAX_DATABYTES))
    {
        return FDS_ESIZE;
    }

    if (uid >= FDS_NUM_RECORDS)
    {
        return FDS_EEINVAL;
    }

    if (!InitDone)
    {
        retval = init();
        if(retval != FDS_OK)
        {
            return retval;
        }
    }

    /* Prepare the data header. In the header the real number of user data bytes
     * has to be used! The crc clauclation can also start right now as the 
     * header is compleate.
     * */
    hdr.Magic = FDS_DATAMAGIC;
    hdr.Uid = uid;
    hdr.Siz = numBytes;
    crc.calc(&hdr, sizeof(hdr));

    /* There is a spare byte reserved in the footer. It shall be used for user 
     * data if the users data size is uneven. In this case the nuber of user 
     * data bytes has to be reduced by one. 
     * */
    if(numBytes%2 != 0)
    {
        numBytes--;
        ftr.Data = ((uint8_t*)pData)[numBytes];
    }
    else
    {
        ftr.Data = 0;
    }
    
    /* Calculate the size of the record in the flash */
    siz = (sizeof(hdr) + numBytes + sizeof(ftr)) / 2;

    /* If this does not fit in the current page proceed on the next page */
    if (BSP_FLASH_ADDRTOPAGE(pWrite) != BSP_FLASH_ADDRTOPAGE(pWrite + siz))
    {
        retval = switchPage(uid);
        if(retval != FDS_OK)
        {
            logErr("Error %u while switchPage\n", retval);
            return retval;
        }
    }

    logDebug("New data starts @ 0x%08lx\n", (uint32_t)pWrite);

    do
    {
        retval = writeToFlash(&hdr, sizeof(hdr), false);
        breakIfDiverse(retval, FDS_OK);
    
        if(numBytes > 0)
        {
            crc.calc(pData, numBytes);
            retval = writeToFlash(pData, numBytes, false);
            breakIfDiverse(retval, FDS_OK);
        }
        
        /* The footer union takes care of correct byte postions. If a change is 
         * needed read the doc of fdsDataFtr_t first.
         * */
        ftr.Crc = crc.calc(ftr.Data);
        retval = writeToFlash(&ftr, sizeof(ftr), false);
        breakIfDiverse(retval, FDS_OK);
    
    } while (0);

    if (retval != FDS_OK)
    {
        logErr("Error %u while writing to the flash\n", retval);
        return FDS_EFLASH;
    }

    crc = 0;
    if (crc.calc(pStart, siz * 2) != 0)
    {
        return FDS_ECRC;
    }

    pRecords[uid] = pStart;

    return FDS_OK;
}

size_t Fds::read(uint8_t uid, void* pData, size_t siz)
{
    fdsStatus_t retval = FDS_OK;
    fdsDataHdr_t *pHdr = 0;
    uint8_t *pFlash = 0;

    if (!InitDone)
    {
        retval = init();
        if(retval != FDS_OK)
        {
            return 0;
        }
    }

    if((uid >= FDS_NUM_RECORDS) || (pData == 0) || (siz == 0))
    {
        return 0;
    }

    if(pRecords[uid] == 0)
    {
        return 0;
    }

    pHdr = (fdsDataHdr_t*)pRecords[uid];
    pFlash = (uint8_t*)(pRecords[uid]) + sizeof(fdsDataHdr_t);
    siz = min(siz, pHdr->Siz);
    memcpy(pData, pFlash, siz);

    return siz;
}

fdsStatus_t Fds::del(uint8_t uid)
{
    fdsStatus_t retval;
    fdsDataHdr_t hdr;
    fdsDataFtr_t ftr;
    uint16_t *pStart = pWrite;
    crc8 crc;

    if (!InitDone)
    {
        retval = init();
        if(retval != FDS_OK)
        {
            return retval;
        }
    }

    if(uid >= FDS_NUM_RECORDS)
    {
        return FDS_EEINVAL;
    }

    hdr.Magic = FDS_DELMAGIC;
    hdr.Uid = uid;
    hdr.Siz = 0;
    ftr.Data = 0;
    crc.calc(&hdr, sizeof(hdr));
    ftr.Crc = crc.calc(ftr.Data);

    do
    {
        retval = writeToFlash(&hdr, sizeof(hdr), false);
        breakIfDiverse(retval, FDS_OK);

        retval = writeToFlash(&ftr, sizeof(ftr), false);
        breakIfDiverse(retval, FDS_OK);

    } while (0);

    if (retval != FDS_OK)
    {
        logErr("Error %u while writing to the flash\n", retval);
        return retval;
    }

    crc = 0;
    if (crc.calc(pStart, sizeof(hdr) + sizeof(ftr)) != 0)
    {
        return FDS_ECRC;
    }

    pRecords[uid] = 0;

    return FDS_OK;
}

fdsStatus_t Fds::format(void)
{   
    fdsStatus_t retval = FDS_OK;

    InitDone = false;
    
    bspFlashUnlock();

    for (uint16_t page = 0; page < FDS_NUM_PAGES; page++)
    {
        bspFlashErasePage(BSP_FLASH_PAGETOADDR(FDS_FIRSTFLASHPAGE + page));
    }

    bspFlashLock();

    retval = writePageHdr(0, 0);
    if(retval != FDS_OK)
    {
        return retval;
    }

    return init(false);
}

uint16_t Fds::getPageid(uint16_t page)
{
    uint16_t pageId = 0xFFFF;
    fdsPageHdr_t *pHdr;
    crc8 crc;

    page += FDS_FIRSTFLASHPAGE;
    pHdr = (fdsPageHdr_t*)BSP_FLASH_PAGETOADDR(page);

    if(crc.calc(pHdr, sizeof(fdsPageHdr_t)) == 0)
    {
        pageId = pHdr->Id;
    }

    return pageId;
}

fdsStatus_t Fds::writePageHdr(uint16_t page, uint16_t uid)
{
    fdsStatus_t retval = FDS_OK;
    fdsPageHdr_t pageHdr;
    crc8 crc;

    page += FDS_FIRSTFLASHPAGE;
    pWrite = BSP_FLASH_PAGETOADDR(page);
    
    pageHdr.Magic = FDS_PAGEMAGIC;
    pageHdr.Id = uid;
    pageHdr.Crc = crc.calc(&pageHdr, sizeof(pageHdr) - 1);

    retval = writeToFlash(&pageHdr, sizeof(pageHdr));
    if(retval != FDS_OK)
    {
        logErr("Error %u while writing PageHdr %u\n", retval, page);
    }

    return retval;
}

fdsStatus_t Fds::readPage(uint16_t page, bool updateWritePointer)
{
    fdsStatus_t retval = FDS_OK;
    uint8_t *pData = 0;
    fdsDataHdr_t *pHdr = 0;
    uint16_t siz = 0;
    crc8 crc;

    page += FDS_FIRSTFLASHPAGE;
    pData = (uint8_t*)BSP_FLASH_PAGETOADDR(page) + sizeof(fdsPageHdr_t);

    logDebug("Reading page %d\n", page);

    while (BSP_FLASH_ADDRTOPAGE(pData) == page)
    {
        pHdr = (fdsDataHdr_t*)pData;
        siz = sizeof(fdsDataHdr_t) + pHdr->Siz + sizeof(fdsDataFtr_t);

        if (siz % 2 != 0)
        {
            siz--;
        }

        if (pHdr->Uid < FDS_NUM_RECORDS)
        {
            if (crc.calc(pData, siz) == 0)
            {
                if (pHdr->Magic == FDS_DATAMAGIC)
                {
                    logDebug("Uid %d Data @ 0x%08lx\n", pHdr->Uid, 
                        (uint32_t)pData);
                    pRecords[pHdr->Uid] = pData;
                }
                else if(pHdr->Magic == FDS_DELMAGIC)
                {
                    logDebug("Uid %d RM @ 0x%08lx\n", pHdr->Uid, 
                        (uint32_t)pData);
                    pRecords[pHdr->Uid] = 0;
                }
                else
                {
                    logErr("Invalid Header Magic @ 0x%08lx\n", 
                        (uint32_t)pData);
                }
            }
            else
            {
                logDebug("Invalid crc @ 0x%08lx (%u, 0x%x)\n", 
                    (uint32_t)pData, siz, (uint8_t)crc);
                retval = FDS_ECRC;
                break;
            }
        }
        else if (pHdr->Raw == 0xFFFFFFFF)
        {
            logDebug("EOP @ 0x%08lx.\n", (uint32_t)pData);
            if(updateWritePointer)
            {
                pWrite = (uint16_t*)pData;
                logDebug("pWrite updated\n");
            }
            break;
        }
        else
        {
            /* Found a uid outside the valid range */
            retval = FDS_EDATA;
            break;
        }
        
        pData += siz;
    }
    
    return retval;
}

fdsStatus_t Fds::switchPage(uint16_t dataId)
{
    fdsStatus_t retval = FDS_OK;
    uint16_t page, pageId;
    
    /* Get the current page number */
    page = BSP_FLASH_ADDRTOPAGE(pWrite) - FDS_FIRSTFLASHPAGE;
    
    /* Get the page id, increment it and take care of the wrap around */
    pageId = wrapInc(getPageid(page), 1, 0xFFFF);
    
    /* Get the next page number */ 
    page = wrapInc(page, 1, FDS_NUM_PAGES);

    do
    {
        /* Check if the next page is free */
        if (getPageid(page) != 0xFFFF)
        {
            retval = FDS_ERR;
            break;
        }

        /* write a page header to the new page, this will move pWrite */
        retval = writePageHdr(page, pageId);
        if (retval != FDS_OK)
        {
            break;
        }

        /* Get the next page number, this one will be the one to erase */ 
        page = wrapInc(page, 1, FDS_NUM_PAGES);

        /* Relocate all known parameters in tis page to the new one it can be 
         * erased without losing data.
         * */
        for (uint16_t n = 0; n < FDS_NUM_RECORDS; n++)
        {
            if ((n == dataId) || (pRecords[n] == 0))
            {
                continue;
            }

            if (page == BSP_FLASH_ADDRTOPAGE(pRecords[n]))
            {
                retval = relocate(n);
                breakIfDiverse(retval, FDS_OK);
            }
        }

        /* Free the next page */
        bspFlashUnlock();
        bspFlashErasePage(BSP_FLASH_PAGETOADDR(FDS_FIRSTFLASHPAGE + page));
        bspFlashLock();
        
    } while (0);

    return retval;
}

fdsStatus_t Fds::relocate(uint16_t uid)
{
    fdsStatus_t retval = FDS_OK;
    uint16_t siz = ((fdsDataHdr_t*)pRecords[uid])->Siz;

    siz = siz % 2 != 0 ? siz - 1 : siz;
    siz += sizeof(fdsDataHdr_t) + sizeof(fdsDataFtr_t);

    do
    {   
        retval = writeToFlash(pRecords[uid], siz);
        if(retval != FDS_OK)
        {
            break;
        }
        
        pRecords[uid] = pWrite;

    } while (0);
    
    return retval;
}


fdsStatus_t Fds::writeToFlash(void * pData, size_t siz, bool checkCrc)
{
    fdsStatus_t retval = FDS_OK;
    bspStatus_t bspStatus;
    uint16_t *pStart = pWrite;
    crc8 crc;

    do
    {   
        bspFlashUnlock();
        bspStatus = bspFlashProg(pWrite, (uint16_t*)pData, siz);
        bspFlashLock();

        if (bspStatus != BSP_OK)
        {
            logErr("Error %u while writing to flash @ 0x%08lx, %u\n",
                bspStatus, (uint32_t)pWrite, siz);
            retval = FDS_EFLASH;
            break;    
        }

        pWrite += siz/2;

        if(checkCrc == false)
        {
            break;
        }

        if (crc.calc(pStart, siz) != 0)
        {
            retval = FDS_ECRC;
            break;
        }

    } while (0);
    
    return retval;
}