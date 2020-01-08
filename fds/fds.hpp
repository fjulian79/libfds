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

#ifndef FDS_HPP_
#define FDS_HPP_

#include "fds_config.h"
#include <bsp/bsp_flash.h>
#include <stdint.h>

/**
 * @brief Defnition of return codes used by libFds
 */
typedef enum
{
    /*  0 */ FDS_OK = 0,        ///<! In case of success.
    /*  1 */ FDS_ERR,           ///<! In case of a non specified error.
    /*  2 */ FDS_EREADY,        ///<! If the lib has non been intilized yet.
    /*  3 */ FDS_ESIZE,         ///<! If the data does not fit into the flash.
    /*  4 */ FDS_EEINVAL,       ///<! in case of a invalid argument. 
    /*  5 */ FDS_EFLASH,        ///<! In case of a FLASH related error.
    /*  6 */ FDS_ECRC,          ///<! In case of a invlaid checksum.
    /*  7 */ FDS_EDATA,         ///<! In case of invalid data.    

}fdsStatus_t;

/**
 * @brief A class used to manage the a fraction of the on chip flash as data 
 * storage. It shall not be as mighty as a full blown file system as there are 
 * allready several of them out there. But it sahll also be more powerfull then
 * a simple EEPROM Emulation. Records are scifoed by a number, their size is not
 * requiered to be constant. It shall also be possible to delete records. 
 */
class Fds
{
    public:

        /**
         * @brief To get a instance of the flash data storage class.
         * 
         * This class is designed as a singleton as there is only one on chip
         * flash. Beside of that it shall be possible to use this class in
         * multiple libries within a project. Therefore it makes sense to 
         * initialize the libray only once. 
         * 
         * @return Fds* The instance.
         */
        static Fds* getInsance(void);

        /**
         * @brief Intializes the library.
         * 
         * Has to be called once before it can be used. It reads the flash and 
         * tries to format the falsh once in case of any error.
         * 
         * @param doReset Default is true, set this to false If you do not 
         *        want this function to format the falsh in case of a error.
         * 
         * @return FDS_OK       In case of sucess, also when a error has been 
         *                      solved by formatting the flash. 
         *         FDS_ERR      In case of invalid page numbering.
         *         FDS_EFLASH   In case of a flash related error.
         *         FDS_ECRC     in case of a invalid CRC.
         *         FDS_EDATA    in case of invalid data id's in the falsh.
         */
        fdsStatus_t init(bool doReset = true);

        /**
         * @brief Used to print some status infos.
         * 
         * @return FDS_ERR      In case of invalid page numbering.
         *         FDS_EFLASH   In case of a flash related error.
         *         FDS_ECRC     in case of a invalid CRC.
         *         FDS_EDATA    in case of invalid data id's in the falsh.
         */
        fdsStatus_t info(void);

        /**
         * @brief Used to write data to the flash
         * 
         * @param uid The UID of the provided parameters. Must be in the range 
         *        of 0 - (FDS_NUM_RECORDS-1)
         * 
         * @param pData Pointer to the user's data.
         * 
         * @param siz Size of the user's data in bytes. 
         * 
         * @return FDS_OK       in case of success.
         *         FDS_ESIZE    If the numBytes is of bytes is out of range
         *         FDS_EINVAL   If the uid is out of range.
         *         FDS_ERR      In case of invalid page numbering.
         *         FDS_EFLASH   In case of a flash related error.
         *         FDS_ECRC     in case of a invalid CRC.
         *         FDS_EDATA    in case of invalid data id's in the falsh.
         * 
         */
        fdsStatus_t write(uint8_t uid, void* pData, size_t numBytes);

        /**
         * @brief TBD
         * 
         * @param uid   The UID of the provided parameters. 
         *              Must be in the range of 0 - (FDS_NUM_RECORDS-1)
         * @param pData Pointer to some memory to read to.
         * @param siz   Size of the proided memeory.
         * @return      Number of bytes read from the Flash.
         *              Might be zero if the requested UID is not present in 
         *              the flash yet. Will also be zero in case of any other
         *              error.
         */
        size_t read(uint8_t uid, void* pData, size_t siz);

        /**
         * @brief Used to delete a record from the falsh.
         * 
         * This function sets the data pointer for the given id to zero and 
         * stores a marker in the flash which will cause the same effect when
         * initializing this libary next time. While time goes on and further 
         * records will be written to flash, the pages with the data for the 
         * specified record will be recycled but the data will no be preserved. 
         * So right after this call the data is tell there.
         * 
         * @param uid The id of the record to remove from the flash.
         * 
         * @return FDS_OK       In case of success.
         *         FDS_EEINVAL  If the uid is out of range.
         *         FDS_ERR      In case of invalid page numbering.
         *         FDS_EFLASH   In case of a flash related error.
         *         FDS_ECRC     in case of a invalid CRC.
         *         FDS_EDATA    in case of invalid data id's in the falsh.
         */
        fdsStatus_t del(uint8_t uid);
        
        /**
         * @brief Use to reset the flash to a known state.
         * 
         * @return FDS_OK       In case of success.
         *         FDS_EFLASH   In case of a flash related error.
         *         FDS_ECRC     In case of a invalid CRC. 
         */
        fdsStatus_t format(void);

