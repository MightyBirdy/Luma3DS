/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2018 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include <3ds.h>
#include <3ds/os.h>
#include "menus.h"
#include "menu.h"
#include "draw.h"
#include "menus/process_list.h"
#include "menus/process_patches.h"
#include "menus/n3ds.h"
#include "menus/debugger.h"
#include "menus/miscellaneous.h"
#include "menus/sysconfig.h"
#include "menus/screen_filters.h"
#include "ifile.h"
#include "memory.h"
#include "fmt.h"
#include "pmExtension.h"
#include "pmExtension_O3DS.h"

void    RosalinaMenu_EnablePluginLoader(void);

Menu rosalinaMenu = {
    "Rosalina menu",
    .nbItems = 12,
    {
        { "Process list", METHOD, .method = &RosalinaMenu_ProcessList },
        { "Take screenshot (slow!)", METHOD, .method = &RosalinaMenu_TakeScreenshot },
        { "Enable plugin loader", METHOD, .method = &RosalinaMenu_EnablePluginLoader },
        { "New 3DS menu...", MENU, .menu = &N3DSMenu },
        { "Cheats...", METHOD, .method = &RosalinaMenu_Cheats },
        { "Debugger options...", MENU, .menu = &debuggerMenu },
        { "System configuration...", MENU, .menu = &sysconfigMenu },
        { "Screen filters...", MENU, .menu = &screenFiltersMenu },
        { "Miscellaneous options...", MENU, .menu = &miscellaneousMenu },
        { "Power off", METHOD, .method = &RosalinaMenu_PowerOff },
        { "Reboot", METHOD, .method = &RosalinaMenu_Reboot },
        { "Credits", METHOD, .method = &RosalinaMenu_ShowCredits }
    }
};

void RosalinaMenu_ShowCredits(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina -- Luma3DS credits");

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Luma3DS (c) 2016-2018 AuroraWright, TuxSH") + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "3DSX loading code by fincs");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Networking code & basic GDB functionality by Stary");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "InputRedirection by Stary (PoC by ShinyQuagsire)");

        posY += 2 * SPACING_Y;

        Draw_DrawString(10, posY, COLOR_WHITE,
            (
                "Special thanks to:\n"
                "  Bond697, WinterMute, yifanlu,\n"
                "  Luma3DS contributors, ctrulib contributors,\n"
                "  other people"
            ));

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void RosalinaMenu_Reboot(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to reboot, press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            APT_HardwareResetAsync();
            menuLeave();
        } else if(pressed & BUTTON_B)
            return;
    }
    while(!terminationRequest);
}

void RosalinaMenu_PowerOff(void) // Soft shutdown.
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to power off, press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            menuLeave();
            srvPublishToSubscriber(0x203, 0);
        }
        else if(pressed & BUTTON_B)
            return;
    }
    while(!terminationRequest);
}

