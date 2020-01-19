#include <pspkernel.h>
#include <pspsdk.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psppower.h>
#include <psputility_sysparam.h>

#include <stdio.h>
#include <string.h>


PSP_MODULE_INFO("1.00 Downdate", 0x1000, 1, 1);
PSP_MAIN_THREAD_ATTR(0);


#define printf	pspDebugScreenPrintf

u8 iplbuf[700*1024];
SceSize iplsize;



/*
 * It erases some of the first blocks of the nand, probably 
 * to clear the ipl.
 *
 * @returns - always zero
 *
*/
int (* sceIplUpdateClearIpl)(void);

int (* sceIplUpdateSetIpl)(void *buf, SceSize sise);


int (* flash_format)(int argc, char **argv);




/*** This function from PspPet PSARDUMPER ***/
static u32 FindProc(const char* szMod, const char* szLib, u32 nid)
{
    SceModule* modP = sceKernelFindModuleByName(szMod);
    if (modP == NULL)
    {
        //***printf("Failed to find mod '%s'\n", szMod);
        return 0;
    }
    SceLibraryEntryTable* entP = (SceLibraryEntryTable*)modP->ent_top;
    while ((u32)entP < ((u32)modP->ent_top + modP->ent_size))
    {
        if (entP->libname != NULL && strcmp(entP->libname, szLib) == 0)
        {
            // found lib
            int i;
            int count = entP->stubcount + entP->vstubcount;
            u32* nidtable = (u32*)entP->entrytable;
            for (i = 0; i < count; i++)
            {
                if (nidtable[i] == nid)
                {
                    u32 procAddr = nidtable[count+i];
                    // printf("entry found: '%s' '%s' = $%x\n", szMod, szLib, (int)procAddr);
                    return procAddr;
                }
            }
            //***printf("Found mod '%s' and lib '%s' but not nid=$%x\n", szMod, szLib, nid);
            return 0;
        }
        entP++;
    }
    //***printf("Found mod '%s' but not lib '%s'\n", szMod, szLib);
    return 0;
}

int InitEntries()
{

	sceIplUpdateClearIpl = (void *)FindProc("IplUpdater", "sceIplUpdate_driver", 0x26093B04);

	sceIplUpdateSetIpl = (void *)FindProc("IplUpdater", "sceIplUpdate_driver", 0xEE7EB563);

	flash_format = (void *)FindProc("sceLflashFatfmt", "LflashFatfmt", 0xB7A424A4);

	if (!sceIplUpdateClearIpl || !sceIplUpdateSetIpl || !flash_format)
	{
		return -1;
	}

	return 0;
}

void delayExit(int ms)
{
	sceKernelDelayThread(ms*1000);
	sceKernelExitGame();
}

void restart(int ms)
{
	sceKernelDelayThread(ms*1000);
	
	// The hard reset is not working properly
	// Note: except if you call working what it does that can destroy 
	// your heart for a minute :)
	//sceSysconResetDevice(1, 1); 	

	// 
	
	while (1)
	{
		sceKernelDelayThread(1000000);
	}
}

char inputlist[12*1024], outputlist[12*1024];

int readfilelist()
{
	SceUID inp = sceIoOpen("ms0:/DOWNDATER/inputfl.bin", PSP_O_RDONLY, 0777);
	SceUID outp = sceIoOpen("ms0:/DOWNDATER/outputfl.bin", PSP_O_RDONLY, 0777);

	if (inp < 0 || outp < 0)
		return -1;

	if (sceIoRead(inp, inputlist, 12*1024) <= 0)
	{
		sceIoClose(inp);
		return -1;
	}

	if (sceIoRead(outp, outputlist, 12*1024) <= 0)
	{
		sceIoClose(outp);
		return -1;
	}

	sceIoClose(inp);
	sceIoClose(outp);

	return 0;
}

/* check if the input files can be read */
int checkreadable()
{
	char *p = inputlist;

	while (*p != 0)
	{
		SceUID fd = sceIoOpen(p, PSP_O_RDONLY, 0777);

		if (fd < 0)
			return -1;

		sceIoClose(fd);

		p += strlen(p)+1;
	}

	return 0;
}