    private:

        /**
         * @brief Defines the page header used in each and every FDS falsh page.
         */
        typedef union
        {
            struct __attribute__((__packed__))
            {
                uint8_t Magic;      ///<! Consant magic.
                uint16_t Id;        ///<! Id of the particular page
                uint8_t Crc;        ///<! CRC of the page header.
            };

            uint32_t Raw;
        }
        fdsPageHdr_t;

        /**
         * @brief Defines the data header used for each end every data record 
         * in the flash.
         */
        typedef union 
        {
            struct __attribute__((__packed__))
            {
                uint8_t Magic;      ///<! Constant magic.
                uint8_t Uid;        ///<! The id of the record.
                uint16_t Siz;       ///<! The size of the data in bytes.
            };

            uint32_t Raw;           ///<! uint32_t raw value for easy access.

        }fdsDataHdr_t;

        /**
         * @brief Defines the footer of a single data record.
         * 
         * As a crc8 is used, it is needed that the crc byte is realy the last 
         * byte of the record. Therefore a additional byte has to be written 
         * between the end of the data and the CRC. if the number of user data
         * bytes is even its a padding byte and should be set to zero. If the 
         * number of user data bytes is uneven the last user data byte shall be 
         * writen to it. This allows to write allways 16Bit words to the data 
         * falsh. The struct in the union defines the possion of the Data byte
         * correctly and no further actions are needed when wiring the uint16_t
         * raw value to the flash. 
         * */
        typedef union 
        {
            struct __attribute__((__packed__))
            {
                uint8_t Data;       ///<! Data/Padding byte
                uint8_t Crc;        ///<! The CRC of the data record.
            };

            uint16_t Raw;           ///<! uint16_t raw value for easy access.

        }fdsDataFtr_t;

        /**
         * @brief Construct a new Fds object
         */
        Fds();

        /**
         * @brief Destroy the Fds object
         */
        ~Fds();

        /**
         * @brief Used to get the page number stored in the page header of the
         *        given page.
         * 
         * @param page the page number.
         * 
         * @return The page number stored in the page header of the given page.
         *         0xFFFF       in case of a invalid page header crc. 
         */
        uint16_t getPageid(uint16_t page);

        /**
         * @brief Used to write the page header to the given page number.
         * 
         * @param page The Fds flash page number to write to realtive to the 
         *        first Fds page number.
         * 
         * @param uid The page number to use.
         * 
         * @return FDS_OK       In case of success.
         *         FDS_EFLASH   In case of a flash related error.
         *         FDS_ECRC     In case of a invalid CRC.
         */
        fdsStatus_t writePageHdr(uint16_t page, uint16_t uid);

        /**
         * @brief Used the read the provided page.
         * 
         * @param page The Fds flash page number to read realtive to the first 
         *        Fds page number.
         * 
         * @param updateWritePointer if set to true the internal write pointer 
         *        will be updated. This is only needed ontil the most recent 
         *        page has not been found.
         * 
         * @return FDS_OK       In case of success.
         *         FDS_ECRC     In case of a invalid CRC.
         *         FDS_EDATA    In case of invalid data in the falsh.
         */
        fdsStatus_t readPage(uint16_t page, bool updateWritePointer);

        /**
         * @brief Used to move the write pointer to FDS flash page (n+1)
         * 
         * This function will recycle the FDS falsh page (n+2) by moving it's 
         * data records to fds falsh page (n+1) and erasing it. The data records
         * with the given id will be dropped. 
         * 
         * @param uid The uid to drop.
         * 
         * @return FDS_OK       In case of success.
         *         FDS_ERR      If flash page (n+1) is not free as expected.
         *         FDS_EFLASH   In case of a flash related error.
         *         FDS_ECRC     In case of a invalid CRC.         
         */
        fdsStatus_t switchPage(uint16_t uid);

        /**
         * @brief Used to rewrite the data record with the given id at the 
         *        current write position.
         * 
         * @param dataId The data record to rewrite.
         * 
         * @return FDS_EFLASH   In case of a flash related error.
         *         FDS_ECRC     In case of a invalid CRC.
         */
        fdsStatus_t relocate(uint16_t dataId);

        /**
         * @brief Internal write function which takes care of moving the write
         *        poniter and checks crc is needed.
         * 
         * @param pData Data to write.
         * @param siz Size of the data in bytes.
         * @param checkCrc Defines if a CRC check shall be performed.
         *        Defauls to true.
         * 
         * @return FDS_OK       In case of success.
         *         FDS_EFLASH   In case of a flash related error.
         *         FDS_ECRC     In case of a invalid CRC.
         */
        fdsStatus_t writeToFlash(void * pData, size_t siz, bool checkCrc=true);

        /**
         * @brief Pomzer of the singelton instance.
         */
        static Fds *pInstance;

        /**
         * @brief To indicate if the initialization has ben done.
         */
        bool InitDone;

        /**
         * @brief The array of user data records.
         */
        void* pRecords[FDS_NUM_RECORDS];

        /**
         * @brief The current write pointer in the flash.
         */
        uint16_t *pWrite;
};

#endif /* FDS_HPP_  */