extern u8 framebufferCache[FB_BOTTOM_SIZE];
void RosalinaMenu_TakeScreenshot(void)
{
#define TRY(expr) if(R_FAILED(res = (expr))) goto end;

    u64 total;
    IFile file;
    Result res;

    char filename[64];

    FS_Archive archive;
    FS_ArchiveID archiveId;
    s64 out;
    bool isSdMode;

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    Draw_Lock();
    Draw_RestoreFramebuffer();

    svcFlushEntireDataCache();

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if(R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/screenshots"), 0);
        if((u32)res == 0xC82044BE) // directory already exists
            res = 0;
        FSUSER_CloseArchive(archive);
    }

    u32 seconds, minutes, hours, days, year, month;
    u64 milliseconds = osGetTime();
    seconds = milliseconds/1000;
    milliseconds %= 1000;
    minutes = seconds / 60;
    seconds %= 60;
    hours = minutes / 60;
    minutes %= 60;
    days = hours / 24;
    hours %= 24;

    year = 1900; // osGetTime starts in 1900

    while(true)
    {
        bool leapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        u16 daysInYear = leapYear ? 366 : 365;
        if(days >= daysInYear)
        {
            days -= daysInYear;
            ++year;
        }
        else
        {
            static const u8 daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            for(month = 0; month < 12; ++month)
            {
                u8 dim = daysInMonth[month];

                if (month == 1 && leapYear)
                    ++dim;

                if (days >= dim)
                    days -= dim;
                else
                    break;
            }
            break;
        }
    }
    days++;
    month++;

    sprintf(filename, "/luma/screenshots/%04u-%02u-%02u_%02u-%02u-%02u.%03u_top.bmp", year, month, days, hours, minutes, seconds, milliseconds);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    Draw_CreateBitmapHeader(framebufferCache, 400, 240);

    for(u32 y = 0; y < 120; y++)
        Draw_ConvertFrameBufferLine(framebufferCache + 54 + 3 * 400 * y, true, true, y);

    TRY(IFile_Write(&file, &total, framebufferCache, 54 + 3 * 400 * 120, 0));

    for(u32 y = 120; y < 240; y++)
        Draw_ConvertFrameBufferLine(framebufferCache + 3 * 400 * (y - 120), true, true, y);

    TRY(IFile_Write(&file, &total, framebufferCache, 3 * 400 * 120, 0));
    TRY(IFile_Close(&file));

    sprintf(filename, "/luma/screenshots/%04u-%02u-%02u_%02u-%02u-%02u.%03u_bot.bmp", year, month, days, hours, minutes, seconds, milliseconds);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    Draw_CreateBitmapHeader(framebufferCache, 320, 240);

    for(u32 y = 0; y < 120; y++)
        Draw_ConvertFrameBufferLine(framebufferCache + 54 + 3 * 320 * y, false, true, y);

    TRY(IFile_Write(&file, &total, framebufferCache, 54 + 3 * 320 * 120, 0));

    for(u32 y = 120; y < 240; y++)
        Draw_ConvertFrameBufferLine(framebufferCache + 3 * 320 * (y - 120), false, true, y);

    TRY(IFile_Write(&file, &total, framebufferCache, 3 * 320 * 120, 0));
    TRY(IFile_Close(&file));

    if((GPU_FB_TOP_FMT & 0x20) && (Draw_GetCurrentFramebufferAddress(true, true) != Draw_GetCurrentFramebufferAddress(true, false)))
    {
        sprintf(filename, "/luma/screenshots/%04u-%02u-%02u_%02u-%02u-%02u.%03u_top_right.bmp", year, month, days, hours, minutes, seconds, milliseconds);
        TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
        Draw_CreateBitmapHeader(framebufferCache, 400, 240);

        for(u32 y = 0; y < 120; y++)
            Draw_ConvertFrameBufferLine(framebufferCache + 54 + 3 * 400 * y, true, false, y);

        TRY(IFile_Write(&file, &total, framebufferCache, 54 + 3 * 400 * 120, 0));

        for(u32 y = 120; y < 240; y++)
            Draw_ConvertFrameBufferLine(framebufferCache + 3 * 400 * (y - 120), true, false, y);

        TRY(IFile_Write(&file, &total, framebufferCache, 3 * 400 * 120, 0));
        TRY(IFile_Close(&file));
    }

end:
    IFile_Close(&file);
    svcFlushEntireDataCache();
    Draw_SetupFramebuffer();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Screenshot");
        if(R_FAILED(res))
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08x).", (u32)res);
        else
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);

#undef TRY
}

// MMU hax code from NTR: todo rwx on the LCD in k11Extension (or a svcFlash ?)
static u32 *translateAddress(u32 addr)
{
    if (addr < 0x1ff00000)
    {
        return (u32 *)(addr - 0x1f3f8000 + 0xfffdc000);
    }
    return (u32*)(addr - 0x1ff00000 + 0xdff00000);

}

void set_kmmu_rw(int cpu, u32 addr, u32 size)
{
    int i, j;
    u32 mmu_p;
    u32 p1, p2;
    u32 v1, v2;
    u32 end;

    if (cpu == 0){
        mmu_p = 0x1fff8000;
    }
    if (cpu == 1) {
        mmu_p = 0x1fffc000;
    }
    if (cpu == 2) {
        mmu_p = 0x1f3f8000;
    }

    end = addr + size;

    v1 = 0x20000000;
    
    for (i = 512; i<4096; i++)
    {
        p1 = *translateAddress(mmu_p + i * 4);
        if ((p1 & 3) == 2)
        {
            if (v1 >= addr && v1<end)
            {
                p1 &= 0xffff73ff;
                p1 |= 0x00000c00;
                *translateAddress(mmu_p + i * 4) = p1;
            }
        }
        else if ((p1 & 3) == 1){
            p1 &= 0xfffffc00;
            for (j = 0; j<256; j++){
                v2 = v1 + j * 0x1000;
                if ((v2 >= addr) && (v2<end)){
                    p2 = *translateAddress(p1 + j * 4);
                    if ((p2 & 3) == 1){
                        p2 &= 0xffff7dcf;
                        p2 |= 0x00000030;
                        *translateAddress(p1 + j * 4) = p2;
                    }
                    else if ((p2 & 3)>1){
                        p2 &= 0xfffffdce;
                        p2 |= 0x00000030;
                        *translateAddress(p1 + j * 4) = p2;
                    }
                }
            }
            
        }
        v1 += 0x00100000;
    }
}

extern bool isN3DS;