void ipl_downdate()
{
	sceIplUpdateClearIpl();
	sceIplUpdateSetIpl(iplbuf, iplsize);
}


char buffer[8192];

int filecount=0;

void flash_file(char *src, char *dst)
{

    SceUID infd = sceIoOpen(src, PSP_O_RDONLY, 0777);
	 
	int bytesread, totalwritten = 0;

	printf("Flashing file %s to %s\n", src, dst);
		         
    sceIoRemove(dst);
	SceUID outfd = sceIoOpen(dst, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	     
	if (infd < 0 || outfd < 0)
	{
		// !!!!!!!!!!!!!!!!!!
		// Better not to be here ever...
		//printf("Error writing file.\n");
		return;
	}

	while ((bytesread = sceIoRead(infd, buffer, 8192)) > 0)
	{
		totalwritten += sceIoWrite(outfd, buffer, bytesread);
	}

	sceIoClose(infd);
	sceIoClose(outfd);
}


void createDirs()
{
	sceIoMkdir("flash0:/data", 0777);
    sceIoMkdir("flash0:/data/cert", 0777);
    sceIoMkdir("flash0:/dic", 0777);
    sceIoMkdir("flash0:/font", 0777);
    sceIoMkdir("flash0:/kd", 0777);
    sceIoMkdir("flash0:/kd/resource", 0777);
    sceIoMkdir("flash0:/vsh", 0777);
    sceIoMkdir("flash0:/vsh/etc", 0777);
    sceIoMkdir("flash0:/vsh/module", 0777);
    sceIoMkdir("flash0:/vsh/resource", 0777);
}

void write_flash()
{
	char *s = inputlist;
	char *t = outputlist;

	createDirs();

	while (*s != 0)
	{
		if (t[5] == '8')
			t[5] = '0'; // Just to make it compatible with the new format
				
		flash_file(s, t);

		s += strlen(s)+1;
		t += strlen(t)+1;
	}
}


void downdate()
{
	// Lets the party begin :)
	int  Secu = 0; 	

	//Fixe Yoshihiro Security  
    if(sceIoUnassign("flash0:") < 0)
    {
		printf("Error in unassign flash0.\n");
		return;
    } 

	if (sceIoUnassign("flash1:") < 0)
	{
		printf("Error in unassign flash1.\n");
		return;
	}

	char *argv[2];

	argv[0] = "fatfmt";
	argv[1] = "lflash0:0,0";
	
	printf("Formating flash0.");
	flash_format(2, argv);
    
    
	if(sceIoAssign("flash0:", "lflash0:0,0", "flashfat0:", 0, IOASSIGN_RDWR , 0) < 0)
    {
		printf("Error in assign flash0.\n");
		return;
    }
	else
	{
        printf("flash0 assigned in write mode.\n");
    }  
	
	if(sceIoAssign("flash1:", "lflash0:0,1", "flashfat1:", 0, IOASSIGN_RDWR , 0) < 0)
    {
		printf("Error in assign flash1.\n");
		return;
    }
	else
	{
        printf("flash1 assigned in write mode.\n");
    }     
        
    //Yoshihiro ^-^    
	if(Secu == 0)
	{		
		// First critical moment. The writing of the files.
		printf("Starting the write of files.\n");
		write_flash();

		// Second and last critical moment. The writing of the ipl.
		printf("Downdating IPL...\n");
		ipl_downdate();
	
	}
	else
	{
		delayExit(4000);
	}
	
    // Arrived here. We should have 1.00 or a brick
}

void checklanguage()
{
	int lang;

	if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &lang) == PSP_SYSTEMPARAM_RETVAL_FAIL)
	{
		printf("Error checking language.\n");
		delayExit(4000);
	}

	if (lang !=  PSP_SYSTEMPARAM_LANGUAGE_JAPANESE && lang !=  PSP_SYSTEMPARAM_LANGUAGE_ENGLISH)
	{
		printf("The downdater requires that you set the english " 
				"or japanese language before you start it.\n\n\n");

		printf("El downdater requiere que establezcas el idioma ingles "
			    "o japones antes de ejecutarlo.\n\n\n");

		printf("Le downdater a besoin d'etre mis en anglais "
				"ou bien en japonnais avans de commencer.\n\n\n");


		delayExit(9000);
	}
}