s32 K_DoKernelHax() ///< This is hardcoded for N3DS for the test !
{
    __asm__ __volatile__("cpsid aif");

    u32 kmmuAddr = isN3DS ? 0xFFFBA000 : 0xFFFBE000;
    u32 kmmusize = 0x10000;
    // set mmu    
    for (int i = 0; i < 2 + isN3DS; i++)
        set_kmmu_rw(i, kmmuAddr, kmmusize);

    if (isN3DS)
    {
        *(u32*)0xDFF8862C = 0;
        *(u32*)0xDFF88630 = 0;
    }
    else
    {
        *(u32*)0xDFF88514 = 0;
        *(u32*)0xDFF88518 = 0;
    }

    return (0);
}

void flash(u32 cl) ///< Hardcoded N3DS !!!
{
    u32 i;

    if (isN3DS)
    {
        for ( i = 0; i < 64; i++){
            *(vu32*)(0xFFFC4000+ 0x204) = cl;
            svcSleepThread(5000000);
        }
        *(vu32*)(0xFFFC4000 + 0x204) = 0;
    }
    else
    {
        for ( i = 0; i < 64; i++){
            *(vu32*)(0xfffc8000+ 0x204) = cl;
            svcSleepThread(5000000);
        }
        *(vu32*)(0xfffc8000 + 0x204) = 0;
    }
}

#define PM_PAYLOAD (isN3DS ? pmExtension : pmExtension_O3DS)
#define PM_PAYLOAD_SIZE (isN3DS ? size_pmExtension : size_pmExtension_O3DS)
#define PM_INJECT_ADDR (isN3DS ? 0x10BA00 : 0x10AC00)
#define PM_INJECT_SIZE 0x2000
#define PM_SVCRUN_ADDR (isN3DS ? 0x00103150 : 0x00103154)

bool    RosalinaMenu_InstallPm(void)
{
    static bool isInstalled = 0;

    if (isInstalled)
        return isInstalled;

    svcBackdoor(K_DoKernelHax);
    svcFlushEntireDataCache();
    svcInvalidateEntireInstructionCache();

    u32   *dst = (u32 *)PM_INJECT_ADDR;
    u32   *payload = (u32 *)PM_PAYLOAD;

    for (u32 i = 0; i < PM_PAYLOAD_SIZE / 4; i++)
        dst[i] = payload[i];

    isInstalled = 1;
    return false;
}

void    RosalinaMenu_EnablePluginLoader(void)
{
    static bool isEnabled = false;
    static u32  pmBack[2] = {0};

    s64 startAddress, textTotalRoundedSize, rodataTotalRoundedSize, dataTotalRoundedSize;
    u32 totalSize;
    Handle processHandle;
    Result res;

    if (osGetKernelVersion() < SYSTEM_VERSION(2, 54, 0))
        return;

    res = OpenProcessByName("pm", &processHandle);

    if (R_SUCCEEDED(res))
    {
        svcControlProcessMemory(processHandle, PM_INJECT_ADDR & ~0xFFF, PM_INJECT_ADDR & ~0xFFF, PM_INJECT_SIZE, 6, 7);
        svcGetProcessInfo(&textTotalRoundedSize, processHandle, 0x10002); // only patch .text + .data
        svcGetProcessInfo(&rodataTotalRoundedSize, processHandle, 0x10003);
        svcGetProcessInfo(&dataTotalRoundedSize, processHandle, 0x10004);

        totalSize = (u32)(textTotalRoundedSize + rodataTotalRoundedSize + dataTotalRoundedSize);

        svcGetProcessInfo(&startAddress, processHandle, 0x10005);
        res = svcMapProcessMemoryEx(processHandle, 0x00100000, (u32) startAddress, totalSize);

        u32   *code = (u32 *)PM_SVCRUN_ADDR; ///< addr >= 11.0, TODO: search for pattern

        if (R_SUCCEEDED(res))
        {
            if (isEnabled)
            {
                code[0] = pmBack[0];
                code[1] = pmBack[1];                
                rosalinaMenu.items[2].title = "Enable plugin loader";
                isEnabled = false;
            }
            else
            {
                bool doFlash = RosalinaMenu_InstallPm();
                
                // Install hook
                pmBack[0] = code[0];
                pmBack[1] = code[1];
                code[0] = 0xE51FF004; // ldr pc, [pc, #-4]
                code[1] = PM_INJECT_ADDR; // Payload
                rosalinaMenu.items[2].title = "Disable plugin loader";
                isEnabled = true;
                if (doFlash)
                    flash(0x100FF00);
            }
        } 

        svcUnmapProcessMemoryEx(processHandle, 0x00100000, totalSize);        
    }
    svcCloseHandle(processHandle);
}