void waitAC()
{
	if (!scePowerIsPowerOnline())
	{
		printf("Please, connect the psp to the AC adaptor.\n\n\n");
		printf("Por favor, conecta la psp a la corriente.\n\n\n");
		printf("S'il vous plais connectez le chargeur de la psp.\n\n\n");

		while (!scePowerIsPowerOnline())
		{
			sceKernelDelayThread(50000);
		}
	}
}


#define IPL_UPDATE		"ms0:/DOWNDATER/PRX/ipl_update_100.prx"
#define LFLASH_FORMAT	"ms0:/DOWNDATER/PRX/lflash_fatfmt.prx"

int main()
{	
	
	pspSdkInstallNoDeviceCheckPatch();
	pspSdkInstallNoPlainModuleCheckPatch();

	pspDebugScreenInit();
	pspDebugScreenSetTextColor(0x000000FF);

	printf("1.50 to 1.00 Downdater by Dark_AleX/Mathieulh/Yoshihiro\n\n");

	//checklanguage();
	//waitAC();

	SceUID fd = sceIoOpen("ms0:/DOWNDATER/ipl.bin", PSP_O_RDONLY, 0777);
	if (fd < 0)
	{
		printf("Cannot open ipl.bin.\n");
		delayExit(4000);
	}

	iplsize = sceIoRead(fd, iplbuf, 700*1024);

	if (iplsize <= 0)
	{
		printf("Error while reading ipl.bin.\n");
		delayExit(4000);
	}

	SceUID mod = sceKernelLoadModule(IPL_UPDATE, 0, NULL);

	if (mod < 0)
	{
		printf("Error 0x%08X loading ipl_update\n", mod);		
		delayExit(4000);
	}

	mod = sceKernelStartModule(mod, strlen(IPL_UPDATE)+1, IPL_UPDATE, NULL, NULL);

	if (mod < 0)
	{
		printf("Error 0x%08X starting ipl_update\n", mod);
		delayExit(4000); 
	}

	mod = sceKernelLoadModule(LFLASH_FORMAT, 0, NULL);

	if (mod < 0)
	{
		printf("Error 0x%08X loading lflash_fmt\n", mod);		
		delayExit(4000);
	}

	mod = sceKernelStartModule(mod, strlen(LFLASH_FORMAT)+1, LFLASH_FORMAT, NULL, NULL);

	if (mod < 0)
	{
		printf("Error 0x%08X starting lflash_fmt\n", mod);
		delayExit(4000); 
	}
	

	if (InitEntries() < 0)
	{
		printf("Failed to init entries.\n");
		delayExit(4000);
	}

	if (readfilelist() < 0)
	{
		printf("Failed to read file lists.\n");
		delayExit(4000);
	}

	if (checkreadable() < 0)
	{
		printf("Some input file was not readable");
		delayExit(4000);
	}

	printf("The downgrade is ready to be started."
		   "Press X to start or O to exit.\n"
		   "Don't remove the memstick and don't shut down the psp until finished.\n"
		   "Remember that you are  doing this at your risk!\n");

	SceCtrlData pad;

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(1);

	while (1)
	{
		sceCtrlReadBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_CROSS)
			break;
		else if (pad.Buttons & PSP_CTRL_CIRCLE)
			delayExit(0);
		sceKernelDelayThread(50000);
	}

	pspDebugScreenClear();


	// Yeahhhhhhhhh
	downdate();

	printf("\n\n\nThe downdate has finished.\nRestart your psp manually by holding the power button\n");
	
	restart(2500);

	return 0;
}


